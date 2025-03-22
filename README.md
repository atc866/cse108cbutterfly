bitonic_sort.cpp/h->bitonic sort
bitonic_sort.py bitonic sort in python
bucket_sort_constant.cpp->test oblivious_sort_constant by reading in json file with two column format
bucket_sort_merge.cpp->test oblivious_sort_merge by reading in json file with two column format
bucket_sort_simple->test oblivious_sort_simple
bucket_sort_string.cpp->test oblivious_sort_string with string data
bucket_sort_two->test butterfly bitonic sort by reading in json file with two column format
bucket_sort_xor(constant/merge/two).cpp->test but with xor encryption (simple)

data.json->10^6 elements with 4 byte payload
data0.json->10^6 elements with 4 byte payload
data1.json->2^22 elements with 5 byte payload
data2.json->2^22 elements with 10 byte payload
data3.json->2^22 elements with 25 byte payload
data4.json->2^22 elements with 50 byte payload
data5.json->2^22 elements with 100 byte payload

enclave_sim.py->python implementation with enclave classes
gen_test_data.py->generate string data
generate_json.py->generate json data with two column (can specify how many elements, payload size, file name to write to)

oblivious_sort_constant.cpp/h->butterfly network with bitonic sort with constant storage (user can specify through variable WORKING_SIZE)

oblivious_sort_merge.cpp/h->butterfly network with merge split

oblivious_sort_simple.cpp/h->simple oblivious sort

oblivious_sort_two.cpp/h->butterfly network with bitonic sort 2Z client storage where Z is bucket size

oblivious_sort.cpp/h->can ignore

test_bitonic_sort.cpp-> used to test bitonic sort

test_distributed_bitonic_sort_objects/string.cpp->test distributed bitonic sort with payload/string data

test_osort.cpp->test oblivious_sort.cpp

Makefile-> create executeables for bucket_sort_two/xortwo/merge/xormerge/constant/xorconstant/string/simple


RECREATING TEST
run the makefile, (have to make different for distributed test) to create executables
use gen_test_data.py to make json that is string array
use generate_json.py to make json that has two column array (sorting column, payload column)
use the executetable for the test and add the correctly formatted file as an arguement in command line

FOR CRYPTOGRAPHIC OPERATIONS/CRYPTOPP->NEED TO HAVE CRYPTOPP INSTALLED AND WILL NEED TO CHANGE MAKEFILE TO SUPPORT THIS
JSON LIBRARY SHOULD ALREADY BE IN GITHUB
->CRYPTOPP_INCLUDES = -I/opt/homebrew/include
