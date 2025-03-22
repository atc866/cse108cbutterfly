#include <iostream>
#include <fstream>
#include <sstream>
#include "nlohmann/json.hpp"
#include "oblivious_sort_xorconstant.h"

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json_file>\n";
        return 1;
    }
    
    std::ifstream ifs(argv[1]);
    if (!ifs.is_open()) {
        std::cerr << "Error: Could not open input file.\n";
        return 1;
    }
    
    json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << "\n";
        return 1;
    }
    
    // Build vector<Element> from JSON.
    std::vector<Element> inputElements;
    for (const auto& item : j) {
        int sorting = item.at("sorting").get<int>();
        std::string payload = item.at("payload").get<std::string>();
        // Key will be assigned later; is_dummy is false.
        inputElements.push_back(Element{ sorting, payload, 0, false });
    }
    
    std::cout << "Loaded " << inputElements.size() << " elements.\n";
    
    UntrustedMemory untrusted;
    Enclave enclave(&untrusted);
    
    // Choose bucket size (Z). For example, 512.
    int bucket_size = 256;
    std::cout << "Starting oblivious bucket sort with bucket size " << bucket_size << "...\n";
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Element> sortedElements = enclave.oblivious_sort(inputElements, bucket_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Done oblivious bucket sort with bucket size " << bucket_size << "...\n";
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    
    // Build output JSON array.
    json output = json::array();
    for (const auto &e : sortedElements) {
        output.push_back({ {"sorting", e.sorting}, {"payload", e.payload} });
    }
    
    std::ofstream ofs("sorted_output.json");
    if (!ofs.is_open()) {
        std::cerr << "Error: Could not open output file.\n";
        return 1;
    }
    ofs << output.dump(4) << "\n";
    std::cout << "Sorted output written to sorted_output.json\n";
    
    return 0;
}
