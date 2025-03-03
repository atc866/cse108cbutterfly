#include "oblivious_sort.h"
#include <iostream>
#include <random>

int main() {
    // Generate a random list of integers.
    std::vector<int> input_data;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(1, 1000);
    for (int i = 0; i < 100; i++) {
        input_data.push_back(dist(rng));
    }

    std::cout << "Input Data: ";
    for (int x : input_data)
        std::cout << x << " ";
    std::cout << std::endl;

    // Instantiate the untrusted memory and the enclave.
    UntrustedMemory untrusted;
    Enclave enclave(&untrusted);

    try {
        // Use a bucket size that is large enough (e.g., 8).
        std::vector<int> sorted_data = enclave.oblivious_sort(input_data, 8);

        std::cout << "Sorted Data: ";
        for (int x : sorted_data)
            std::cout << x << " ";
        std::cout << std::endl;

        // Optionally, print the untrusted memory access log.
        std::cout << "\nAccess Log:" << std::endl;
        for (const auto &log : untrusted.get_access_log())
            std::cout << log << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    return 0;
}
