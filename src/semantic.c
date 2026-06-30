#include "semantic.h"

FieldList* symbol_table[HASH_SIZE];

// --- 引入上下文状态 ---
Type* current_return_type = NULL; // 记录当前所在函数的返回值类型
int current_loop_depth = 0;       // 记录当前处于几层循环中

void init_semantic() {
    for (int i = 0; i < HASH_SIZE; i++) symbol_table[i] = NULL;
}

unsigned int hash_pjw(char* name) {
    unsigned int val = 0, i;
    for (; *name; ++name) {
        val = (val << 2) + *name;
        if ((i = val & ~0x3FFF)) val = (val ^ (i >> 12)) & 0x3FFF;
    }
    return val % HASH_SIZE;
}

FieldList* lookup_symbol(char* name) {
    if (!name) return NULL;
    FieldList* current = symbol_table[hash_pjw(name)];
    while (current) {
        if (strcmp(current->name, name) == 0) return current;
        current = current->next;
    }
    return NULL;
}

void insert_symbol(char* name, Type* type) {
    unsigned int index = hash_pjw(name);
    FieldList* new_symbol = (FieldList*)malloc(sizeof(FieldList));
    new_symbol->name = strdup(name);
    new_symbol->type = type;
    new_symbol->next = symbol_table[index];
    symbol_table[index] = new_symbol;
}

int type_equal(Type* t1, Type* t2) {
    if (t1 == t2) return 1;
    if (!t1 || !t2) return 0;
    if (t1->kind != t2->kind) return 0;
    if (t1->kind == BASIC) return t1->u.basic == t2->u.basic;
    if (t1->kind == ARRAY) return type_equal(t1->u.array.elem, t2->u.array.elem);
    return 0;
}

Type* parse_vardec(struct Node* vardec, Type* base_type, char** out_name) {
    struct Node* child = vardec->child;
    if (strcmp(child->name, "ID") == 0) {
        *out_name = child->val_str;
        return base_type; 
    } else {
        Type* array_type = (Type*)malloc(sizeof(Type));
        array_type->kind = ARRAY;
        array_type->u.array.size = atoi(child->brother->brother->val_str);
        array_type->u.array.elem = parse_vardec(child, base_type, out_name);
        return array_type;
    }
}

// ==========================================
// 处理全局/函数定义 (拦截函数返回值类型)
// ==========================================
void handle_extdef(struct Node* extdef_node) {
    struct Node* specifier = extdef_node->child;
    Type* base_type = (Type*)malloc(sizeof(Type));
    base_type->kind = BASIC;
    
    char* type_name = specifier->child->val_str;
    if (strcmp(type_name, "int") == 0) base_type->u.basic = 0;
    else if (strcmp(type_name, "float") == 0) base_type->u.basic = 1;
    else if (strcmp(type_name, "void") == 0) base_type->u.basic = 2; // 处理 void

    // 如果是函数定义 (ExtDef -> Specifier FunDec CompSt)
    if (specifier->brother && strcmp(specifier->brother->name, "FunDec") == 0) {
        current_return_type = base_type; // 记录下当前函数的返回值类型，供 return 检查使用
        
        // 我们需要继续解析函数体 (CompSt)，但不要再去解析兄弟节点，避免重复
        analyze_tree(specifier->brother->brother); 
    }
}

void handle_def(struct Node* def_node) {
    if (!def_node) return;
    struct Node* specifier = def_node->child;
    
    Type* base_type = (Type*)malloc(sizeof(Type));
    base_type->kind = BASIC;
    base_type->u.basic = (strcmp(specifier->child->val_str, "int") == 0) ? 0 : 1;

    struct Node* declist = specifier->brother;
    struct Node* dec = declist->child;
    
    while (dec) {
        struct Node* vardec = dec->child;
        char* var_name = NULL;
        Type* final_type = parse_vardec(vardec, base_type, &var_name);
        
        if (lookup_symbol(var_name) != NULL) {
            printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", def_node->line, var_name);
        } else {
            insert_symbol(var_name, final_type);
        }

        if (vardec->brother) { // 检查初始化
            Type* exp_type = handle_exp(vardec->brother->brother);
            if (exp_type && !type_equal(final_type, exp_type)) {
                printf("Error type 5 at Line %d: Type mismatched for assignment.\n", dec->line);
            }
        }

        if (dec->brother) dec = dec->brother->brother->child;
        else break;
    }
}

// ==========================================
// 核心补全：处理控制流语句
// ==========================================
void handle_stmt(struct Node* stmt_node) {
    if (!stmt_node || !stmt_node->child) return;
    struct Node* first = stmt_node->child;

    // 1. Stmt -> Exp SEMI
    if (strcmp(first->name, "Exp") == 0) {
        handle_exp(first);
    }
    // 2. Stmt -> CompSt (嵌套的大括号代码块)
    else if (strcmp(first->name, "CompSt") == 0) {
        analyze_tree(first); // 直接交给总控向下遍历
    }
    // 3. Stmt -> RETURN Exp SEMI
    else if (strcmp(first->name, "RETURN") == 0) {
        Type* ret_type = NULL;
        if (first->brother && strcmp(first->brother->name, "Exp") == 0) {
            ret_type = handle_exp(first->brother);
        } else {
            // 如果是 return; (没有表达式)，按 void 处理
            ret_type = (Type*)malloc(sizeof(Type));
            ret_type->kind = BASIC;
            ret_type->u.basic = 2; // void
        }

        // 检查：返回类型与函数声明是否一致 (错误类型 8)
        if (current_return_type && ret_type) {
            if (!type_equal(current_return_type, ret_type)) {
                printf("Error type 8 at Line %d: Type mismatched for return.\n", stmt_node->line);
            }
        }
    }
    // 4. Stmt -> IF LP Exp RP Stmt (ELSE Stmt)
    else if (strcmp(first->name, "IF") == 0) {
        struct Node* exp_node = first->brother->brother;
        Type* cond_type = handle_exp(exp_node); // 检查条件表达式

        if (cond_type && cond_type->kind == ARRAY) {
            printf("Error type 6 at Line %d: Condition cannot be an array.\n", exp_node->line);
        }
        
        struct Node* true_stmt = exp_node->brother->brother;
        handle_stmt(true_stmt); // 递归解析 if 里面的语句
        
        // 如果有 ELSE 分支
        if (true_stmt->brother && strcmp(true_stmt->brother->name, "ELSE") == 0) {
            handle_stmt(true_stmt->brother->brother); // 解析 else 里面的语句
        }
    }
    // 5. Stmt -> WHILE LP Exp RP Stmt
    else if (strcmp(first->name, "WHILE") == 0) {
        struct Node* exp_node = first->brother->brother;
        Type* cond_type = handle_exp(exp_node); // 检查条件表达式

        if (cond_type && cond_type->kind == ARRAY) {
            printf("Error type 6 at Line %d: Condition cannot be an array.\n", exp_node->line);
        }
        
        current_loop_depth++; // 进入循环体，深度 +1
        handle_stmt(exp_node->brother->brother);
        current_loop_depth--; // 离开循环体，深度 -1
    }
    // 6. Stmt -> BREAK SEMI
    else if (strcmp(first->name, "BREAK") == 0) {
        if (current_loop_depth <= 0) {
            // 选做/非标错误：不在循环里使用了 break
            printf("Error at Line %d: 'break' statement not within a loop.\n", stmt_node->line);
        }
    }
    // 7. Stmt -> CONTINUE SEMI
    else if (strcmp(first->name, "CONTINUE") == 0) {
        if (current_loop_depth <= 0) {
            // 选做/非标错误：不在循环里使用了 continue
            printf("Error at Line %d: 'continue' statement not within a loop.\n", stmt_node->line);
        }
    }
}

Type* handle_exp(struct Node* exp_node) {
    if (!exp_node || !exp_node->child) return NULL;
    struct Node* c1 = exp_node->child;

    if (strcmp(c1->name, "INT") == 0) {
        Type* t = (Type*)malloc(sizeof(Type)); t->kind = BASIC; t->u.basic = 0; return t;
    }
    if (strcmp(c1->name, "FLOAT") == 0) {
        Type* t = (Type*)malloc(sizeof(Type)); t->kind = BASIC; t->u.basic = 1; return t;
    }
    if (strcmp(c1->name, "ID") == 0 && !c1->brother) {
        FieldList* sym = lookup_symbol(c1->val_str);
        if (!sym) {
            printf("Error type 1 at Line %d: Undefined variable \"%s\".\n", exp_node->line, c1->val_str);
            return NULL;
        }
        return sym->type;
    }
    if (strcmp(c1->name, "LP") == 0) return handle_exp(c1->brother);
    if (strcmp(c1->name, "MINUS") == 0 || strcmp(c1->name, "NOT") == 0) return handle_exp(c1->brother);

    if (c1->brother) {
        struct Node* op = c1->brother;
        
        if (strcmp(op->name, "LB") == 0) {
            Type* base = handle_exp(c1);
            Type* index = handle_exp(op->brother);
            
            if (base && base->kind != ARRAY) {
                printf("Error type 10 at Line %d: Variable is not an array.\n", exp_node->line);
                return NULL;
            }
            if (index && (index->kind != BASIC || index->u.basic != 0)) {
                printf("Error type 12 at Line %d: Array index is not an integer.\n", exp_node->line);
            }
            return base ? base->u.array.elem : NULL;
        }

        Type* left = handle_exp(c1);
        Type* right = handle_exp(op->brother);
        if (!left || !right) return NULL;

        if (strcmp(op->name, "ASSIGNOP") == 0) {
            if (!type_equal(left, right)) {
                printf("Error type 5 at Line %d: Type mismatched for assignment.\n", exp_node->line);
            }
            return left;
        }
        
        if (strcmp(op->name, "PLUS") == 0 || strcmp(op->name, "MINUS") == 0 || 
            strcmp(op->name, "STAR") == 0 || strcmp(op->name, "DIV") == 0) {
            if (left->kind == ARRAY || right->kind == ARRAY || !type_equal(left, right)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", exp_node->line);
                return NULL;
            }
            return left;
        }

        if (strcmp(op->name, "AND") == 0 || strcmp(op->name, "OR") == 0 || strcmp(op->name, "RELOP") == 0) {
            Type* ret = (Type*)malloc(sizeof(Type));
            ret->kind = BASIC; ret->u.basic = 0; 
            return ret;
        }
    }
    return NULL;
}

// 主遍历入口优化：全面接管所有核心节点
void analyze_tree(struct Node* node) {
    if (node == NULL) return;

    if (strcmp(node->name, "ExtDef") == 0) {
        handle_extdef(node); // 处理全局/函数定义
        analyze_tree(node->brother); 
        return;
    }
    else if (strcmp(node->name, "Def") == 0) {
        handle_def(node);    // 处理局部变量定义
        analyze_tree(node->brother); 
        return; 
    }
    else if (strcmp(node->name, "Stmt") == 0) {
        handle_stmt(node);   // 处理控制流语句
        analyze_tree(node->brother); 
        return; 
    }
    else if (strcmp(node->name, "Exp") == 0) {
        handle_exp(node);    // 处理裸表达式
        analyze_tree(node->brother);
        return;
    } 
    else {
        analyze_tree(node->child);
    }
    analyze_tree(node->brother);
}