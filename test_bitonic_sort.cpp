#include "bitonic_sort.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>

using json = nlohmann::json;

vector<json> parseJsonObjects(const string& filename) {
    ifstream ifs(filename);
    if (!ifs.is_open())
        throw runtime_error("Could not open " + filename);
    json j;
    ifs >> j;
    if (!j.is_array())
        throw runtime_error("Expected a JSON array in " + filename);
    vector<json> objects;
    for (const auto& obj : j)
        objects.push_back(obj);
    return objects;
}

int main(int argc, char** argv) {
    //take arguments 1 of file name, 2 as input size
    
    //generate input data
    vector<json> jsonObjects = parseJsonObjects(argv[1]);
    vector<Element> input_data;

    for (const auto& obj : jsonObjects) {
            // You might add additional error checking here.
            int sortVal = obj.at("sorting").get<int>();
            string payload = obj.at("payload").get<string>();
            Element e;
            e.sorting = sortVal;
            // Initially, we set the key to the same as sorting (or you could mix in randomness later).
            e.index = sortVal;
            e.payload = payload;
            input_data.push_back(e);
        }

    int input_size = stoi(argv[2]);
    

    Server server(input_data, input_size);
    Client client(&server);

    try {
	auto start_time = std::chrono::high_resolution_clock::now();
	client.sort(input_size, 1);
    	auto end_time = std::chrono::high_resolution_clock::now();
    	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        cout << duration.count() / 1000 << endl;

    }
    catch (const exception &ex) {
        cerr << "Error: " << ex.what() << std::endl;
    }

    return 0;
}
