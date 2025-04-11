CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./trpc
LDFLAGS = -lpthread

# 源文件目录
SRC_DIR = ./trpc
EXAMPLE_DIR = ./example

# 目标文件目录
OBJ_DIR = build/obj
BIN_DIR = build/bin

# 源文件
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
EXAMPLE_SRCS = $(wildcard $(EXAMPLE_DIR)/*.cpp)

# 目标文件
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
EXAMPLE_OBJS = $(patsubst $(EXAMPLE_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(EXAMPLE_SRCS))

# 可执行文件
SERVER_TARGET = $(BIN_DIR)/server
CLIENT_TARGET = $(BIN_DIR)/client

# 默认目标
all: server client

# 创建必要的目录
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# 编译规则
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(EXAMPLE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 链接规则
server: $(OBJS) $(OBJ_DIR)/server.o
	$(CXX) $^ -o $(SERVER_TARGET) $(LDFLAGS)

client: $(OBJS) $(OBJ_DIR)/test_add.o
	$(CXX) $^ -o $(CLIENT_TARGET) $(LDFLAGS)

# 清理规则
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all server client clean
