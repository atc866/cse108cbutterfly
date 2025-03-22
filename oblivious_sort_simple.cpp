#include "oblivious_sort_simple.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include <cmath>
#include <stdexcept>

// -------------------------
// UntrustedMemory Methods
// -------------------------
std::vector<Element> UntrustedMemory::read_bucket(int level, int bucket_index) {
    std::pair<int, int> key = {level, bucket_index};
    std::ostringstream oss;
    oss << "Read bucket at level " << level << ", index " << bucket_index;
    access_log.push_back(oss.str());
    return storage[key];
}

void UntrustedMemory::write_bucket(int level, int bucket_index, const std::vector<Element>& bucket) {
    std::pair<int, int> key = {level, bucket_index};
    storage[key] = bucket;
    std::ostringstream oss;
    oss << "Write bucket at level " << level << ", index " << bucket_index;
    access_log.push_back(oss.str());
}

std::vector<std::string> UntrustedMemory::get_access_log() {
    return access_log;
}

std::vector< std::vector<Element> > UntrustedMemory::read_level(int level) {
    if (levels_storage.find(level) == levels_storage.end())
        return {};
    // For simplicity, we do not log full-level accesses here.
    return levels_storage[level];
}

void UntrustedMemory::write_level(int level, const std::vector< std::vector<Element> >& buckets) {
    levels_storage[level] = buckets;
    // Logging omitted for brevity.
}

// -------------------------
// Enclave Methods for Integers
// -------------------------
Enclave::Enclave(UntrustedMemory *u) : untrusted(u) {
    std::random_device rd;
    rng.seed(rd());
}

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
    // Compute B as the smallest power of 2 that is ≥ 2n/Z.
    int B_required = static_cast<int>(std::ceil((2.0 * n) / Z));
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {

    int L = total_levels;
    int bit_index = L - 1 - level;
    std::vector<Element> combined;
    combined.insert(combined.end(), bucket1.begin(), bucket1.end());
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());

    std::vector<Element> real_elements;
    for (const auto &elem : combined)
        if (!elem.is_dummy)
            real_elements.push_back(elem);

    std::vector<Element> out_bucket0, out_bucket1;
    for (const auto &elem : real_elements) {
        if (((elem.key >> bit_index) & 1) == 0)
            out_bucket0.push_back(elem);
        else
            out_bucket1.push_back(elem);
    }
    if (out_bucket0.size() > static_cast<size_t>(Z) ||
        out_bucket1.size() > static_cast<size_t>(Z))
        throw std::overflow_error("Bucket overflow occurred in merge_split.");

    while (out_bucket0.size() < static_cast<size_t>(Z))
        out_bucket0.push_back(Element{0, 0, true});
    while (out_bucket1.size() < static_cast<size_t>(Z))
        out_bucket1.push_back(Element{0, 0, true});

    return {out_bucket0, out_bucket1};
}

std::vector<int> Enclave::oblivious_sort(const std::vector<int>& input_array, int bucket_size) {
    int n = input_array.size();
    int Z = bucket_size;
    auto [B, L] = computeBucketParameters(n, Z);

    // Level 0 Initialization (Oblivious Random Bin Assignment)
    std::vector< std::vector<Element> > level0(B);
    std::vector<Element> elements;
    std::uniform_int_distribution<int> key_dist(0, B - 1);
    for (int x : input_array) {
        int random_key = key_dist(rng);
        elements.push_back(Element{x, random_key, false});
    }
    int group_size = (n + B - 1) / B;
    for (int i = 0; i < B; i++) {
        int start = i * group_size;
        int end = std::min(start + group_size, n);
        std::vector<Element> bucket;
        if (start < n)
            bucket.assign(elements.begin() + start, elements.begin() + end);
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{0, 0, true});
        level0[i] = encryptBucket(bucket);
    }
    untrusted->write_level(0, level0);

    // Merge-Split Phase
    for (int i = 0; i < L; i++) {
        std::vector< std::vector<Element> > current_level = untrusted->read_level(i);
        std::vector< std::vector<Element> > next_level(B);
        int block_size = 1 << (i + 1);
        for (int base = 0; base < B; base += block_size) {
            for (int k = 0; k < (1 << i); k++) {
                int idx1 = base + k;
                int idx2 = base + k + (1 << i);
                std::vector<Element> bucket1 = decryptBucket(current_level[idx1]);
                std::vector<Element> bucket2 = decryptBucket(current_level[idx2]);
                auto [bucket0, bucket1_out] = merge_split(bucket1, bucket2, i, L, Z);
                next_level[base + 2 * k]     = encryptBucket(bucket0);
                next_level[base + 2 * k + 1] = encryptBucket(bucket1_out);
            }
        }
        untrusted->write_level(i + 1, next_level);
    }

    // Final Extraction and Sort
    std::vector< std::vector<Element> > final_level = untrusted->read_level(L);
    std::vector<Element> final_elements;
    for (int i = 0; i < B; i++) {
        std::vector<Element> bucket = decryptBucket(final_level[i]);
        for (const auto &elem : bucket) {
            if (!elem.is_dummy)
                final_elements.push_back(elem);
        }
    }
    std::shuffle(final_elements.begin(), final_elements.end(), rng);
    std::sort(final_elements.begin(), final_elements.end(),
              [](const Element &a, const Element &b) {
                  return a.value < b.value;
              });
    std::vector<int> sorted_values;
    for (const auto &elem : final_elements)
        sorted_values.push_back(elem.value);
    return sorted_values;
}

// -------------------------
// New: Oblivious Sort for Strings
// -------------------------

// Define an internal structure for string elements.
struct StringElement {
    std::string value;
    int key;
    bool is_dummy;
};

// Simple string encryption/decryption using XOR on each character.
static std::string encryptString(const std::string& s) {
    std::string out = s;
    for (char &c : out)
        c = c ^ (Enclave::encryption_key & 0xFF);
    return out;
}

static std::string decryptString(const std::string& s) {
    // XOR encryption is reversible.
    return encryptString(s);
}

static std::vector<StringElement> encryptBucketString(const std::vector<StringElement>& bucket) {
    auto encrypted = bucket;
    for (auto &elem : encrypted) {
        if (!elem.is_dummy) {
            elem.value = encryptString(elem.value);
            elem.key ^= Enclave::encryption_key;
        }
    }
    return encrypted;
}

static std::vector<StringElement> decryptBucketString(const std::vector<StringElement>& bucket) {
    auto decrypted = bucket;
    for (auto &elem : decrypted) {
        if (!elem.is_dummy) {
            elem.value = decryptString(elem.value);
            elem.key ^= Enclave::encryption_key;
        }
    }
    return decrypted;
}

static std::pair<int, int> computeBucketParametersString(int n, int Z) {
    int B_required = static_cast<int>(std::ceil((2.0 * n) / Z));
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

static std::pair<std::vector<StringElement>, std::vector<StringElement>> merge_split_string(
    const std::vector<StringElement>& bucket1,
    const std::vector<StringElement>& bucket2,
    int level, int total_levels, int Z) {

    int L = total_levels;
    int bit_index = L - 1 - level;
    std::vector<StringElement> combined;
    combined.insert(combined.end(), bucket1.begin(), bucket1.end());
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());

    std::vector<StringElement> real_elements;
    for (const auto &elem : combined)
        if (!elem.is_dummy)
            real_elements.push_back(elem);

    std::vector<StringElement> out_bucket0, out_bucket1;
    for (const auto &elem : real_elements) {
        if (((elem.key >> bit_index) & 1) == 0)
            out_bucket0.push_back(elem);
        else
            out_bucket1.push_back(elem);
    }
    if (out_bucket0.size() > static_cast<size_t>(Z) ||
        out_bucket1.size() > static_cast<size_t>(Z))
        throw std::overflow_error("Bucket overflow occurred in merge_split_string.");

    while (out_bucket0.size() < static_cast<size_t>(Z))
        out_bucket0.push_back(StringElement{"", 0, true});
    while (out_bucket1.size() < static_cast<size_t>(Z))
        out_bucket1.push_back(StringElement{"", 0, true});

    return {out_bucket0, out_bucket1};
}

std::vector<std::string> Enclave::oblivious_sort(const std::vector<std::string>& input_array, int bucket_size) {
    int n = input_array.size();
    int Z = bucket_size;
    auto [B, L] = computeBucketParametersString(n, Z);

    // Level 0 Initialization for strings.
    std::vector< std::vector<StringElement> > level0(B);
    std::vector<StringElement> elements;
    std::uniform_int_distribution<int> key_dist(0, B - 1);
    for (const auto &s : input_array) {
        int random_key = key_dist(rng);
        elements.push_back(StringElement{s, random_key, false});
    }
    int group_size = (n + B - 1) / B;
    for (int i = 0; i < B; i++) {
        int start = i * group_size;
        int end = std::min(start + group_size, n);
        std::vector<StringElement> bucket;
        if (start < n)
            bucket.assign(elements.begin() + start, elements.begin() + end);
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(StringElement{"", 0, true});
        level0[i] = encryptBucketString(bucket);
    }
    // For the string version, we simulate untrusted memory storage locally.
    // (In a full implementation, you might have a separate UntrustedMemory for strings.)
    std::vector< std::vector<StringElement> > current_level = level0;

    // Merge-Split Phase for strings.
    for (int i = 0; i < L; i++) {
        std::vector< std::vector<StringElement> > next_level(B);
        int block_size = 1 << (i + 1);
        for (int base = 0; base < B; base += block_size) {
            for (int k = 0; k < (1 << i); k++) {
                int idx1 = base + k;
                int idx2 = base + k + (1 << i);
                std::vector<StringElement> bucket1 = decryptBucketString(current_level[idx1]);
                std::vector<StringElement> bucket2 = decryptBucketString(current_level[idx2]);
                auto [bucket0, bucket1_out] = merge_split_string(bucket1, bucket2, i, L, Z);
                next_level[base + 2 * k]     = encryptBucketString(bucket0);
                next_level[base + 2 * k + 1] = encryptBucketString(bucket1_out);
            }
        }
        current_level = next_level;
    }

    // Final Extraction and Local Sort for strings.
    std::vector<StringElement> final_elements;
    for (int i = 0; i < B; i++) {
        std::vector<StringElement> bucket = decryptBucketString(current_level[i]);
        for (const auto &elem : bucket) {
            if (!elem.is_dummy)
                final_elements.push_back(elem);
        }
    }
    std::shuffle(final_elements.begin(), final_elements.end(), rng);
    std::sort(final_elements.begin(), final_elements.end(),
              [](const StringElement &a, const StringElement &b) {
                  return a.value < b.value;
              });
    std::vector<std::string> sorted_values;
    for (const auto &elem : final_elements)
        sorted_values.push_back(elem.value);
    return sorted_values;
}
