CC = gcc
CFLAGS = -Wall -g -I./src/common -I./src/semantic -I./src/ir -I./build

SRC_DIR = src
BUILD_DIR = build
TARGET = compiler

# 手动指定现有的各个阶段手写源文件
SRCS = $(SRC_DIR)/syntax/main.c \
       $(SRC_DIR)/common/tree.c \
       $(SRC_DIR)/semantic/semantic.c \
       $(SRC_DIR)/ir/ir.c \
       $(SRC_DIR)/ir/translate.c

# 💥 精准修复：先去掉 src/ 前缀换成 build/，再把 .c 替换为 .o
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS)) \
       $(BUILD_DIR)/syntax.tab.o \
       $(BUILD_DIR)/lex.yy.o

.PHONY: all clean test

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/syntax $(BUILD_DIR)/common $(BUILD_DIR)/semantic $(BUILD_DIR)/ir

$(BUILD_DIR)/syntax.tab.c $(BUILD_DIR)/syntax.tab.h: $(SRC_DIR)/syntax/syntax.y
	bison -d -o $(BUILD_DIR)/syntax.tab.c $(SRC_DIR)/syntax/syntax.y

$(BUILD_DIR)/lex.yy.c: $(SRC_DIR)/lexical/lexical.l $(BUILD_DIR)/syntax.tab.h
	flex -o $(BUILD_DIR)/lex.yy.c $(SRC_DIR)/lexical/lexical.l

$(BUILD_DIR)/syntax.tab.o: $(BUILD_DIR)/syntax.tab.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lex.yy.o: $(BUILD_DIR)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

# 正确匹配 build/.../xxx.o <- src/.../xxx.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR) $(TARGET)