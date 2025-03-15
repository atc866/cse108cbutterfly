#include "bitonic_sort.h"
#include <vector>
#include <string>

//compares the values and replaces the values that are different

//Server Methods--
Server::Server(vector<int> data, int input_size) {
    storage = data;
    size = input_size;
}

int Server :: get_value(int index) {
    return storage[index];
}

int Server :: set_value(int index, int value) {
    storage[index] = value;
    return 0;
}

//Client Methods--
Client::Client(Server* new_server) {
  server = new_server;
}

//without encryption
void Client :: comp_values(int i, int j, int dir) {
    int a = decrypt(server->get_value(i));
    int b = decrypt(server->get_value(j));
    if (dir==(a>b)) {
        server->set_value(j, encrypt(a));
        server->set_value(i, encrypt(b));
    }
    else {
        server->set_value(i, encrypt(a));
        server->set_value(j, encrypt(b));
    }
}
 
//Recursively sorts the bitonic sequence
//If dire = 1, sorts ascending, else descending
//low is the starting index, cnt is the # of elements in the section
void Client :: bitonic_merge(int low, int cnt, int dir) {
    if (cnt>1) {
        int k = cnt/2;
        for (int i=low; i<low+k; i++) {
            comp_values(i, i+k, dir);
	}
        bitonic_merge(low, k, dir);
        bitonic_merge(low+k, k, dir);
    }
}
 
//Produces a bitonic sequence by recursively splitting it and sorting it them in opposite sorting orders
//Calls bitonic_merge on each part, sorting them in the determined order
//Low is the starting index, count is the size of each split, dir is the direction of the sort
void Client :: bitonic_sort(int low, int cnt, int dir) {
    if (cnt>1) {
        int k = cnt/2;
        bitonic_sort(low, k, 1);
        bitonic_sort(low+k, k, 0);
        bitonic_merge(low, cnt, dir);
    }
}
 
//calls bitonic sort in ascending order with Length N
//up = 1 is ascending, 0 is descending
void Client :: sort(int N, int up) {
    bitonic_sort(0, N, up);
}

int Client :: encrypt(int data) {
  return data ^ encryption_key;
}

int Client :: decrypt(int data) {
  return data ^ encryption_key;
}


