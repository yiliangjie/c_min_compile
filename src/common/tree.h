#ifndef TREE_H
#define TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 定义树节点的类型：是终结符（Flex抓到的词）还是非终结符（Bison拼出来的语法）
typedef enum { NODE_TOKEN, NODE_SYNTAX } NodeType;

typedef struct Node {
    char* name;               // 节点名 (如 "INT", "Exp", "Stmt")
    char* val_str;            // 存放字符串值 (如 ID 的名字 "a")
    int val_int;              // 存放整数值
    double val_float;         // 存放浮点数值
    int line;                 // 行号
    NodeType type;            // 节点类型
    
    struct Node* child;       // 左孩子 (第一个子节点)
    struct Node* brother;     // 右兄弟 (同级下一个节点)
} Node;

// 函数声明
struct Node* create_node(char* name, int line, NodeType type, char* val);
void insert_child(struct Node* parent, struct Node* child);
void print_tree(struct Node* root, int depth);
// --- AST 辅助访问工具 ---
struct Node* get_child(struct Node* node, const char* name);
struct Node* get_brother(struct Node* node, const char* name);

#endif