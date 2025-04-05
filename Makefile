CXX := g++
CXXFLAGS := -Wall -Wextra -Wno-unused -g -std=c++17
LDFLAGS := -lSDL2

SRCS := $(wildcard *.cc)
OBJS := $(SRCS:.cc=.o)
TARGET := nessie

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean