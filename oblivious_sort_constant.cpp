#include "oblivious_sort_constant.h"  // Ensure your header declares the same functions (see below)
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>
#include <cassert>
#include <cmath>
#include <iomanip>
// --------------------- Crypto++ Headers ---------------------
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cstring>

// --------------------- Crypto++ Helper Functions ---------------------
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



// Encrypt with explicit binary handling
std::string encrypt_blob(const std::string &plaintext) {
    initAES();
    std::string ciphertext;
    CTR_Mode<AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(aesKey, aesKey.size(), aesIV);
    StringSource ss(
        plaintext, 
        true,
        new StreamTransformationFilter(
            encryptor,
            new StringSink(ciphertext)
        )
    );
    
    return ciphertext;
}

// Decrypt with explicit binary handling
std::string decrypt_blob(const std::string &ciphertext) {
    initAES();
    std::string plaintext;
    CTR_Mode<AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(aesKey, aesKey.size(), aesIV);
    StringSource ss(
        ciphertext, 
        true,
        new StreamTransformationFilter(
            decryptor,
            new StringSink(plaintext)
        )
    );
    return plaintext;
}

    // Serialize an Element into a binary string.
    // We pack the following in order: 
    // - sorting (4 bytes, int)
    // - key (4 bytes, int)
    // - is_dummy flag (1 byte)
    // - payload length (4 bytes, uint32_t)
    // - payload content.
    std::string serializeElement(const Element &e) {
        std::string out;
        out.append(reinterpret_cast<const char*>(&e.sorting), sizeof(e.sorting));
        out.append(reinterpret_cast<const char*>(&e.key), sizeof(e.key));
        char flag = e.is_dummy ? 1 : 0;
        out.append(&flag, sizeof(flag));
        uint32_t payload_len = static_cast<uint32_t>(e.payload.size());
        out.append(reinterpret_cast<const char*>(&payload_len), sizeof(payload_len));
        out.append(e.payload);
        return out;
    }
    // Deserialize a binary string back into an Element.
    Element deserializeElement(const std::string &data) {
        const size_t header_size = sizeof(int) + sizeof(int) + 1 + sizeof(uint32_t); // 4+4+1+4 = 13 bytes
        if (data.size() < header_size) {
            std::cout<<data<<std::endl;
            throw std::runtime_error("Decrypted blob too short to contain header.");
        }
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
        uint32_t payload_len;
        std::memcpy(&payload_len, data.data() + offset, sizeof(payload_len));
        offset += sizeof(payload_len);
        if (offset + payload_len > data.size()) {
            throw std::out_of_range("Decrypted blob payload length out of range.");
        }
        e.payload = data.substr(offset, payload_len);
        return e;
    }
    
} // end anonymous namespace

// Fixed working buffer size for streaming operations.
const int WORKING_SIZE = 64;
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
    std::vector<Element> block(bucket.begin() + offset, bucket.begin() + end);
    return block;
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
// Updated encryption: serialize the entire Element (including the is_dummy flag)
// and encrypt the resulting blob. The cleartext fields are then overwritten.
std::vector<Element> Enclave::encryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> encrypted;
    for (const auto &elem : bucket) {
        // Serialize the complete element.
        std::string serialized = serializeElement(elem);
        // Encrypt the serialized data.
        std::string encrypted_blob = encrypt_blob(serialized);
        // Create a new Element that hides cleartext information.
        Element encElem;
        encElem.sorting = 0;   // Overwrite with dummy value.
        encElem.key = 0;       // Overwrite with dummy value.
        encElem.is_dummy = false; // The actual flag is hidden inside the blob.
        // Store the encrypted blob in the payload field.
        encElem.payload = encrypted_blob;
        encrypted.push_back(encElem);
    }
    return encrypted;
}

// Updated decryption: decrypt each element's blob and deserialize to recover all fields.
std::vector<Element> Enclave::decryptBucket(const std::vector<Element>& bucket) {
    std::vector<Element> decrypted;
    for (const auto &encElem : bucket) {
        // Decrypt the blob stored in the payload.
        std::string decrypted_blob = decrypt_blob(encElem.payload);
        // Deserialize to recover the original Element.
        Element elem = deserializeElement(decrypted_blob);
        decrypted.push_back(elem);
    }
    return decrypted;
}


std::pair<int,int> Enclave::computeBucketParameters(int n, int Z) {
    int minimalB = static_cast<int>(std::ceil((2.0 * n) / Z));
    int safety_factor = 16; // Increase safety
    int B_required = minimalB * safety_factor;
    int B = 1;
    while (B < B_required)
        B *= 2;
    int L = static_cast<int>(std::log2(B));
    if (n > B * (Z / 2))
        throw std::invalid_argument("Bucket size too small for input size.");
    return {B, L};
}

// Initialize buckets by dividing the input evenly and padding with dummies,
// writing each bucket block-by-block.
// Change the signature to match the header:
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
            // Use the sorting value and payload from the input Element.
            bucket.push_back(Element{ input_array[j].sorting, input_array[j].payload, random_key, false });
        }
        if (bucket.size() > static_cast<size_t>(Z / 2))
            throw std::overflow_error("Bucket overflow in initializeBuckets: too many real elements.");
        // Create dummy elements correctly: sorting=0, payload="", key=0, is_dummy=true.
        while (bucket.size() < static_cast<size_t>(Z))
            bucket.push_back(Element{ 0, "", 0, true });
        std::vector<Element> encrypted_bucket = encryptBucket(bucket);
        for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
            int block_size = std::min(WORKING_SIZE, Z - offset);
            std::vector<Element> block(encrypted_bucket.begin() + offset, encrypted_bucket.begin() + offset + block_size);
            untrusted->write_bucket_block(0, i, offset, block);
        }
    }
}

// In-memory bitonic sort and merge (since Z is constant, these use O(1) storage).
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

// Streaming merge: Merge two sorted subarrays [start, mid) and [mid, end)
// using a fixed-size working buffer.
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

// Streaming block-based bitonic sort: Sort an array (of size 2Z) using block merges.
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

// Modified merge_split_bitonic that streams the processing of two buckets.
// The parameter Z is passed in so that we know the bucket size.
std::pair<std::vector<Element>, std::vector<Element>> Enclave::merge_split_bitonic(
    const std::vector<Element>& bucket1,
    const std::vector<Element>& bucket2,
    int level, int total_levels, int Z) {

    int L = total_levels;
    int bit_index = L - 1 - level;

    // Streaming count: Process each bucket in blocks.
    int count0 = 0, count1 = 0;
    for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
        int block_size = std::min(WORKING_SIZE, Z - offset);
        for (int i = 0; i < block_size; i++) {
            const Element &e = bucket1[offset + i];
            if (!e.is_dummy) {
                if (((e.key >> bit_index) & 1) == 0)
                    count0++;
                else
                    count1++;
            }
        }
        for (int i = 0; i < block_size; i++) {
            const Element &e = bucket2[offset + i];
            if (!e.is_dummy) {
                if (((e.key >> bit_index) & 1) == 0)
                    count0++;
                else
                    count1++;
            }
        }
    }
    if (count0 > Z || count1 > Z)
        throw std::overflow_error("Bucket overflow occurred in merge_split.");

    int needed_dummies0 = Z - count0;
    int needed_dummies1 = Z - count1;
    int assigned_dummies0 = 0, assigned_dummies1 = 0;

    // Stream the two buckets into a combined array block-by-block.
    std::vector<Element> combined(2 * Z);
    for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
        int block_size = std::min(WORKING_SIZE, Z - offset);
        for (int i = 0; i < block_size; i++) {
            Element e = bucket1[offset + i];
            if (!e.is_dummy) {
                int bit_val = (e.key >> bit_index) & 1;
                e.key = (bit_val << 1);
            }
            combined[offset + i] = e;
        }
        for (int i = 0; i < block_size; i++) {
            Element e = bucket2[offset + i];
            if (!e.is_dummy) {
                int bit_val = (e.key >> bit_index) & 1;
                e.key = (bit_val << 1);
            }
            combined[Z + offset + i] = e;
        }
    }
    // Stream-tag dummy elements.
    for (int offset = 0; offset < 2 * Z; offset += WORKING_SIZE) {
        int block_size = std::min(WORKING_SIZE, 2 * Z - offset);
        for (int i = 0; i < block_size; i++) {
            int idx = offset + i;
            if (combined[idx].is_dummy) {
                if (assigned_dummies0 < needed_dummies0) {
                    combined[idx].key = 1;
                    assigned_dummies0++;
                } else {
                    combined[idx].key = 3;
                    assigned_dummies1++;
                }
            }
        }
    }
    // Sort the combined array using our constant-space bitonic sort.
    constantSpaceBitonicSort(combined);
    // Stream-copy sorted results into two output buckets.
    std::vector<Element> out_bucket0(Z), out_bucket1(Z);
    for (int offset = 0; offset < Z; offset += WORKING_SIZE) {
        int block_size = std::min(WORKING_SIZE, Z - offset);
        for (int i = 0; i < block_size; i++) {
            out_bucket0[offset + i] = combined[offset + i];
            out_bucket1[offset + i] = combined[Z + offset + i];
        }
    }
    return { out_bucket0, out_bucket1 };
}

// Process the butterfly network using block-based I/O.
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

// Stream extraction of final elements.
std::vector<Element> Enclave::extractFinalElements(int B, int L, int Z) {
    std::vector<Element> final_elements;
    // Assume we know Z (the bucket size) from the original oblivious_sort call.
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


// Oblivious permutation on a bucket using in-memory bitonic sort (bucket size is constant).
void Enclave::obliviousPermuteBucket(std::vector<Element>& bucket) {
    for (auto &elem : bucket) {
        elem.key = rng();
    }
    bitonicSort(bucket, 0, bucket.size(), true);
}

// Final non-oblivious sort of extracted elements. (If final_elements is large, use external sort.)
// Change finalSort to return vector<Element> and compare using the 'sorting' field.
std::vector<Element> Enclave::finalSort(const std::vector<Element>& final_elements) {
    std::vector<Element> sorted_elements = final_elements;
    std::sort(sorted_elements.begin(), sorted_elements.end(), [](const Element &a, const Element &b) {
        return a.sorting < b.sorting;
    });
    return sorted_elements;
}


// Main oblivious sort function. The bucket size is provided as bucket_size (alias Z).
std::vector<Element> Enclave::oblivious_sort(const std::vector<Element>& input_array, int bucket_size) {
    int n = input_array.size();
    int Z = bucket_size; // Define Z here.
    auto params = computeBucketParameters(n, Z);
    int B = params.first, L = params.second;
    initializeBuckets(input_array, B, Z);
    performButterflyNetwork(B, L, Z);
    std::vector<Element> final_elements = extractFinalElements(B, L, Z);
    return finalSort(final_elements);
}