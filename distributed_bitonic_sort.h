#ifndef BITONIC_SORT_H
#define BITONIC_SORT_H

using namespace std;

#include <vector>
#include <string>

class Server {
    public:
        //store a pointer to the storage and keep track of the size of the dataset
        int* storage;
        int size;
        Server(int* input, int input_size);
        vector<string> access_log;
        vector<Client*> clients;
        int get_value(int index);
        int set_value(int index, int value);
};

class Client {
    public:
        //let the client access the server
        Server* server;
        int* slice;

        static constexpr int encryption_key = 0xDeadbeef;

        //client constructor to set connected server
        Client(Server* new_server);

        //Simulated encryption, xors the values with the encryption key
        int encrypt(int value);
        int decrypt(int value);

        //run the bitonic sort for an array of N values
        void sort(int N, int up);

    private:
        void comp_values(int i, int j, int dir);
        void bitonic_merge(int low, int cnt, int dir);
        void bitonic_sort(int low, int cnt, int dir);
};

#endif