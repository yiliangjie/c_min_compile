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
void print_tree(struct Node* root, int depth) {
    if (root == NULL) return;
    
    // 打印缩进
    for (int i = 0; i < depth; i++) printf("  ");
    
    // 根据节点类型打印不同格式
    if (root->type == NODE_SYNTAX) {
        printf("%s (%d)\n", root->name, root->line);
    } else {
        if (strcmp(root->name, "ID") == 0) printf("ID: %s\n", root->val_str);
        else if (strcmp(root->name, "INT") == 0) printf("INT: %d\n", root->val_int);
        else if (strcmp(root->name, "FLOAT") == 0) printf("FLOAT: %f\n", root->val_float);
        else printf("%s\n", root->name);
    }
    
    // 先遍历孩子（深入下一层），再遍历兄弟（同层遍历）
    print_tree(root->child, depth + 1);
    print_tree(root->brother, depth);
}