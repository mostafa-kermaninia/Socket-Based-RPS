# Compiler and compiler flags
CXX = g++
CXXFLAGS = -std=c++11  # No warnings

# Targets
.PHONY: all clean

all: clean client.out server.out

# Compile client.cpp into client.out
client.out: client.cpp magic-value-client.hpp
	$(CXX) $(CXXFLAGS) -o client.out client.cpp

# Compile server.cpp into server.out
server.out: server.cpp magic-value-server.hpp
	$(CXX) $(CXXFLAGS) -o server.out server.cpp

# Clean up generated files
clean:
	rm -f client.out server.out
