# Makefile for building the oblivious sort project.

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall

TARGET = oblivious_sort
SRCS = test_osort.cpp oblivious_sort.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET) bitonic_sort

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

bitonic_sort: bitonic_sort.o test_bitonic_sort.o
	$(CXX) $(CXXFLAGS) -o bitonic_sort bitonic_sort.o test_bitonic_sort.o

bitonic_sort.o: bitonic_sort.cpp bitonic_sort.h
	$(CXX) $(CXXFLAGS) -c $<

test_bitonic_sort.o: test_bitonic_sort.cpp bitonic_sort.h
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.cpp oblivious_sort.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
