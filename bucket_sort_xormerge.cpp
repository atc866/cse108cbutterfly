#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "nlohmann/json.hpp"
#include "oblivious_sort_xormerge.h"
#include <chrono>

using json = nlohmann::json;

int main(int argc, char* argv[]){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }
    std::string inputFileName = argv[1];
    std::ifstream ifs(inputFileName);
    if(!ifs.is_open()){
        std::cerr << "Error: Could not open " << inputFileName << "\n";
        return 1;
    }
    json j;
    ifs >> j;
    std::vector<Element> inputRows;
    for(const auto &row : j){
        int sorting = row["sorting"].get<int>();
        std::string payload = row["payload"].get<std::string>();
        inputRows.push_back(Element{ sorting, 0, false, payload});
    }
    std::cout << "Loaded " << inputRows.size() << " rows from " << inputFileName << ".\n";
    
    UntrustedMemory untrusted;
    Enclave enclave(&untrusted);
    
    int bucket_size = 256;
    std::cout << "Starting oblivious bucket sort with bucket size " << bucket_size << "...\n";
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Element> sortedRows = enclave.oblivious_sort(inputRows, bucket_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Done oblivious bucket sort with bucket size " << bucket_size << "...\n";
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    
    std::string outputFileName = "sorted_output_oblivious.json";
    std::ofstream ofs(outputFileName);
    if(!ofs.is_open()){
        std::cerr << "Error: Could not open " << outputFileName << " for writing\n";
        return 1;
    }
    json outputJson = json::array();
    for(const auto &row : sortedRows){
        json obj;
        obj["sorting"] = row.sorting;
        obj["payload"] = row.payload;
        outputJson.push_back(obj);
    }
    ofs << outputJson.dump(4) << std::endl;
    ofs.close();
    
    std::cout << "Wrote " << outputFileName << "\n";
    
    return 0;
}
