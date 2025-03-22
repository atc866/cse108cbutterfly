#ifndef OBLIVIOUS_SORT_H
#define OBLIVIOUS_SORT_H

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <cmath>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <utility>

// Represents a data element for integers. For real elements, is_dummy is false.
struct Element {
    int value;
    int key;
    bool is_dummy;
};

// UntrustedMemory simulates untrusted storage (outside the enclave) that holds encrypted buckets.
class UntrustedMemory {
public:
    // Storage: keys are (level, bucket_index) and values are encrypted buckets.
    std::map<std::pair<int, int>, std::vector<Element>> storage;
    // Additional storage for entire levels.
    std::map<int, std::vector< std::vector<Element> >> levels_storage;
    std::vector<std::string> access_log;

    // Read an encrypted bucket from untrusted memory.
    std::vector<Element> read_bucket(int level, int bucket_index);

    // Write an encrypted bucket to untrusted memory.
    void write_bucket(int level, int bucket_index, const std::vector<Element>& bucket);

    // Retrieve the access log.
    std::vector<std::string> get_access_log();

    // New methods: Read and write an entire level (array of buckets) from/to untrusted memory.
    std::vector< std::vector<Element> > read_level(int level);
    void write_level(int level, const std::vector< std::vector<Element> >& buckets);
};

// Enclave represents the trusted SGX enclave. It decrypts data from untrusted memory,
// performs the oblivious sort operations, and reencrypts data when writing back.
class Enclave {
public:
    UntrustedMemory *untrusted;
    std::mt19937 rng; // Random number generator.

    // A fixed key for our simulated encryption.
    static constexpr int encryption_key = 0xdeadbeef;

    // Constructor: initializes the enclave with a pointer to untrusted memory.
    Enclave(UntrustedMemory *u);

    // Simulated encryption for integer buckets: XOR each integer field with encryption_key.
    static std::vector<Element> encryptBucket(const std::vector<Element>& bucket);

    // Simulated decryption for integer buckets.
    static std::vector<Element> decryptBucket(const std::vector<Element>& bucket);

    // Computes the bucket parameters (B: number of buckets, L: number of levels)
    // given the input size n and bucket capacity Z.
    std::pair<int, int> computeBucketParameters(int n, int Z);

    // MergeSplit operation for integer buckets: combines two buckets and partitions real elements based on a designated bit.
    std::pair<std::vector<Element>, std::vector<Element>> merge_split(
        const std::vector<Element>& bucket1,
        const std::vector<Element>& bucket2,
        int level, int total_levels, int Z);

    // The main oblivious sort function for integers.
    std::vector<int> oblivious_sort(const std::vector<int>& input_array, int bucket_size);

    // -------------------------------
    // New: The oblivious sort function for strings.
    // -------------------------------
    // This routine converts each string into a StringElement (with a random key),
    // performs the oblivious sort via merge-split on encrypted levels stored in untrusted memory,
    // and finally extracts and returns a sorted vector of strings.
    std::vector<std::string> oblivious_sort(const std::vector<std::string>& input_array, int bucket_size);
};

#endif // OBLIVIOUS_SORT_H
