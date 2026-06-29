#include "tree.h"

// 创建一个新节点
struct Node* create_node(char* name, int line, NodeType type, char* val) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->name = strdup(name);
    node->line = line;
    node->type = type;
    node->child = NULL;
    node->brother = NULL;
    
    if (val != NULL) {
        node->val_str = strdup(val);
        if (strcmp(name, "INT") == 0) node->val_int = atoi(val);
        if (strcmp(name, "FLOAT") == 0) node->val_float = atof(val);
    } else {
        node->val_str = NULL;
    }
    return node;
}

// 把子节点插入到父节点下 (支持多叉树的树左孩子右兄弟表示法)
void insert_child(struct Node* parent, struct Node* child) {
    if (parent == NULL || child == NULL) return;
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        struct Node* cur = parent->child;
        while (cur->brother != NULL) {
            cur = cur->brother;
        }
        cur->brother = child;
    }
}

// 递归打印整棵树，depth 用来控制缩进
void print_tree(struct Node* node, int depth) {
    if (node == NULL) return;

    // 1. 打印缩进，让层次感更强（你可以把两个空格换成 "|- " 看起来更酷）
    for (int i = 0; i < depth; i++) {
        printf("  "); 
    }

    // 2. 根据节点类型智能打印
    if (node->type == NODE_SYNTAX) {
        // 语法非终结符：打印名字和所在行号
        printf("%s (%d)\n", node->name, node->line);
    } 
    else if (node->type == NODE_TOKEN) {
        // 词法终结符：判断是否需要打印具体的值
        if (strcmp(node->name, "ID") == 0 ||
            strcmp(node->name, "TYPE") == 0 ||
            strcmp(node->name, "RELOP") == 0 ||
            strcmp(node->name, "INT") == 0 ||
            strcmp(node->name, "FLOAT") == 0) {
            
            // 打印带具体值的 Token，例如: "TYPE: int"
            printf("%s: %s\n", node->name, node->val_str);
        } else {
            // 其他没有具体值的 Token (比如 IF, WHILE, PLUS, LC)，直接打印名字即可
            printf("%s\n", node->name);
        }
    }

    // 3. 递归打印孩子和兄弟
    print_tree(node->child, depth + 1);
    print_tree(node->brother, depth);
}