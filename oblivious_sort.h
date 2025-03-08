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
#include <cstring> // for memcpy

#ifdef SGX_ENCLAVE
    // SGX-specific headers.
    #include "sgx_trts.h"
    extern "C" {
        // OCALL prototypes. These must also be declared in the EDL file.
        void ocall_read_bucket(int level, int bucket_index, void* bucket, int bucket_size);
        void ocall_write_bucket(int level, int bucket_index, const void* bucket, int bucket_size);
        void ocall_log(const char* message);
    }
#endif

// Represents a data element. For real elements, is_dummy is false.
struct Element {
    int value;
    int key;
    bool is_dummy;
};

// UntrustedMemory simulates untrusted storage (outside the enclave) that holds encrypted buckets.
// Remove the #ifndef SGX_ENCLAVE guard so that this class is defined for both the host and simulation.
class UntrustedMemory {
public:
    // Storage: keys are (level, bucket_index) and values are encrypted buckets.
    std::map<std::pair<int, int>, std::vector<Element>> storage;
    std::vector<std::string> access_log;

    // Read an encrypted bucket from untrusted memory.
    std::vector<Element> read_bucket(int level, int bucket_index);

    // Write an encrypted bucket to untrusted memory.
    void write_bucket(int level, int bucket_index, const std::vector<Element>& bucket);

    // Retrieve the access log.
    std::vector<std::string> get_access_log();
};

// Enclave represents the trusted SGX enclave.
// In simulation (SGX_MODE=SIM), it uses OCALLs to access untrusted memory.
class Enclave {
public:
#ifdef SGX_ENCLAVE
    // In the enclave, no direct pointer to untrusted memory is maintained.
    Enclave() {
        std::random_device rd;
        rng.seed(rd());
    }
#else
    // For simulation outside SGX, keep a pointer to UntrustedMemory.
    UntrustedMemory *untrusted;
    Enclave(UntrustedMemory *u) : untrusted(u) {
        std::random_device rd;
        rng.seed(rd());
    }
#endif

    std::mt19937 rng; // Random number generator.

    // A fixed key for our simulated encryption.
    static constexpr int encryption_key = 0xdeadbeef;

    // Simulated encryption: XOR each integer field with encryption_key.
    static std::vector<Element> encryptBucket(const std::vector<Element>& bucket);

    // Simulated decryption.
    static std::vector<Element> decryptBucket(const std::vector<Element>& bucket);

    // Computes the bucket parameters (B: number of buckets, L: number of levels)
    // given the input size n and bucket capacity Z.
    std::pair<int, int> computeBucketParameters(int n, int Z);

    // Step 1: Initializes buckets by assigning random keys, partitioning the input, and padding with dummies.
    void initializeBuckets(const std::vector<int>& input_array, int B, int Z);

    // Step 2: Processes the butterfly network by performing MergeSplit on each bucket pair.
    void performButterflyNetwork(int B, int L, int Z);

    // Step 3: Extracts final elements from the last level and performs a local oblivious permutation.
    std::vector<Element> extractFinalElements(int B, int L, int Z);

    // Step 4: Performs a final non-oblivious sort on the extracted elements.
    std::vector<int> finalSort(const std::vector<Element>& final_elements);

    // The main oblivious sort function.
    std::vector<int> oblivious_sort(const std::vector<int>& input_array, int bucket_size);

    // Bitonic sort based functions for constant storage MergeSplit.
    void bitonicMerge(std::vector<Element>& a, int low, int cnt, bool ascending);
    void bitonicSort(std::vector<Element>& a, int low, int cnt, bool ascending);

    // Modified MergeSplit function that uses bitonic sort.
    std::pair<std::vector<Element>, std::vector<Element>> merge_split_bitonic(
        const std::vector<Element>& bucket1,
        const std::vector<Element>& bucket2,
        int level, int total_levels, int Z);

#ifdef SGX_ENCLAVE
    // Helper wrappers to invoke OCALLs.
    std::vector<Element> ocall_read_bucket_wrapper(int level, int bucket_index, int bucket_size);
    void ocall_write_bucket_wrapper(int level, int bucket_index, const std::vector<Element>& bucket);
#endif
};

#endif // OBLIVIOUS_SORT_H
