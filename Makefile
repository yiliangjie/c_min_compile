CC = gcc
CFLAGS = -Wall -g
SRC_DIR = src
TEST_DIR = test
TARGET = compiler

# 1. 在这里把我们新写的 semantic.o 加进去
OBJS = $(SRC_DIR)/syntax.tab.o $(SRC_DIR)/lex.yy.o $(SRC_DIR)/tree.o $(SRC_DIR)/semantic.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.tab.h: $(SRC_DIR)/syntax.y
	bison -d -o $(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.y

$(SRC_DIR)/lex.yy.c: $(SRC_DIR)/lexical.l $(SRC_DIR)/syntax.tab.h
	flex -o $(SRC_DIR)/lex.yy.c $(SRC_DIR)/lexical.l

# 显式声明头文件依赖，防止修改了 header 后编译不更新
$(SRC_DIR)/semantic.o: $(SRC_DIR)/semantic.c $(SRC_DIR)/semantic.h $(SRC_DIR)/tree.h
$(SRC_DIR)/tree.o: $(SRC_DIR)/tree.c $(SRC_DIR)/tree.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 2. 新增自动化测试命令：一键运行 test 目录下的两个测试文件
test: $(TARGET)
	@echo "======================================"
	@echo "▶ 运行正向测试 (不应输出任何错误):"
	@echo "======================================"
	./$(TARGET) $(TEST_DIR)/test_clean.cmm
	@echo "\n======================================"
	@echo "▶ 运行反向测试 (应当精准报出 6 个错误):"
	@echo "======================================"
	./$(TARGET) $(TEST_DIR)/test_poison.cmm

clean:
	rm -f $(TARGET) $(SRC_DIR)/*.o $(SRC_DIR)/lex.yy.c $(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.tab.h

.PHONY: all clean test