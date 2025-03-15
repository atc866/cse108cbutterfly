#include "bitonic_sort.h"
#include <iostream>
#include <random>


int main(int argc, char** argv) {
    //take argument as input data size, only powers of 2

    //generate input data
    vector<string> input_data;
    int input_size

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
