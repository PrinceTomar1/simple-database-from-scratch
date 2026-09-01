CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRC_DIR = source
BUILD_DIR = build
TARGET = dbengine

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) unit_tests
