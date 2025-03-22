#include "oblivious_sort_xorconstant.h"
#include <iostream>
#include <random>
#include <cstring>
#include <cassert>
#include <cmath>

// Define a working block size for block-based I/O.
const int WORKING_SIZE = 64;

// ----- XOR Helper Functions -----
static std::string xor_encrypt_string(const std::string &s, int key) {
    std::string result = s;
    for (char &c : result) {
        c = c ^ (key & 0xFF);
    }
    return result;
}

static int xor_encrypt_int(int value, int key) {
    return value ^ key;
}

// ----- UntrustedMemory Methods -----
std::vector<Element> UntrustedMemory::read_bucket(int level, int bucket_index) {
    std::pair<int,int> key = { level, bucket_index };
    return storage[key];
}

void UntrustedMemory::write_bucket(int level, int bucket_index, const std::vector<Element>& bucket) {
    std::pair<int,int> key = { level, bucket_index };
    storage[key] = bucket;
}

std::vector<std::string> UntrustedMemory::get_access_log() {
    return access_log;
}

std::vector<Element> UntrustedMemory::read_bucket_block(int level, int bucket_index, int offset, int block_size) {
    std::pair<int,int> key = { level, bucket_index };
    const std::vector<Element>& bucket = storage[key];
    int end = std::min(static_cast<int>(bucket.size()), offset + block_size);
    return std::vector<Element>(bucket.begin() + offset, bucket.begin() + end);
}

void UntrustedMemory::write_bucket_block(int level, int bucket_index, int offset, const std::vector<Element>& block) {
    std::pair<int,int> key = { level, bucket_index };
    std::vector<Element>& bucket = storage[key];
    if (bucket.size() < static_cast<size_t>(offset + block.size()))
        bucket.resize(offset + block.size());
    for (size_t i = 0; i < block.size(); i++) {
        bucket[offset + i] = block[i];
    }
}

// ----- Enclave Methods -----
Enclave::Enclave(UntrustedMemory* u) : untrusted(u) {
    std::random_device rd;
    rng.seed(rd());
}

// XOR-based encryption: encrypt every field for each element.
std::vector<Element> Enclave::encryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> encrypted;
    for (const auto &elem : bucket) {
        Element e;
        // Encrypt integers with XOR.
        e.sorting = xor_encrypt_int(elem.sorting, encryption_key);
        e.key = xor_encrypt_int(elem.key, encryption_key);
        // Encrypt the payload string.
        e.payload = xor_encrypt_string(elem.payload, encryption_key);
        // Encrypt the dummy flag by toggling it (optional). Here we leave it unencrypted.
        e.is_dummy = elem.is_dummy; 
        encrypted.push_back(e);
    }
    return encrypted;
}

// XOR-based decryption is identical to encryption.
std::vector<Element> Enclave::decryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> decrypted;
    for (const auto &elem : bucket) {
        Element e;
        e.sorting = xor_encrypt_int(elem.sorting, encryption_key);
        e.key = xor_encrypt_int(elem.key, encryption_key);
        e.payload = xor_encrypt_string(elem.payload, encryption_key);
        e.is_dummy = elem.is_dummy;
        decrypted.push_back(e);
    }
    return decrypted;
}

std::pair<int, int> Enclave::computeBucketParameters(int n, int Z) {
    int minimalB = static_cast<int>(std::ceil((2.0 * n) / Z));
    // For constant-storage, we may not need a high safety factor.
    int safety_factor = 1;
    int B_required = minimalB * safety_factor;
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

// Initialize buckets by partitioning input elements and padding with dummies.
void Enclave::initializeBuckets(const std::vector<Element>& input_array, int B, int Z) {
    int n = input_array.size();
    int group_size = (n + B - 1) / B; // ceiling(n/B)
    for (int i = 0; i < B; i++) {
        std::vector<Element> bucket;
        int start = i * group_size;
        int end = std::min(start + group_size, n);
        for (int j = start; j < end; j++) {
            std::uniform_int_distribution<int> key_dist(0, B - 1);
            int random_key = key_dist(rng);
            // Use the input element directly.
            bucket.push_back(Element{ input_array[j].sorting, input_array[j].payload, random_key, false });
        }
        if (bucket.size() > static_cast<size_t>(Z / 2))
            throw std::overflow_error("Bucket overflow in initializeBuckets: too many real elements.");
        // Pad with dummy elements.
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{ 0, "", 0, true });
        // Encrypt the entire bucket.
        std::vector<Element> encrypted_bucket = encryptBucket(bucket);
        for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
            int block_size = std::min(WORKING_SIZE, Z - offset);
            std::vector<Element> block(encrypted_bucket.begin() + offset, encrypted_bucket.begin() + offset + block_size);
            untrusted->write_bucket_block(0, i, offset, block);
        }
    }
}

// In-memory bitonic merge.
void Enclave::bitonicMerge(std::vector<Element>& a, int low, int cnt, bool ascending) {
    if (cnt > 1) {
        int k = cnt / 2;
        for (int i = low; i < low + k; i++) {
            if ((ascending && a[i].key > a[i+k].key) ||
                (!ascending && a[i].key < a[i+k].key))
                std::swap(a[i], a[i+k]);
        }
        bitonicMerge(a, low, k, ascending);
        bitonicMerge(a, low+k, k, ascending);
    }
}

void Enclave::bitonicSort(std::vector<Element>& a, int low, int cnt, bool ascending) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonicSort(a, low, k, true);
        bitonicSort(a, low+k, k, false);
        bitonicMerge(a, low, cnt, ascending);
    }
}

// Constant-space merge using a fixed-size buffer.
void Enclave::constantSpaceMerge(std::vector<Element>& arr, int start, int mid, int end) {
    int left = start, right = mid, out = start;
    std::vector<Element> buffer;
    buffer.reserve(WORKING_SIZE);
    while (left < mid && right < end) {
        buffer.clear();
        while ((int)buffer.size() < WORKING_SIZE && left < mid && right < end) {
            if (arr[left].key <= arr[right].key)
                buffer.push_back(arr[left++]);
            else
                buffer.push_back(arr[right++]);
        }
        while ((int)buffer.size() < WORKING_SIZE && left < mid)
            buffer.push_back(arr[left++]);
        while ((int)buffer.size() < WORKING_SIZE && right < end)
            buffer.push_back(arr[right++]);
        for (size_t i = 0; i < buffer.size(); i++) {
            arr[out + i] = buffer[i];
        }
        out += buffer.size();
    }
    while (left < mid)
        arr[out++] = arr[left++];
    while (right < end)
        arr[out++] = arr[right++];
}

// Constant-space bitonic sort over blocks.
void Enclave::constantSpaceBitonicSort(std::vector<Element>& arr) {
    int n = arr.size();
    for (int start = 0; start < n; start += WORKING_SIZE) {
        int len = std::min(WORKING_SIZE, n - start);
        bitonicSort(arr, start, len, true);
    }
    int current_block_size = WORKING_SIZE;
    while (current_block_size < n) {
        for (int start = 0; start < n; start += 2 * current_block_size) {
            int mid = std::min(start + current_block_size, n);
            int end = std::min(start + 2 * current_block_size, n);
            constantSpaceMerge(arr, start, mid, end);
        }
        current_block_size *= 2;
    }
}

// Merge-split: combine two buckets and split them into two.
std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split_bitonic(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {
    
    int L = total_levels;
    int bit_index = L - 1 - level;
    
    // Count real elements for each output bucket.
    int count0 = 0, count1 = 0;
    std::vector<Element> combined = bucket1;
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());
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
    
    // Process combined array.
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
    // Sort combined array.
    bitonicSort(combined, 0, combined.size(), true);
    
    std::vector<Element> out_bucket0(combined.begin(), combined.begin() + Z);
    std::vector<Element> out_bucket1(combined.begin() + Z, combined.end());
    return { out_bucket0, out_bucket1 };
}

// Process the butterfly network.
void Enclave::performButterflyNetwork(int B, int L, int Z) {
    for (int level = 0; level < L; level++) {
        for (int i = 0; i < B; i += 2) {
            std::vector<Element> bucket1_enc, bucket2_enc;
            for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
                auto block1 = untrusted->read_bucket_block(level, i, offset, WORKING_SIZE);
                bucket1_enc.insert(bucket1_enc.end(), block1.begin(), block1.end());
                auto block2 = untrusted->read_bucket_block(level, i+1, offset, WORKING_SIZE);
                bucket2_enc.insert(bucket2_enc.end(), block2.begin(), block2.end());
            }
            std::vector<Element> bucket1 = decryptBucket(bucket1_enc);
            std::vector<Element> bucket2 = decryptBucket(bucket2_enc);
            auto merge_result = merge_split_bitonic(bucket1, bucket2, level, L, Z);
            // Re-encrypt buckets after merge-split.
            std::vector<Element> out_bucket0 = encryptBucket(merge_result.first);
            std::vector<Element> out_bucket1 = encryptBucket(merge_result.second);
            for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
                int block_size = std::min(WORKING_SIZE, Z - offset);
                std::vector<Element> block0(out_bucket0.begin() + offset, out_bucket0.begin() + offset + block_size);
                std::vector<Element> block1(out_bucket1.begin() + offset, out_bucket1.begin() + offset + block_size);
                untrusted->write_bucket_block(level+1, i, offset, block0);
                untrusted->write_bucket_block(level+1, i+1, offset, block1);
            }
        }
    }
}

// Extract final elements from the last level.
std::vector<Element> Enclave::extractFinalElements(int B, int L, int Z) {
    std::vector<Element> final_elements;
    for (int i = 0; i < B; i++) {
        std::vector<Element> bucket_enc;
        for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
            auto block = untrusted->read_bucket_block(L, i, offset, WORKING_SIZE);
            bucket_enc.insert(bucket_enc.end(), block.begin(), block.end());
        }
        std::vector<Element> bucket = decryptBucket(bucket_enc);
        obliviousPermuteBucket(bucket);
        for (const auto &elem : bucket)
            if (!elem.is_dummy)
                final_elements.push_back(elem);
    }
    return final_elements;
}

// Oblivious permutation using in-memory bitonic sort.
void Enclave::obliviousPermuteBucket(std::vector<Element>& bucket) {
    for (auto &elem : bucket) {
        elem.key = rng();
    }
    bitonicSort(bucket, 0, bucket.size(), true);
}

// Final sort (non-oblivious) by the sorting field.
std::vector<Element> Enclave::finalSort(const std::vector<Element>& final_elements) {
    std::vector<Element> sorted_elements = final_elements;
    std::sort(sorted_elements.begin(), sorted_elements.end(), [](const Element &a, const Element &b) {
        return a.sorting < b.sorting;
    });
    return sorted_elements;
}

// Main oblivious sort function.
std::vector<Element> Enclave::oblivious_sort(const std::vector<Element>& input_array, int bucket_size) {
    int n = input_array.size();
    int Z = bucket_size; // Full bucket size.
    auto params = computeBucketParameters(n, Z);
    int B = params.first, L = params.second;
    initializeBuckets(input_array, B, Z);
    performButterflyNetwork(B, L, Z);
    std::vector<Element> final_elements = extractFinalElements(B, L, Z);
    return finalSort(final_elements);
}
