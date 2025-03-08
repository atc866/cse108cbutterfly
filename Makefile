# Set SGX_MODE to SIM for simulation mode.
SGX_MODE ?= SIM
SGX_ENCLAVE_DEFINE = -DSGX_ENCLAVE

SGX_SDK ?= /home/vboxuser/Downloads/sgxsdk
SGX_INCLUDE := -I$(SGX_SDK)/include
SGX_LIB := -L$(SGX_SDK)/lib64 -lsgx_urts -lsgx_uae_service

CXX := g++
CXXFLAGS := -std=c++11 -O2 -fPIC $(SGX_INCLUDE)
LDFLAGS := -shared -Wl,-Bsymbolic -fPIC $(SGX_LIB)

all: app enclave.signed.so

# Build enclave object with SGX_ENCLAVE defined.
enclave.o: oblivious_sort.cpp oblivious_sort.h
	$(CXX) $(CXXFLAGS) $(SGX_ENCLAVE_DEFINE) -c oblivious_sort.cpp -o enclave.o

# Link the enclave shared library.
enclave.signed.so: enclave.o
	$(CXX) $(LDFLAGS) enclave.o -o enclave.signed.so

# Build the untrusted application.
app: App.cpp oblivious_sort.cpp oblivious_sort.h
	$(CXX) $(CXXFLAGS) $(SGX_ENCLAVE_DEFINE) App.cpp oblivious_sort.cpp -o app $(SGX_LIB)



clean:
	rm -f *.o enclave.signed.so app
