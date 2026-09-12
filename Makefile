CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRC_DIR = source
BUILD_DIR = build
TARGET = dbengine

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
LIB_SOURCES = $(filter-out $(SRC_DIR)/main.cpp,$(SOURCES))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

unit_tests: tests/unit_tests.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ tests/unit_tests.cpp $(LIB_SOURCES)

test: $(TARGET) unit_tests
	./unit_tests
	bash tests/run_tests.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET) unit_tests
