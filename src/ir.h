#ifndef IR_H
#define IR_H

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 1. 操作数类型 ---
typedef struct Operand_ {
    enum { 
        OP_VARIABLE,    // 普通用户变量 (如 x, y)
        OP_CONSTANT,    // 整数常量 (如 #5)
        OP_TEMPORARY,   // 编译器生成的临时变量 (如 t0, t1)
        OP_LABEL,       // 跳转标签 (如 label0, label1)
        OP_ADDRESS,     // 地址/指针变量 (用于数组和结构体传参或访问)
    } kind;
    union {
        char* name;     // 变量名
        int val;        // 常数值
        int no;         // 临时变量或标签的编号
    } u;
} Operand;

// --- 2. 中间代码指令种类 ---
typedef struct InterCode_ {
    enum {
        IR_LABEL,       // LABEL x :
        IR_FUNCTION,    // FUNCTION f :
        IR_ASSIGN,      // x := y
        IR_ADD,         // x := y + z
        IR_SUB,         // x := y - z
        IR_MUL,         // x := y * z
        IR_DIV,         // x := y / z
        IR_GET_ADDR,    // x := &y
        IR_READ_ADDR,   // x := *y
        IR_WRITE_ADDR,  // *x := y
        IR_GOTO,        // GOTO x
        IR_IF_GOTO,     // IF x [relop] y GOTO z
        IR_RETURN,      // RETURN x
        IR_DEC,         // DEC x size (内存申请，用于局部数组)
        IR_ARG,         // ARG x (传实参)
        IR_CALL,        // x := CALL f (函数调用)
        IR_PARAM        // PARAM x (函数接收形参)
    } kind;
    union {
        struct { Operand* op; } one_op;                         // LABEL, GOTO, RETURN, ARG, PARAM, FUNCTION
        struct { Operand* left; Operand* right; } assign;       // ASSIGN, GET_ADDR, READ_ADDR, WRITE_ADDR
        struct { Operand* result; Operand* op1; Operand* op2; } binop; // ADD, SUB, MUL, DIV
        struct { Operand* x; Operand* y; Operand* label; char* relop; } if_goto; // IF_GOTO
        struct { Operand* op; int size; } dec;                  // DEC
        struct { Operand* result; char* func_name; } call;      // CALL
    } u;
} InterCode;

// --- 3. 双向链表节点 ---
typedef struct InterCodes_ {
    InterCode* code;
    struct InterCodes_* prev;
    struct InterCodes_* next;
} InterCodes;

// --- 4. 全局链表头尾指针 ---
extern InterCodes* ir_head;
extern InterCodes* ir_tail;

// --- 5. 核心辅助函数声明 ---
Operand* new_temp();
Operand* new_label();
Operand* new_constant(int val);
Operand* new_variable(char* name);

void append_code(InterCode* code);
void print_ir(const char* filename);

#endif