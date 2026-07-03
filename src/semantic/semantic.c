#include "semantic.h"

Type* current_return_type = NULL; 
int current_loop_depth = 0;        
int current_scope_depth = 0; // 新增：全局作用域深度为 0
int semantic_error_count = 0;
FieldList* symbol_table[HASH_SIZE];

// 进入局部作用域
void enter_scope() {
    current_scope_depth++;
}

// 离开局部作用域：清理当前深度的所有变量
void exit_scope() {
    for (int i = 0; i < HASH_SIZE; i++) {
        FieldList* current = symbol_table[i];
        FieldList* prev = NULL;
        
        while (current) {
            if (current->depth == current_scope_depth) {
                // 发现当前作用域的变量，出栈（这里为了轻便，暂不深度 free）
                if (prev == NULL) symbol_table[i] = current->next;
                else prev->next = current->next;
                
                FieldList* temp = current;
                current = current->next;
                free(temp->name);
                free(temp); 
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
    current_scope_depth--;
}

// 修改 insert_symbol，打上深度思想钢印
void insert_symbol(char* name, Type* type) {
    unsigned int index = hash_pjw(name);
    FieldList* new_symbol = (FieldList*)malloc(sizeof(FieldList));
    new_symbol->name = strdup(name);
    new_symbol->type = type;
    new_symbol->depth = current_scope_depth; // 💥 记录当前深度
    // 采用头插法，这样 lookup 时总是先找到最新/最内层的变量 (自动实现 Shadowing)
    new_symbol->next = symbol_table[index];
    symbol_table[index] = new_symbol;
}

// 新增专用于查重的 lookup 函数
FieldList* lookup_symbol_current_scope(char* name) {
    if (!name) return NULL;
    FieldList* current = symbol_table[hash_pjw(name)];
    while (current) {
        // 只找同名且在当前深度的变量
        if (strcmp(current->name, name) == 0 && current->depth == current_scope_depth) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}    

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


int type_equal(Type* t1, Type* t2) {
    if (t1 == t2) return 1;
    if (!t1 || !t2) return 0;
    if (t1->kind != t2->kind) return 0;
    if (t1->kind == BASIC) return t1->u.basic == t2->u.basic;
    if (t1->kind == ARRAY) return type_equal(t1->u.array.elem, t2->u.array.elem);
    // 选做：如果是函数，可以深度对比参数列表，这里简化处理
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
// 处理全局/函数定义 (包含提取函数形参)
// ==========================================
// ==========================================
// 重构完整版：处理全局/函数定义 (全面接入工具与Scope)
// ==========================================
void handle_extdef(struct Node* extdef_node) {
    if (!extdef_node) return;

    // 1. 使用工具函数获取基础类型说明符
    struct Node* specifier = get_child(extdef_node, "Specifier");
    if (!specifier || !specifier->child) return;

    Type* base_type = (Type*)malloc(sizeof(Type));
    base_type->kind = BASIC;
    
    char* type_name = specifier->child->val_str;
    if (strcmp(type_name, "int") == 0) base_type->u.basic = 0;
    else if (strcmp(type_name, "float") == 0) base_type->u.basic = 1;
    else if (strcmp(type_name, "void") == 0) base_type->u.basic = 2; // 处理 void 返回值

    // 2. 检查这是否是一个函数定义 (ExtDef -> Specifier FunDec CompSt)
    struct Node* fundec = get_brother(specifier, "FunDec");
    if (fundec) {
        current_return_type = base_type; // 为后续的 return 语句记录期望的返回值类型
        
        struct Node* id_node = get_child(fundec, "ID");
        if (!id_node) return;
        char* func_name = id_node->val_str;

        // 3. 构建 FUNCTION 类型结构体
        Type* func_type = (Type*)malloc(sizeof(Type));
        func_type->kind = FUNCTION;
        func_type->u.function.ret = base_type;
        func_type->u.function.argc = 0;
        func_type->u.function.params = NULL;

        // 4. 检查函数是否有形参列表 (FunDec -> ID LP VarList RP)
        struct Node* varlist = get_child(fundec, "VarList");
        FieldList* head = NULL;
        FieldList* tail = NULL;

        if (varlist) {
            struct Node* paramdec = get_child(varlist, "ParamDec");
            while (paramdec) {
                struct Node* param_spec = get_child(paramdec, "Specifier");
                if (param_spec && param_spec->child) {
                    Type* param_base = (Type*)malloc(sizeof(Type));
                    param_base->kind = BASIC;
                    param_base->u.basic = (strcmp(param_spec->child->val_str, "int") == 0) ? 0 : 1;

                    // 解析形参的变量名和类型（可能是基础类型或数组）
                    struct Node* vardec = get_child(paramdec, "VarDec");
                    char* param_name = NULL;
                    Type* param_type = parse_vardec(vardec, param_base, &param_name);

                    // 创建形参节点，挂载到函数的参数链表上
                    FieldList* p = (FieldList*)malloc(sizeof(FieldList));
                    p->name = strdup(param_name);
                    p->type = param_type;
                    p->next = NULL;

                    if (!head) { head = p; tail = p; }
                    else { tail->next = p; tail = p; }

                    func_type->u.function.argc++;
                }

                // 寻找下一个形参：VarList -> ParamDec COMMA VarList
                struct Node* comma = get_brother(paramdec, "COMMA");
                if (comma) {
                    struct Node* next_varlist = get_brother(comma, "VarList");
                    paramdec = get_child(next_varlist, "ParamDec");
                } else {
                    break;
                }
            }
            func_type->u.function.params = head;
        }

        // 5. 检查函数名是否在全局（depth = 0）重复定义 (错误类型 4)
        if (lookup_symbol_current_scope(func_name)) {
            printf("Error type 4 at Line %d: Redefined function \"%s\".\n", fundec->line, func_name);
            semantic_error_count++;
        } else {
            insert_symbol(func_name, func_type); // 此时处于全局，depth 自动为 0
        }

        // 6. 核心 Scope 衔接：解析函数体（CompSt）
        struct Node* compst = get_brother(fundec, "CompSt");
        if (compst) {
            enter_scope(); // 进入函数体内层作用域，depth 变为 1
            
            // 关键：把刚才收集到的形参正式注入到函数的局部作用域（depth = 1）
            FieldList* param = func_type->u.function.params;
            while (param) {
                // 检查形参之间是否有重名 (形参同层查重)
                if (lookup_symbol_current_scope(param->name)) {
                    printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", fundec->line, param->name);
                    semantic_error_count++;
                } else {
                    insert_symbol(param->name, param->type); // 自动带上当前的内层 depth
                }
                param = param->next;
            }
            
            // 遍历函数体内部：跳过 CompSt 节点自身（避免重复触发 enter_scope），直接解析其子节点
            analyze_tree(compst->child); 
            
            exit_scope(); // 离开函数体，销毁形参和函数内的全部局部变量
        }
    }
}

void handle_def(struct Node* def_node) {
    if (!def_node) return;
    struct Node* specifier = get_child(def_node, "Specifier"); // 更加优雅
    Type* base_type = (Type*)malloc(sizeof(Type));
    base_type->kind = BASIC;
    base_type->u.basic = (strcmp(specifier->child->val_str, "int") == 0) ? 0 : 1;
    
    struct Node* declist = get_brother(specifier, "DecList");
    struct Node* dec = get_child(declist, "Dec");
    
    while (dec) {
        struct Node* vardec = get_child(dec, "VarDec");
        char* var_name = NULL;
        Type* final_type = parse_vardec(vardec, base_type, &var_name);
        
        // 关键修复：只查当前作用域！这样局部变量覆盖全局变量就不会报错了
        if (lookup_symbol_current_scope(var_name) != NULL) {
            printf("Error type 3 at Line %d: Redefined variable \"%s\".\n", def_node->line, var_name);
            semantic_error_count++;
        } else {
            insert_symbol(var_name, final_type);
        }

        struct Node* assignop = get_brother(vardec, "ASSIGNOP");
        if (assignop) { // 如果有赋值
            struct Node* exp = get_brother(assignop, "Exp");
            Type* exp_type = handle_exp(exp);
            if (exp_type && !type_equal(final_type, exp_type)) {
                printf("Error type 5 at Line %d: Type mismatched for assignment.\n", dec->line);
                semantic_error_count++;

            }
        }
        
        struct Node* comma = get_brother(dec, "COMMA");
        if (comma) dec = get_brother(comma, "DecList")->child;
        else break;
    }
}

void handle_stmt(struct Node* stmt_node) {
    if (!stmt_node || !stmt_node->child) return;
    struct Node* first = stmt_node->child;

    if (strcmp(first->name, "Exp") == 0) handle_exp(first);
    else if (strcmp(first->name, "CompSt") == 0) analyze_tree(first);
    else if (strcmp(first->name, "RETURN") == 0) {
        Type* ret_type = NULL;
        if (first->brother && strcmp(first->brother->name, "Exp") == 0) {
            ret_type = handle_exp(first->brother);
        } else {
            ret_type = (Type*)malloc(sizeof(Type));
            ret_type->kind = BASIC; ret_type->u.basic = 2; // void
        }
        if (current_return_type && ret_type) {
            if (!type_equal(current_return_type, ret_type)) {
                printf("Error type 8 at Line %d: Type mismatched for return.\n", stmt_node->line);
                semantic_error_count++;
            }
        }
    }
    else if (strcmp(first->name, "IF") == 0 || strcmp(first->name, "WHILE") == 0) {
        struct Node* exp_node = first->brother->brother;
        Type* cond_type = handle_exp(exp_node);
        
        // 添加数组作为条件的拦截！
        if (cond_type && cond_type->kind == ARRAY) {
            printf("Error type 6 at Line %d: Condition cannot be an array.\n", exp_node->line);
            semantic_error_count++;
        }
        
        if (strcmp(first->name, "WHILE") == 0) current_loop_depth++;
        
        handle_stmt(exp_node->brother->brother);
        
        if (strcmp(first->name, "WHILE") == 0) current_loop_depth--;
        else if (first->brother->brother->brother->brother) {
            handle_stmt(first->brother->brother->brother->brother->brother);
        }
    }
    else if (strcmp(first->name, "BREAK") == 0 || strcmp(first->name, "CONTINUE") == 0) {
        if (current_loop_depth <= 0) {
            printf("Error at Line %d: Control flow statement not within a loop.\n", stmt_node->line);
            semantic_error_count++;

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
            semantic_error_count++;
            return NULL;
        }
        return sym->type;
    }
    
    // ==========================================
    // 核心补全：处理函数调用 (Exp -> ID LP Args RP 或 ID LP RP)
    // ==========================================
    if (strcmp(c1->name, "ID") == 0 && c1->brother && strcmp(c1->brother->name, "LP") == 0) {
        char* func_name = c1->val_str;
        FieldList* sym = lookup_symbol(func_name);
        
        // 1. 函数未定义 (错误类型 2)
        if (!sym) {
            printf("Error type 2 at Line %d: Undefined function \"%s\".\n", exp_node->line, func_name);
            semantic_error_count++;
            return NULL;
        }
        // 2. 对普通变量使用 () 调用 (错误类型 11)
        if (sym->type->kind != FUNCTION) {
            printf("Error type 11 at Line %d: \"%s\" is not a function.\n", exp_node->line, func_name);
            semantic_error_count++;
            return NULL;
        }

        // 3. 检查参数类型和数量 (错误类型 9)
        struct Node* args_node = NULL;
        if (strcmp(c1->brother->brother->name, "Args") == 0) {
            args_node = c1->brother->brother;
        }

        FieldList* param_ptr = sym->type->u.function.params;
        int arg_count = 0;
        int match = 1;

        while (args_node) {
            struct Node* exp = args_node->child;
            Type* arg_type = handle_exp(exp);
            arg_count++;

            if (param_ptr) {
                if (!type_equal(param_ptr->type, arg_type)) match = 0;
                param_ptr = param_ptr->next;
            }

            if (exp->brother) args_node = exp->brother->brother; // Args -> Exp COMMA Args
            else break;
        }

        if (arg_count != sym->type->u.function.argc || !match) {
            printf("Error type 9 at Line %d: Arguments mismatched for function \"%s\".\n", exp_node->line, func_name);
            semantic_error_count++;
        }

        return sym->type->u.function.ret; // 向上返回该函数的返回值类型
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
                semantic_error_count++;
                return NULL;
            }
            if (index && (index->kind != BASIC || index->u.basic != 0)) {
                printf("Error type 12 at Line %d: Array index is not an integer.\n", exp_node->line);
                semantic_error_count++;
            }
            return base ? base->u.array.elem : NULL;
        }

        Type* left = handle_exp(c1);
        Type* right = handle_exp(op->brother);
        if (!left || !right) return NULL;

        if (strcmp(op->name, "ASSIGNOP") == 0) {
            if (!type_equal(left, right)) {
                printf("Error type 5 at Line %d: Type mismatched for assignment.\n", exp_node->line);
                semantic_error_count++;
            }
            return left;
        }
        
        if (strcmp(op->name, "PLUS") == 0 || strcmp(op->name, "MINUS") == 0 || 
            strcmp(op->name, "STAR") == 0 || strcmp(op->name, "DIV") == 0) {
            if (left->kind == ARRAY || right->kind == ARRAY || left->kind == FUNCTION || right->kind == FUNCTION || !type_equal(left, right)) {
                printf("Error type 7 at Line %d: Type mismatched for operands.\n", exp_node->line);
                semantic_error_count++;
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

// 修改 analyze_tree，让它认识大括号
void analyze_tree(struct Node* node) {
    if (node == NULL) return;

    if (strcmp(node->name, "ExtDef") == 0) {
        handle_extdef(node); 
        analyze_tree(node->brother); 
        return;
    }
    // 拦截大括号，控制 Scope 进出
    else if (strcmp(node->name, "CompSt") == 0) {
        enter_scope();
        analyze_tree(node->child); // 遍历块内的局部变量和语句
        exit_scope();
        analyze_tree(node->brother);
        return;
    }
    else if (strcmp(node->name, "Def") == 0) {
        handle_def(node);    
        analyze_tree(node->brother); 
        return; 
    }
    else if (strcmp(node->name, "Stmt") == 0) {
        handle_stmt(node);   
        analyze_tree(node->brother); 
        return; 
    }
    else if (strcmp(node->name, "Exp") == 0) {
        handle_exp(node);    
        analyze_tree(node->brother);
        return;
    } 
    else {
        analyze_tree(node->child);
    }
    analyze_tree(node->brother);
}