#ifndef BITONIC_SORT_H
#define BITONIC_SORT_H

using namespace std;

#include <vector>
#include <string>

struct Element {
  int index;
  string payload;
};

class Server {
    public:
        //store a pointer to the storage and keep track of the size of the dataset
        vector<Element> storage;
        Server(vector<Element> input);
        Element get_value(int index);
        int set_value(int index, Element value);
};

class Client {
    public:
        //let the client access the server
        Server* server;

        string encryption_key = "Encryption_KeyEncryption_Key";

        //client constructor to set connected server
        Client(Server* new_server);

        //Simulated encryption, xors the values with the encryption key
        string encrypt(string value);
        string decrypt(string value);

        //run the bitonic sort for an array of N values
        void sort(int N, int up);

    private:
        void comp_values(int i, int j, int dir);
        void bitonic_merge(int low, int cnt, int dir);
        void bitonic_sort(int low, int cnt, int dir);
};

#endif
