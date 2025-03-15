// main_oblivious_bucket_sort.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include "nlohmann/json.hpp"
#include "oblivious_sort_string.h"

// Helper function to trim whitespace from both ends of a string.
std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(s[start])) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(s[end - 1])) {
        --end;
    }
    return s.substr(start, end - start);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }
    
    std::string inputFileName = argv[1];
    std::ifstream ifs(inputFileName);
    if (!ifs.is_open()) {
        std::cerr << "Error: Could not open " << inputFileName << "\n";
        return 1;
    }
    
    // Read the entire file into a string.
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();
    
    // Remove the square brackets if present.
    if (!content.empty() && content.front() == '[') {
        content.erase(0, 1);
    }
    if (!content.empty() && content.back() == ']') {
        content.pop_back();
    }
    
    // Split the content by commas and trim whitespace.
    std::vector<std::string> inputValues;
    std::istringstream iss(content);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token = trim(token);
        if (!token.empty()) {
            inputValues.push_back(token);
        }
    }
    
    std::cout << "Loaded " << inputValues.size() << " strings from " << inputFileName << ".\n";
    
    // Create an UntrustedMemory and Enclave.
    UntrustedMemory untrusted;
    Enclave enclave(&untrusted);
    
    // Choose a bucket size (experiment with this value, e.g. 16, 32, or 64).
    int bucket_size = 32;
    std::cout << "Starting oblivious bucket sort with bucket size " << bucket_size << "...\n";
    
    // Sort the strings using your oblivious_sort (make sure it supports strings).
    std::vector<std::string> sortedOblivious = enclave.oblivious_sort(inputValues, bucket_size);
    
    // Write the sorted output to a file as a valid JSON array.
    std::string outputFileName = "sorted_output_oblivious.json";
    std::ofstream ofs(outputFileName);
    if (!ofs.is_open()) {
        std::cerr << "Error: Could not open " << outputFileName << " for writing\n";
        return 1;
    }
    
    // Write output as a pretty-printed JSON array with quotes.
    ofs << "[\n";
    for (size_t i = 0; i < sortedOblivious.size(); ++i) {
        ofs << "  \"" << sortedOblivious[i] << "\"";
        if (i < sortedOblivious.size() - 1) {
            ofs << ",";
        }
        ofs << "\n";
    }
    ofs << "]\n";
    ofs.close();
    
    std::cout << "Wrote " << outputFileName << "\n";
    
    return 0;
}
