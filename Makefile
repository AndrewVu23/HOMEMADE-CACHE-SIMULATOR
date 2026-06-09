CXX      := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -Iheader

# Core simulator sources shared by both the sim binary and the test binary
CORE_SRC := src/Cache.cpp src/MainMem.cpp src/ReplacementAlgo.cpp src/Workload.cpp src/Processor.cpp
CORE_OBJ := $(CORE_SRC:.cpp=.o)

# Simulator entry point (contains main())
SIM_SRC  := src/main.cpp
SIM_OBJ  := $(SIM_SRC:.cpp=.o)

# Test sources (doctest supplies its own main())
TEST_SRC := $(wildcard tests/*.cpp)

SIM      := cache_sim
TESTS    := cache_tests

.PHONY: all run test clean

all: $(SIM)

# ---- Simulator ----
$(SIM): $(CORE_OBJ) $(SIM_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJ) $(SIM_OBJ) -o $(SIM)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(SIM)
	./$(SIM)

# ---- Tests (compiled with the vendored doctest header) ----
$(TESTS): $(CORE_OBJ) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -Ithird_party $(CORE_OBJ) $(TEST_SRC) -o $(TESTS)

test: $(TESTS)
	./$(TESTS)

clean:
	rm -f $(CORE_OBJ) $(SIM_OBJ) $(SIM) $(TESTS)
