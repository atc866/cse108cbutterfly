#ifndef OBLIVIOUS_SORT_CONSTANT_H
#define OBLIVIOUS_SORT_CONSTANT_H

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <cmath>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <utility>

/*
 * Element:
 * Represents a data element used in oblivious sorting.
 *
 * Fields:
 *   - sorting: An integer value used for sorting.
 *   - payload: A string holding the associated data.
 *   - key: An integer used during the sorting process.
 *   - is_dummy: A flag indicating if the element is a dummy.
 *
 * For complete obliviousness, the entire Element (including is_dummy)
 * is serialized into a single blob and then encrypted using Crypto++.
 */
struct Element {
    int sorting;
    std::string payload;
    int key;
    bool is_dummy;
};

class UntrustedMemory {
public:
    // Storage: keys are pairs (level, bucket_index), values are encrypted buckets.
    std::map<std::pair<int, int>, std::vector<Element>> storage;
    std::vector<std::string> access_log;

    // Reads an encrypted bucket from untrusted memory.
    std::vector<Element> read_bucket(int level, int bucket_index);
    // Writes an encrypted bucket to untrusted memory.
    void write_bucket(int level, int bucket_index, const std::vector<Element>& bucket);
    // Returns an access log.
    std::vector<std::string> get_access_log();

    // Block-based I/O for streaming operations.
    std::vector<Element> read_bucket_block(int level, int bucket_index, int offset, int block_size);
    void write_bucket_block(int level, int bucket_index, int offset, const std::vector<Element>& block);
};

class Enclave {
public:
    UntrustedMemory* untrusted;
    std::mt19937 rng; // Random number generator.

    // Fixed encryption key for simulation.
    //static constexpr int encryption_key = 0xdeadbeef;

    Enclave(UntrustedMemory* u);

    // Encrypts a bucket by serializing each Element (including the dummy flag)
    // into a single blob and then encrypting that blob using Crypto++.
    static std::vector<Element> encryptBucket(const std::vector<Element>& bucket);
    // Decrypts a bucket by decrypting each element's blob and deserializing it.
    static std::vector<Element> decryptBucket(const std::vector<Element>& bucket);
    void printHexa(const std::string& label, const std::string& data);
    // Computes bucket parameters (B: number of buckets, L: number of levels)
    // given the input size n and bucket capacity Z.
    std::pair<int, int> computeBucketParameters(int n, int Z);
    // Initializes buckets by partitioning the input and padding with dummies.
    // (Input is now a vector<Element>.)
    void initializeBuckets(const std::vector<Element>& input_array, int B, int Z);
    // Processes the butterfly network using block-based I/O.
    void performButterflyNetwork(int B, int L, int Z);
    // Extracts final elements from the last level.
    std::vector<Element> extractFinalElements(int B, int L, int Z);
    // Final non-oblivious sort on the extracted elements.
    std::vector<Element> finalSort(const std::vector<Element>& final_elements);
    // Main oblivious sort function that now works on vector<Element>.
    std::vector<Element> oblivious_sort(const std::vector<Element>& input_array, int bucket_size);

    // In-memory bitonic sort functions.
    void bitonicMerge(std::vector<Element>& a, int low, int cnt, bool ascending);
    void bitonicSort(std::vector<Element>& a, int low, int cnt, bool ascending);

    // Streaming/block-based helper functions.
    void constantSpaceMerge(std::vector<Element>& arr, int start, int mid, int end);
    void constantSpaceBitonicSort(std::vector<Element>& arr);
    std::pair<std::vector<Element>, std::vector<Element>> merge_split_bitonic(
        const std::vector<Element>& bucket1,
        const std::vector<Element>& bucket2,
        int level, int total_levels, int Z);
    void obliviousPermuteBucket(std::vector<Element>& bucket);
};

#endif // OBLIVIOUS_SORT_CONSTANT_H
