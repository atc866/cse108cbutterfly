#include "oblivious_sort_merge.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>
#include <cstdint>

// Include Crypto++ headers for stronger encryption.
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>

// Anonymous namespace for helper functions.
namespace {
    using namespace CryptoPP;

    // Global AES key and IV for encryption within the enclave.
    SecByteBlock aesKey(AES::DEFAULT_KEYLENGTH);
    byte aesIV[AES::BLOCKSIZE];
    bool aesInitialized = false;

    // Initialize the AES key and IV once.
    void initAES() {
        if (!aesInitialized) {
            AutoSeededRandomPool prng;
            prng.GenerateBlock(aesKey, aesKey.size());
            prng.GenerateBlock(aesIV, sizeof(aesIV));
            aesInitialized = true;
        }
    }

    // Encrypt a string using AES in CTR mode.
    std::string encrypt_string(const std::string &s) {
        initAES();
        std::string ciphertext;
        CTR_Mode<AES>::Encryption encryptor;
        encryptor.SetKeyWithIV(aesKey, aesKey.size(), aesIV);
        StringSource ss(s, true,
            new StreamTransformationFilter(encryptor,
                new StringSink(ciphertext)
            )
        );
        return ciphertext;
    }

    // Decrypt a string using AES in CTR mode.
    std::string decrypt_string(const std::string &s) {
        initAES();
        std::string recovered;
        CTR_Mode<AES>::Decryption decryptor;
        decryptor.SetKeyWithIV(aesKey, aesKey.size(), aesIV);
        StringSource ss(s, true,
            new StreamTransformationFilter(decryptor,
                new StringSink(recovered)
            )
        );
        return recovered;
    }

    // --- Serialization Helpers ---
    // Serializes an Element into a binary string.
    std::string serializeElement(const Element& e) {
        std::string out;
        // Append the 4-byte sorting value.
        out.append(reinterpret_cast<const char*>(&e.sorting), sizeof(e.sorting));
        // Append the 4-byte key.
        out.append(reinterpret_cast<const char*>(&e.key), sizeof(e.key));
        // Append the is_dummy flag as a single byte.
        char flag = e.is_dummy ? 1 : 0;
        out.append(&flag, sizeof(flag));
        // Append the payload length (4 bytes) followed by the payload itself.
        uint32_t payload_size = e.payload.size();
        out.append(reinterpret_cast<const char*>(&payload_size), sizeof(payload_size));
        out.append(e.payload);
        return out;
    }

    // Deserializes a binary string back into an Element.
    Element deserializeElement(const std::string& data) {
        Element e;
        size_t offset = 0;
        std::memcpy(&e.sorting, data.data() + offset, sizeof(e.sorting));
        offset += sizeof(e.sorting);
        std::memcpy(&e.key, data.data() + offset, sizeof(e.key));
        offset += sizeof(e.key);
        char flag;
        std::memcpy(&flag, data.data() + offset, sizeof(flag));
        e.is_dummy = (flag != 0);
        offset += sizeof(flag);
        uint32_t payload_size;
        std::memcpy(&payload_size, data.data() + offset, sizeof(payload_size));
        offset += sizeof(payload_size);
        e.payload = data.substr(offset, payload_size);
        return e;
    }
} // anonymous namespace

// ----- UntrustedMemory Methods -----
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

// ----- Enclave Methods -----
Enclave::Enclave(UntrustedMemory* u) : untrusted(u) {
    std::random_device rd;
    rng.seed(rd());
}

// Updated encryption: Serialize and encrypt the entire Element structure, including the dummy flag.
std::vector<Element> Enclave::encryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> encrypted = bucket;
    for (auto& elem : encrypted) {
        // Serialize the complete Element (all fields).
        std::string serialized = serializeElement(elem);
        // Encrypt the serialized data.
        std::string encrypted_blob = encrypt_string(serialized);
        // Overwrite the cleartext fields to prevent leakage.
        elem.sorting = 0;
        elem.key = 0;
        elem.is_dummy = false; // The true flag is now hidden in the blob.
        // Store the encrypted blob in the payload.
        elem.payload = encrypted_blob;
    }
    return encrypted;
}

// Updated decryption: Decrypt and deserialize the encrypted blob to recover the entire Element.
std::vector<Element> Enclave::decryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> decrypted;
    for (const auto& enc_elem : bucket) {
        // Decrypt the blob stored in the payload.
        std::string decrypted_blob = decrypt_string(enc_elem.payload);
        // Deserialize to recover the original Element.
        Element e = deserializeElement(decrypted_blob);
        decrypted.push_back(e);
    }
    return decrypted;
}

std::pair<int,int> Enclave::computeBucketParameters(int n, int Z) {
    int minimalB = static_cast<int>(std::ceil((2.0 * n) / Z));
    int safety_factor = 1; // Increase safety if needed.
    int B_required = minimalB * safety_factor;
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

void Enclave::initializeBuckets(const std::vector<Element>& input_array, int B, int Z) {
    int n = input_array.size();
    std::vector<Element> elements;
    std::uniform_int_distribution<int> key_dist(0, B - 1);
    for (const Element &elem : input_array) {
        int random_key = key_dist(rng);
        // Use the sorting and payload from the input.
        elements.push_back(Element{ elem.sorting, random_key, false, elem.payload });
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
        // Pad with dummy elements until bucket reaches size Z.
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{ 0, 0, true, "" });
        
        untrusted->write_bucket(0, i, encryptBucket(bucket));
    }
}

// --- New merge_split function (no bitonic sort) ---
// Implements the MergeSplit as described in the paper.
std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {
    
    int L = total_levels;
    int bit_index = L - 1 - level;
    
    // Combine the two buckets (each of size Z) into one vector of size 2Z.
    std::vector<Element> combined = bucket1;
    combined.insert(combined.end(), bucket2.begin(), bucket2.end());
    
    // Partition the real (non-dummy) elements based on the (i+1)-th MSB.
    std::vector<Element> out_bucket0;
    std::vector<Element> out_bucket1;
    
    for (const auto &elem : combined) {
        if (!elem.is_dummy) {
            int bit_val = (elem.key >> bit_index) & 1;
            if (bit_val == 0)
                out_bucket0.push_back(elem);
            else
                out_bucket1.push_back(elem);
        }
    }
    
    // Check for overflow: each output bucket must have at most Z real elements.
    if (out_bucket0.size() > static_cast<size_t>(Z) || out_bucket1.size() > static_cast<size_t>(Z))
        throw std::overflow_error("Bucket overflow occurred in merge_split.");
    
    // Pad each bucket with dummy elements to reach size Z.
    while (out_bucket0.size() < static_cast<size_t>(Z))
        out_bucket0.push_back(Element{0, 0, true, ""});
    while (out_bucket1.size() < static_cast<size_t>(Z))
        out_bucket1.push_back(Element{0, 0, true, ""});
    
    return {out_bucket0, out_bucket1};
}

void Enclave::performButterflyNetwork(int B, int L, int Z) {
    for (int level = 0; level < L; level++) {
        for (int i = 0; i < B; i += 2) {
            std::vector<Element> bucket1_enc = untrusted->read_bucket(level, i);
            std::vector<Element> bucket2_enc = untrusted->read_bucket(level, i + 1);
            std::vector<Element> bucket1 = decryptBucket(bucket1_enc);
            std::vector<Element> bucket2 = decryptBucket(bucket2_enc);
            auto buckets = merge_split(bucket1, bucket2, level, L, Z);
            untrusted->write_bucket(level + 1, i, encryptBucket(buckets.first));
            untrusted->write_bucket(level + 1, i + 1, encryptBucket(buckets.second));
        }
    }
}

// Updated oblivious permutation: assign random keys then use std::sort.
void Enclave::obliviousPermuteBucket(std::vector<Element>& bucket) {
    for (auto &elem : bucket) {
         elem.key = rng();
    }
    std::sort(bucket.begin(), bucket.end(), [](const Element &a, const Element &b) {
         return a.key < b.key;
    });
}

std::vector<Element> Enclave::extractFinalElements(int B, int L) {
    std::vector<Element> final_elements;
    for (int i = 0; i < B; i++) {
        std::vector<Element> bucket_enc = untrusted->read_bucket(L, i);
        std::vector<Element> bucket = decryptBucket(bucket_enc);
        obliviousPermuteBucket(bucket);
        for (const auto& elem : bucket)
            if (!elem.is_dummy)
                final_elements.push_back(elem);
    }
    return final_elements;
}

std::vector<Element> Enclave::finalSort(const std::vector<Element>& final_elements) {
    std::vector<Element> sorted_elements = final_elements;
    std::sort(sorted_elements.begin(), sorted_elements.end(),
        [](const Element& a, const Element& b) {
            return a.sorting < b.sorting;
        });
    return sorted_elements;
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
