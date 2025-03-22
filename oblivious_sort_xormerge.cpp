#include "oblivious_sort_merge.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>
#include <cstdint>

// ---------- XOR Helper Functions ----------
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

// ---------- UntrustedMemory Methods ----------
std::vector<Element> UntrustedMemory::read_bucket(int level, int bucket_index) {
    std::pair<int, int> key = { level, bucket_index };
    return storage[key];
}

void UntrustedMemory::write_bucket(int level, int bucket_index, const std::vector<Element>& bucket) {
    std::pair<int, int> key = { level, bucket_index };
    storage[key] = bucket;
}

std::vector<std::string> UntrustedMemory::get_access_log() {
    return access_log;
}

// ---------- Enclave Methods ----------
Enclave::Enclave(UntrustedMemory* u) : untrusted(u) {
    std::random_device rd;
    rng.seed(rd());
}

std::vector<Element> Enclave::encryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> encrypted = bucket;
    for(auto &elem : encrypted) {
        if(!elem.is_dummy) {
            elem.sorting = xor_encrypt_int(elem.sorting, encryption_key);
            elem.key = xor_encrypt_int(elem.key, encryption_key);
            elem.payload = xor_encrypt_string(elem.payload, encryption_key);
        }
    }
    return encrypted;
}

std::vector<Element> Enclave::decryptBucket(const std::vector<Element>& bucket) {
    return encryptBucket(bucket);
}

std::pair<int, int> Enclave::computeBucketParameters(int n, int Z) {
    int minimalB = static_cast<int>(std::ceil((2.0 * n) / Z));
    int safety_factor = 1;
    int B_required = minimalB * safety_factor;
    int B = 1;
    while(B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if(n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

void Enclave::initializeBuckets(const std::vector<Element>& input_array, int B, int Z) {
    int n = input_array.size();
    int group_size = (n + B - 1) / B;
    std::vector<Element> elements;
    std::uniform_int_distribution<int> key_dist(0, B - 1);
    for(const Element &elem : input_array) {
        int random_key = key_dist(rng);
        elements.push_back(Element{ elem.sorting, elem.key,false,elem.payload });
    }
    std::vector<std::vector<Element>> groups(B);
    for(int i = 0; i < B; i++){
        int start = i * group_size;
        int end = std::min(start + group_size, n);
        if(start < n)
            groups[i] = std::vector<Element>(elements.begin()+start, elements.begin()+end);
        else
            groups[i] = std::vector<Element>();
    }
    for(int i = 0; i < B; i++){
        std::vector<Element> bucket = groups[i];
        while(bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{0, 0,true,""});
        untrusted->write_bucket(0, i, encryptBucket(bucket));
    }
}

void Enclave::performButterflyNetwork(int B, int L, int Z) {
    for(int level = 0; level < L; level++){
        for(int i = 0; i < B; i += 2){
            std::vector<Element> bucket1 = untrusted->read_bucket(level, i);
            std::vector<Element> bucket2 = untrusted->read_bucket(level, i+1);
            bucket1 = decryptBucket(bucket1);
            bucket2 = decryptBucket(bucket2);
            auto buckets = merge_split(bucket1, bucket2, level, L, Z);
            untrusted->write_bucket(level+1, i, encryptBucket(buckets.first));
            untrusted->write_bucket(level+1, i+1, encryptBucket(buckets.second));
        }
    }
}

void Enclave::obliviousPermuteBucket(std::vector<Element>& bucket) {
    for(auto &elem : bucket) {
        elem.key = rng();
    }
    std::sort(bucket.begin(), bucket.end(), [](const Element &a, const Element &b){
        return a.key < b.key;
    });
}

std::vector<Element> Enclave::extractFinalElements(int B, int L) {
    std::vector<Element> final_elements;
    for(int i = 0; i < B; i++){
        std::vector<Element> bucket = untrusted->read_bucket(L, i);
        bucket = decryptBucket(bucket);
        obliviousPermuteBucket(bucket);
        for(const auto &elem : bucket)
            if(!elem.is_dummy)
                final_elements.push_back(elem);
    }
    return final_elements;
}

std::vector<Element> Enclave::finalSort(const std::vector<Element>& final_elements) {
    std::vector<Element> sorted = final_elements;
    std::sort(sorted.begin(), sorted.end(), [](const Element &a, const Element &b){
        return a.sorting < b.sorting;
    });
    return sorted;
}

std::vector<Element> Enclave::oblivious_sort(const std::vector<Element>& input_array, int bucket_size) {
    int n = input_array.size();
    int Z = bucket_size;
    auto params = computeBucketParameters(n, Z);
    int B = params.first, L = params.second;
    initializeBuckets(input_array, B, Z);
    performButterflyNetwork(B, L, Z);
    std::vector<Element> final_elements = extractFinalElements(B, L);
    return finalSort(final_elements);
}

std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {
    
    int L = total_levels;
    int bit_index = L - 1 - level;
    std::vector<Element> combined = bucket1;
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());
    std::vector<Element> out_bucket0, out_bucket1;
    for(const auto &elem : combined){
        if(!elem.is_dummy){
            int bit_val = (elem.key >> bit_index) & 1;
            if(bit_val == 0)
                out_bucket0.push_back(elem);
            else
                out_bucket1.push_back(elem);
        }
    }
    if(out_bucket0.size() > static_cast<size_t>(Z) || out_bucket1.size() > static_cast<size_t>(Z))
        throw std::overflow_error("Bucket overflow occurred in merge_split.");
    while(out_bucket0.size() < static_cast<size_t>(Z))
        out_bucket0.push_back(Element{0, 0, true, ""});
    while(out_bucket1.size() < static_cast<size_t>(Z))
        out_bucket1.push_back(Element{0, 0, true, ""});
    return { out_bucket0, out_bucket1 };
}
