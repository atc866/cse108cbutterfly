CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Crypto++ variables (for non-XOR targets)
CRYPTOPP_INCLUDES = -I/opt/homebrew/include
INCLUDES = -I./.include -I. $(CRYPTOPP_INCLUDES)
CRYPTOPP_LIBS = -L/opt/homebrew/lib -lcryptopp

# For XOR versions, no extra library is needed.
XOR_LIBS =

# Crypto++-based targets
SRCS_INT = bucket_sort_string.cpp oblivious_sort_string.cpp
OBJS_INT = $(SRCS_INT:.cpp=.o)
TARGET_INT = bucket_sort_string

SRCS_TWO = bucket_sort_two.cpp oblivious_sort_two.cpp
OBJS_TWO = $(SRCS_TWO:.cpp=.o)
TARGET_TWO = bucket_sort_two

SRCS_SIMPLE = bucket_sort_simple.cpp oblivious_sort_simple.cpp
OBJS_SIMPLE = $(SRCS_SIMPLE:.cpp=.o)
TARGET_SIMPLE = bucket_sort_simple

SRCS_BITONIC = test_bitonic_sort.cpp bitonic_sort.cpp
OBJS_BITONIC = $(SRCS_BITONIC:.cpp=.o)
TARGET_BITONIC = test_bitonic_sort

SRCS_CONST = bucket_sort_constant.cpp oblivious_sort_constant.cpp
OBJS_CONST = $(SRCS_CONST:.cpp=.o)
TARGET_CONST = bucket_sort_constant

SRCS_MERGE = bucket_sort_merge.cpp oblivious_sort_merge.cpp
OBJS_MERGE = $(SRCS_MERGE:.cpp=.o)
TARGET_MERGE = bucket_sort_merge

# XOR-based targets
SRCS_XORTWO = bucket_sort_xortwo.cpp oblivious_sort_xortwo.cpp
OBJS_XORTWO = $(SRCS_XORTWO:.cpp=.o)
TARGET_XORTWO = bucket_sort_xortwo

SRCS_XORMERGE = bucket_sort_xormerge.cpp oblivious_sort_xormerge.cpp
OBJS_XORMERGE = $(SRCS_XORMERGE:.cpp=.o)
TARGET_XORMERGE = bucket_sort_xormerge

SRCS_XORCONST = bucket_sort_xorconstant.cpp oblivious_sort_xorconstant.cpp
OBJS_XORCONST = $(SRCS_XORCONST:.cpp=.o)
TARGET_XORCONST = bucket_sort_xorconstant

all: $(TARGET_INT) $(TARGET_TWO) $(TARGET_SIMPLE) $(TARGET_BITONIC) $(TARGET_CONST) $(TARGET_MERGE) \
     $(TARGET_XORTWO) $(TARGET_XORMERGE) $(TARGET_XORCONST)

$(TARGET_INT): $(OBJS_INT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_INT) $(OBJS_INT) $(CRYPTOPP_LIBS)

$(TARGET_TWO): $(OBJS_TWO)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_TWO) $(OBJS_TWO) $(CRYPTOPP_LIBS)

$(TARGET_SIMPLE): $(OBJS_SIMPLE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_SIMPLE) $(OBJS_SIMPLE) $(CRYPTOPP_LIBS)

$(TARGET_BITONIC): $(OBJS_BITONIC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_BITONIC) $(OBJS_BITONIC) $(CRYPTOPP_LIBS)

$(TARGET_CONST): $(OBJS_CONST)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_CONST) $(OBJS_CONST) $(CRYPTOPP_LIBS)

$(TARGET_MERGE): $(OBJS_MERGE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_MERGE) $(OBJS_MERGE) $(CRYPTOPP_LIBS)

$(TARGET_XORTWO): $(OBJS_XORTWO)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_XORTWO) $(OBJS_XORTWO) $(XOR_LIBS)

$(TARGET_XORMERGE): $(OBJS_XORMERGE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_XORMERGE) $(OBJS_XORMERGE) $(XOR_LIBS)

$(TARGET_XORCONST): $(OBJS_XORCONST)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET_XORCONST) $(OBJS_XORCONST) $(XOR_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS_INT) $(OBJS_TWO) $(OBJS_SIMPLE) $(OBJS_BITONIC) $(OBJS_CONST) $(OBJS_MERGE) \
	      $(OBJS_XORTWO) $(OBJS_XORMERGE) $(OBJS_XORCONST) \
	      $(TARGET_INT) $(TARGET_TWO) $(TARGET_SIMPLE) $(TARGET_BITONIC) $(TARGET_CONST) $(TARGET_MERGE) \
	      $(TARGET_XORTWO) $(TARGET_XORMERGE) $(TARGET_XORCONST)
