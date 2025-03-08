#include "oblivious_sort.h"
#include <iostream>
#include <vector>
#include <cstring>

// Global instance simulating untrusted memory.
UntrustedMemory g_untrusted_memory;

// OCALL implementations.
extern "C" void ocall_read_bucket(int level, int bucket_index, void* bucket, int bucket_size) {
    std::vector<Element> result = g_untrusted_memory.read_bucket(level, bucket_index);
    memcpy(bucket, result.data(), bucket_size * sizeof(Element));
}

extern "C" void ocall_write_bucket(int level, int bucket_index, const void* bucket, int bucket_size) {
    std::vector<Element> vec(bucket_size);
    memcpy(vec.data(), bucket, bucket_size * sizeof(Element));
    g_untrusted_memory.write_bucket(level, bucket_index, vec);
}

extern "C" void ocall_log(const char* message) {
    std::cout << "[OCALL LOG] " << message << std::endl;
}

int main() {
    // Create a test array.
    std::vector<int> test_data = {9, 3, 7, 1, 5, 2, 8, 6, 4, 0};

#ifdef SGX_ENCLAVE
    // When SGX_ENCLAVE is defined, the Enclave constructor does not take an argument.
    Enclave enclave;
    std::cout<< "sgx_enclave defined"<<std::endl;
#else
    // When SGX_ENCLAVE is not defined, pass the pointer to the global untrusted memory.
    Enclave enclave(&g_untrusted_memory);
    std::cout<< "sgx_enclave not defined"<<std::endl;
#endif

    // Set the bucket size (ensure it meets the algorithm’s constraints).
    int bucket_size = 4;

    // Perform oblivious sort.
    std::vector<int> sorted = enclave.oblivious_sort(test_data, bucket_size);

    // Print the sorted array.
    std::cout << "Sorted array: ";
    for (int val : sorted) {
         std::cout << val << " ";
    }
    std::cout << std::endl;

    return 0;
}
