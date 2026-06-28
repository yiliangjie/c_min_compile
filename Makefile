CC = gcc
CFLAGS = -Wall -g
SRC_DIR = src
TARGET = compiler

OBJS = $(SRC_DIR)/syntax.tab.o $(SRC_DIR)/lex.yy.o $(SRC_DIR)/tree.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.tab.h: $(SRC_DIR)/syntax.y
	bison -d -o $(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.y

$(SRC_DIR)/lex.yy.c: $(SRC_DIR)/lexical.l $(SRC_DIR)/syntax.tab.h
	flex -o $(SRC_DIR)/lex.yy.c $(SRC_DIR)/lexical.l

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(SRC_DIR)/*.o $(SRC_DIR)/lex.yy.c $(SRC_DIR)/syntax.tab.c $(SRC_DIR)/syntax.tab.h

.PHONY: all clean