#include "oblivious_sort.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>

// ----- UntrustedMemory Methods -----
// Removed #ifndef SGX_ENCLAVE so these are always compiled.
std::vector<Element> UntrustedMemory::read_bucket(int level, int bucket_index) {
    std::pair<int, int> key = {level, bucket_index};
    std::ostringstream oss;
    oss << "Read bucket at level " << level << ", index " << bucket_index << ": ";
    if (storage.find(key) != storage.end()) {
        for (const auto &elem : storage[key]) {
            if (elem.is_dummy)
                oss << "dummy ";
            else
                oss << elem.value << " ";
        }
    }
    access_log.push_back(oss.str());
    return storage[key];
}

void UntrustedMemory::write_bucket(int level, int bucket_index, const std::vector<Element>& bucket) {
    std::pair<int, int> key = {level, bucket_index};
    storage[key] = bucket;
    std::ostringstream oss;
    oss << "Write bucket at level " << level << ", index " << bucket_index << ": ";
    for (const auto &elem : bucket) {
        if (elem.is_dummy)
            oss << "dummy ";
        else
            oss << elem.value << " ";
    }
    access_log.push_back(oss.str());
}

std::vector<std::string> UntrustedMemory::get_access_log() {
    return access_log;
}

// ----- Enclave Methods -----
#ifdef SGX_ENCLAVE
// OCALL wrapper implementations for enclave mode.
std::vector<Element> Enclave::ocall_read_bucket_wrapper(int level, int bucket_index, int bucket_size) {
    std::vector<Element> bucket(bucket_size);
    ocall_read_bucket(level, bucket_index, bucket.data(), bucket_size);
    return bucket;
}

void Enclave::ocall_write_bucket_wrapper(int level, int bucket_index, const std::vector<Element>& bucket) {
    ocall_write_bucket(level, bucket_index, bucket.data(), bucket.size());
}
#endif

std::vector<Element> Enclave::encryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> encrypted = bucket;
    for (auto &elem : encrypted) {
        if (!elem.is_dummy) {
            elem.value ^= encryption_key;
            elem.key   ^= encryption_key;
        }
    }
    return encrypted;
}

std::vector<Element> Enclave::decryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> decrypted = bucket;
    for (auto &elem : decrypted) {
        if (!elem.is_dummy) {
            elem.value ^= encryption_key;
            elem.key   ^= encryption_key;
        }
    }
    return decrypted;
}

std::pair<int, int> Enclave::computeBucketParameters(int n, int Z) {
    int B_required = static_cast<int>(std::ceil((2.0 * n) / Z));
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

void Enclave::initializeBuckets(const std::vector<int>& input_array, int B, int Z) {
#ifdef SGX_ENCLAVE
    ocall_log("initializeBuckets: Starting bucket initialization.");
#endif
    int n = input_array.size();
    std::vector<Element> elements;
    std::uniform_int_distribution<int> key_dist(0, B - 1);
    for (int x : input_array) {
        int random_key = key_dist(rng);
        elements.push_back(Element{x, random_key, false});
    }
    int group_size = (n + B - 1) / B;
    std::vector<std::vector<Element>> groups(B);
    for (int i = 0; i < B; i++) {
        int start = i * group_size;
        int end = std::min(start + group_size, n);
        if (start < n)
            groups[i] = std::vector<Element>(elements.begin() + start, elements.begin() + end);
        else
            groups[i] = std::vector<Element>(); // Empty group.
    }
    for (int i = 0; i < B; i++) {
        std::vector<Element> bucket = groups[i];
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{0, 0, true});
#ifdef SGX_ENCLAVE
        ocall_write_bucket_wrapper(0, i, encryptBucket(bucket));
#else
        untrusted->write_bucket(0, i, encryptBucket(bucket));
#endif
    }
#ifdef SGX_ENCLAVE
    ocall_log("initializeBuckets: Finished bucket initialization.");
#endif
}

void Enclave::bitonicMerge(std::vector<Element>& a, int low, int cnt, bool ascending) {
    if (cnt > 1) {
        int k = cnt / 2;
        for (int i = low; i < low + k; i++) {
            if ((ascending && a[i].key > a[i + k].key) ||
                (!ascending && a[i].key < a[i + k].key)) {
                std::swap(a[i], a[i + k]);
            }
        }
        bitonicMerge(a, low, k, ascending);
        bitonicMerge(a, low + k, k, ascending);
    }
}

void Enclave::bitonicSort(std::vector<Element>& a, int low, int cnt, bool ascending) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonicSort(a, low, k, true);
        bitonicSort(a, low + k, k, false);
        bitonicMerge(a, low, cnt, ascending);
    }
}

std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split_bitonic(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {

#ifdef SGX_ENCLAVE
    ocall_log(("merge_split_bitonic: Merging buckets at level " + std::to_string(level)).c_str());
#endif

    int L = total_levels;
    int bit_index = L - 1 - level;
    
    std::vector<Element> combined = bucket1;
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());

    int count0 = 0, count1 = 0;
    for (const auto &elem : combined) {
        if (!elem.is_dummy) {
            if (((elem.key >> bit_index) & 1) == 0)
                count0++;
            else
                count1++;
        }
    }
    if (count0 > Z || count1 > Z)
        throw std::overflow_error("Bucket overflow occurred in merge_split.");

    int needed_dummies0 = Z - count0;
    int needed_dummies1 = Z - count1;
    int assigned_dummies0 = 0, assigned_dummies1 = 0;

    for (auto &elem : combined) {
        if (elem.is_dummy) {
            if (assigned_dummies0 < needed_dummies0) {
                elem.key = 1;
                assigned_dummies0++;
            } else {
                elem.key = 3;
                assigned_dummies1++;
            }
        } else {
            int bit_val = (elem.key >> bit_index) & 1;
            elem.key = (bit_val << 1);
        }
    }

    bitonicSort(combined, 0, combined.size(), true);
    
#ifdef SGX_ENCLAVE
    ocall_log(("merge_split_bitonic: Finished merging at level " + std::to_string(level)).c_str());
#endif

    std::vector<Element> out_bucket0(combined.begin(), combined.begin() + Z);
    std::vector<Element> out_bucket1(combined.begin() + Z, combined.end());

    return {out_bucket0, out_bucket1};
}

void Enclave::performButterflyNetwork(int B, int L, int Z) {
#ifdef SGX_ENCLAVE
    ocall_log("performButterflyNetwork: Starting butterfly network processing.");
#endif
    for (int level = 0; level < L; level++) {
#ifdef SGX_ENCLAVE
        ocall_log(("performButterflyNetwork: Processing level " + std::to_string(level)).c_str());
#endif
        for (int i = 0; i < B; i += 2) {
#ifdef SGX_ENCLAVE
            std::vector<Element> bucket1_enc = ocall_read_bucket_wrapper(level, i, Z);
            std::vector<Element> bucket2_enc = ocall_read_bucket_wrapper(level, i + 1, Z);
#else
            std::vector<Element> bucket1_enc = untrusted->read_bucket(level, i);
            std::vector<Element> bucket2_enc = untrusted->read_bucket(level, i + 1);
#endif
            std::vector<Element> bucket1 = decryptBucket(bucket1_enc);
            std::vector<Element> bucket2 = decryptBucket(bucket2_enc);
            auto buckets = merge_split_bitonic(bucket1, bucket2, level, L, Z);
#ifdef SGX_ENCLAVE
            ocall_write_bucket_wrapper(level + 1, i, encryptBucket(buckets.first));
            ocall_write_bucket_wrapper(level + 1, i + 1, encryptBucket(buckets.second));
#else
            untrusted->write_bucket(level + 1, i, encryptBucket(buckets.first));
            untrusted->write_bucket(level + 1, i + 1, encryptBucket(buckets.second));
#endif
        }
    }
#ifdef SGX_ENCLAVE
    ocall_log("performButterflyNetwork: Finished butterfly network processing.");
#endif
}

std::vector<Element> Enclave::extractFinalElements(int B, int L, int Z) {
#ifdef SGX_ENCLAVE
    ocall_log(("extractFinalElements: Extracting final elements from level " + std::to_string(L)).c_str());
#endif
    std::vector<Element> final_elements;
    for (int i = 0; i < B; i++) {
#ifdef SGX_ENCLAVE
        std::vector<Element> bucket_enc = ocall_read_bucket_wrapper(L, i, Z);
#else
        std::vector<Element> bucket_enc = untrusted->read_bucket(L, i);
#endif
        std::vector<Element> bucket = decryptBucket(bucket_enc);
        std::vector<Element> real_elements;
        for (const auto &elem : bucket)
            if (!elem.is_dummy)
                real_elements.push_back(elem);
        std::shuffle(real_elements.begin(), real_elements.end(), rng);
        final_elements.insert(final_elements.end(), real_elements.begin(), real_elements.end());
    }
#ifdef SGX_ENCLAVE
    ocall_log("extractFinalElements: Finished extraction.");
#endif
    return final_elements;
}

std::vector<int> Enclave::finalSort(const std::vector<Element>& final_elements) {
#ifdef SGX_ENCLAVE
    ocall_log("finalSort: Starting final sort of extracted elements.");
#endif
    std::vector<Element> sorted_elements = final_elements;
    std::sort(sorted_elements.begin(), sorted_elements.end(),
              [](const Element &a, const Element &b) {
                  return a.value < b.value;
              });
    std::vector<int> sorted_values;
    for (const auto &elem : sorted_elements)
        sorted_values.push_back(elem.value);
#ifdef SGX_ENCLAVE
    ocall_log("finalSort: Finished final sort.");
#endif
    return sorted_values;
}

std::vector<int> Enclave::oblivious_sort(const std::vector<int>& input_array, int bucket_size) {
#ifdef SGX_ENCLAVE
    ocall_log("oblivious_sort: Starting oblivious sort operation.");
#endif
    int n = input_array.size();
    int Z = bucket_size;
    auto params = computeBucketParameters(n, Z);
    int B = params.first;
    int L = params.second;
    initializeBuckets(input_array, B, Z);
    performButterflyNetwork(B, L, Z);
    std::vector<Element> final_elements = extractFinalElements(B, L, Z);
#ifdef SGX_ENCLAVE
    ocall_log("oblivious_sort: Finished oblivious sort operation.");
#endif
    return finalSort(final_elements);
}

#ifdef SGX_ENCLAVE
// ECALL wrapper: This function is callable from the untrusted host.
extern "C" void ecall_oblivious_sort(const int* input_array, int input_size, int bucket_size, int* output_array) {
    ocall_log("ecall_oblivious_sort: Entering ECALL.");
    std::vector<int> input(input_array, input_array + input_size);
    Enclave enclave;
    std::vector<int> sorted = enclave.oblivious_sort(input, bucket_size);
    if (sorted.size() != static_cast<size_t>(input_size)) {
        ocall_log("ecall_oblivious_sort: Warning - sorted size mismatch.");
    }
    memcpy(output_array, sorted.data(), sorted.size() * sizeof(int));
    ocall_log("ecall_oblivious_sort: Exiting ECALL.");
}
#endif
