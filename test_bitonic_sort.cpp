#include "bitonic_sort.h"
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>

int main(int argc, char** argv) {
    //take arguments 1 of file name, 2 as input size

    //generate input data
    vector<string> input_data;
    int input_size = stoi(argv[2]);
    ifstream file(argv[1]); 
    string op;
    while(getline(file, op, ',').good()) {
        input_data.emplace_back(op);
    }

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
