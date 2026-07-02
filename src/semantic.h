#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 1. 强化的类型系统 ---
typedef struct Type_ {
    enum { BASIC, ARRAY, FUNCTION } kind;
    union {
        int basic; // 0 代表 int, 1 代表 float, 2 代表 void (新增)
        struct { 
            struct Type_* elem; 
            int size;           
        } array;
        struct {
            struct Type_* ret;         // 返回值类型
            int argc;                  // 参数个数
            struct FieldList_* params; // 形参列表 (链表)
        } function;
    } u;
} Type;

// --- 2. 符号表项 ---
typedef struct FieldList_ {
    char* name;
    Type* type;
    int depth;
    struct FieldList_* next;
} FieldList;

#define HASH_SIZE 1024



// --- 3. 核心函数声明 ---
void init_semantic();
void analyze_tree(struct Node* node);

unsigned int hash_pjw(char* name);
void insert_symbol(char* name, Type* type);
FieldList* lookup_symbol(char* name);
FieldList* lookup_symbol_current_scope(char* name);


// 类型辅助函数
int type_equal(Type* t1, Type* t2);
Type* parse_vardec(struct Node* vardec, Type* base_type, char** out_name);

// 节点处理函数
void handle_extdef(struct Node* extdef_node);
void handle_def(struct Node* def_node);
void handle_stmt(struct Node* stmt_node);
Type* handle_exp(struct Node* exp_node);

void enter_scope();
void exit_scope();


#endif