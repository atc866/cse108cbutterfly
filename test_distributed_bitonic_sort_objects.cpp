// main.cpp
#include "oblivious_sort.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <random>
#include <algorithm>
#include <cmath>
#include <cctype>

using namespace std;
using json = nlohmann::json;

// Helper function: Trim whitespace from both ends of a string.
string trim(const string& s) {
    auto start = s.begin();
    while (start != s.end() && isspace(*start))
        start++;
    auto end = s.end();
    do {
        end--;
    } while (distance(start, end) > 0 && isspace(*end));
    return string(start, end + 1);
}

// Helper function: Parse a file formatted as a JSON array of objects.
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

// Helper: Compute next power of two.
size_t nextPowerOfTwo(size_t n) {
    size_t power = 1;
    while (power < n)
        power *= 2;
    return power;
}

// Helper: Merge two sorted vectors (lexicographically, using the 'sorting' field)
// and split into lower and upper halves.
pair<vector<Element>, vector<Element>> distributedMerge(
    const vector<Element>& a,
    const vector<Element>& b)
{
    vector<Element> merged(a.size() + b.size());
    merge(a.begin(), a.end(), b.begin(), b.end(), merged.begin(),
        [](const Element& e1, const Element& e2) {
            return e1.sorting < e2.sorting;
        });
    size_t total = merged.size();
    size_t half = total / 2;
    vector<Element> lower(merged.begin(), merged.begin() + half);
    vector<Element> upper(merged.begin() + half, merged.end());
    return { lower, upper };
}

int main() {
    try {
        // 1. Read and parse the JSON file.
        vector<json> jsonObjects = parseJsonObjects("data.json");
        cout << "Loaded " << jsonObjects.size() << " JSON objects from data.json." << endl;

        // START TEST HERE

        // 2. Convert JSON objects into a vector of Element.
        // Use the "sorting" field for the sorting column and "payload" for the payload.
        vector<Element> inputElements;
        for (const auto& obj : jsonObjects) {
            // You might add additional error checking here.
            int sortVal = obj.at("sorting").get<int>();
            string payload = obj.at("payload").get<string>();
            Element e;
            e.sorting = sortVal;
            // Initially, we set the key to the same as sorting (or you could mix in randomness later).
            e.key = sortVal;
            e.is_dummy = false;
            e.payload = payload;
            inputElements.push_back(e);
        }
        cout << "Converted " << inputElements.size() << " elements from JSON objects." << endl;

        // 3. Partition the data among 4 enclaves.
        const int numEnclaves = 4; // Must be a power of two.
        size_t totalRows = inputElements.size();
        size_t rowsPerEnclave = totalRows / numEnclaves;
        size_t remainder = totalRows % numEnclaves;
        vector<vector<Element>> partitions(numEnclaves);
        size_t index = 0;
        for (int i = 0; i < numEnclaves; i++) {
            size_t count = rowsPerEnclave + (i < remainder ? 1 : 0);
            partitions[i].reserve(count);
            for (size_t j = 0; j < count; j++) {
                partitions[i].push_back(inputElements[index++]);
            }
        }

        // 4. For each partition, pad to next power of two.
        vector<vector<Element>> enclaveData = partitions; // Copy partitions into enclaveData.
        for (int i = 0; i < numEnclaves; i++) {
            size_t origSize = enclaveData[i].size();
            size_t paddedSize = nextPowerOfTwo(origSize);
            if (paddedSize > origSize) {
                for (size_t j = origSize; j < paddedSize; j++) {
                    Element dummy;
                    dummy.sorting = 0;
                    dummy.key = 0;
                    dummy.is_dummy = true;
                    dummy.payload = "";
                    enclaveData[i].push_back(dummy);
                }
            }
        }

        // 5. Create a UntrustedMemory instance and Enclave objects.
        UntrustedMemory dummyUntrusted;
        vector<Enclave> enclaves;
        for (int i = 0; i < numEnclaves; i++) {
            enclaves.emplace_back(&dummyUntrusted);
        }

        // 6. Perform local bitonic sort in each enclave concurrently.
        vector<thread> threads;
        for (int i = 0; i < numEnclaves; i++) {
            threads.push_back(thread([i, &enclaves, &enclaveData]() {
                enclaves[i].bitonicSort(enclaveData[i], 0, enclaveData[i].size(), true);
                cout << "Enclave " << i << " local sort complete. Partition size: " << enclaveData[i].size() << "\n";
                }));
        }
        for (auto& t : threads) {
            t.join();
        }

        // 7. Perform distributed merge rounds.
        int rounds = log2(numEnclaves);
        for (int r = 1; r <= rounds; r++) {
            int step = 1 << (r - 1);
            for (int i = 0; i < numEnclaves; i += 2 * step) {
                for (int j = 0; j < step; j++) {
                    int idx1 = i + j;
                    int idx2 = i + j + step;
                    auto mergedPair = distributedMerge(enclaveData[idx1], enclaveData[idx2]);
                    enclaveData[idx1] = mergedPair.first;
                    enclaveData[idx2] = mergedPair.second;
                    cout << "Distributed merge round " << r << " pairing enclaves "
                        << idx1 << " and " << idx2 << " complete.\n";
                }
            }
        }

        // 8. Concatenate global sorted results (remove dummy elements).
        vector<Element> globalSorted;
        for (int i = 0; i < numEnclaves; i++) {
            for (const auto& e : enclaveData[i]) {
                if (!e.is_dummy)
                    globalSorted.push_back(e);
            }
        }

        // END TEST HERE

        // 9. Do this in the tee or client, should be low cost since it's already partially sorted
        sort(globalSorted.begin(), globalSorted.end(), [](const Element& a, const Element& b) {
            return a.sorting < b.sorting;
            });

        bool isSorted = is_sorted(globalSorted.begin(), globalSorted.end(), [](const Element& a, const Element& b) {
            return a.sorting < b.sorting;
            });
        cout << "Final sorted order verified? " << (isSorted ? "Yes" : "No") << "\n";
        cout << "Total global sorted rows: " << globalSorted.size() << "\n";

        // 10. Write final sorted results to file.
        ofstream ofs("sorted_output_distributed_bitonic.json");
        if (!ofs.is_open()) {
            cerr << "Error: Could not open sorted_output_distributed_bitonic.json for writing\n";
            return 1;
        }
        for (const auto& e : globalSorted) {
            ofs << "{\"sorting\": " << e.sorting << ", \"payload\": \"" << e.payload << "\"}\n";
        }
        ofs.close();
        cout << "Wrote sorted_output_distributed_bitonic.json\n";
    }
    catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
