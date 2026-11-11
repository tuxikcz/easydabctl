CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c++17
LDLIBS ?= -lcurl
.PHONY: all clean test
all: easydabctl
easydabctl: easydabctl.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)
clean:
	rm -f easydabctl
test: easydabctl
	./tests/test_mock.sh
