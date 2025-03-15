#include "bitonic_sort.h"
#include <iostream>
#include <random>


int main() {
    //take argument as input data size, only powers of 2

    //generate input data
    vector<string> input_data = {"1", "2", "3", "4", "5", "6", "7", "8"};

    Server server(input_data, 16);
    Client client(&server);

    try {
        client.sort(8, 1);

    }
    catch (const exception &ex) {
        cerr << "Error: " << ex.what() << std::endl;
    }

    return 0;
}
