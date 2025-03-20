# Compiler settings
CXX = g++
CXXFLAGS += -std=c++17 -Wall -O2

# curl - for HTTP requests
# jsoncpp - to parse JSON is added in shell.nix
LDFLAGS += -lcurl -ljsoncpp -v

# Directory settings
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# source, object files and target
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/arbibot

# Building
$(TARGET): $(OBJ_FILES)
	$(CXX) $(OBJ_FILES) -o $(TARGET) $(LDFLAGS)

# Compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/*.o $(TARGET)

run: $(TARGET)
	./$(TARGET)
