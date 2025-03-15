#include "bitonic_sort.h"
#include <iostream>
#include <random>


int main(int argc, char** argv) {
    //take argument as input data size, only powers of 2
    int INPUT_DATA_SIZE = stoi(argv[1]);

    //generate input data
    vector<int> input_data;
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> dist(1, 1000);
    for (int i = 0; i < INPUT_DATA_SIZE; i++) {
        input_data.emplace_back(dist(rng));
    }

    Server server(input_data, INPUT_DATA_SIZE);
    Client client(&server);

    try {
        client.sort(INPUT_DATA_SIZE, 1);

    }
    catch (const exception &ex) {
        cerr << "Error: " << ex.what() << std::endl;
    }

    return 0;
}
