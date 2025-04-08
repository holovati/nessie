CXX := g++
CXXFLAGS := -Wall -Wextra -Wno-unused -g -std=c++17 -I.
LDFLAGS := -lSDL2

SRCS := $(wildcard *.cc) $(wildcard mapper/*.cc)
OBJS := $(SRCS:.cc=.o)
TARGET := nessie

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) mapper/*.o $(TARGET)

.PHONY: all clean