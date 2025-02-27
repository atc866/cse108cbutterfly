# Makefile for building the oblivious sort project.

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall

TARGET = oblivious_sort
SRCS = test_osort.cpp oblivious_sort.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp oblivious_sort.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
