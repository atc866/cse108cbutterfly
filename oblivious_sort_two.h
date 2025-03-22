#ifndef OBLIVIOUS_SORT_TWO_H
#define OBLIVIOUS_SORT_TWO_H

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <cmath>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <utility>

// Represents a data element with a numeric sorting column and a variable-length payload.
struct Element {
    int sorting;        // Numeric sorting column.
    int key;
    bool is_dummy;
    std::string payload; // Variable-length payload.
};

class UntrustedMemory {
public:
    std::map<std::pair<int, int>, std::vector<Element>> storage;
    std::vector<std::string> access_log;

    std::vector<Element> read_bucket(int level, int bucket_index);
    void write_bucket(int level, int bucket_index, const std::vector<Element>& bucket);
    std::vector<std::string> get_access_log();
};

class Enclave {
public:
    UntrustedMemory* untrusted;
    std::mt19937 rng;

    static constexpr int encryption_key = 0xdeadbeef;

    Enclave(UntrustedMemory* u);
    static std::vector<Element> encryptBucket(const std::vector<Element>& bucket);
    static std::vector<Element> decryptBucket(const std::vector<Element>& bucket);

    std::pair<int, int> computeBucketParameters(int n, int Z);
    void initializeBuckets(const std::vector<Element>& input_array, int B, int Z);
    void performButterflyNetwork(int B, int L, int Z);
    std::vector<Element> extractFinalElements(int B, int L);
    std::vector<Element> finalSort(const std::vector<Element>& final_elements);
    std::vector<Element> oblivious_sort(const std::vector<Element>& input_array, int bucket_size);

    void bitonicMerge(std::vector<Element>& a, int low, int cnt, bool ascending);
    void bitonicSort(std::vector<Element>& a, int low, int cnt, bool ascending);
    std::pair<std::vector<Element>, std::vector<Element>> merge_split_bitonic(
        const std::vector<Element>& bucket1,
        const std::vector<Element>& bucket2,
        int level, int total_levels, int Z);
    void obliviousPermuteBucket(std::vector<Element>& bucket);
};

#endif // OBLIVIOUS_SORT_TWO_H
