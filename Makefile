# 编译器和工具定义
CC = gcc
CFLAGS = -Wall -Wno-unused-function -g
LEX = flex
YACC = bison

# 目标文件
TARGET = compiler
SRC_DIR = src

# 伪目标
all: $(TARGET)

$(TARGET): $(SRC_DIR)/lexical.l
	$(LEX) -o $(SRC_DIR)/lex.yy.c $(SRC_DIR)/lexical.l
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_DIR)/lex.yy.c

clean:
	rm -f $(TARGET) $(SRC_DIR)/lex.yy.c

.PHONY: all clean