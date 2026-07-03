#include "ir.h"

// ==========================================
// 函数声明区
// ==========================================
void translate_tree(struct Node* node);
void translate_extdef(struct Node* node);
void translate_compst(struct Node* node);
void translate_deflist(struct Node* deflist_node);
void translate_def(struct Node* def_node);
void translate_declist(struct Node* declist_node);
void translate_dec(struct Node* dec_node);
void translate_stmt(struct Node* node);
void translate_cond(struct Node* exp_node, Operand* label_true, Operand* label_false);
void translate_exp(struct Node* exp_node, Operand* place);
static Operand* translate_array_addr(struct Node* array_exp_node);
static void translate_args(struct Node* args_node);

// ==========================================
// 0. 循环上下文栈：记录当前所在循环的 continue/break 目标 label
//    进入 WHILE 语句翻译时压栈，翻译完循环体后弹栈。
//    break/continue 语句直接查栈顶，不需要额外参数在递归调用间传递。
// ==========================================
#define MAX_LOOP_DEPTH 64
static Operand* continue_label_stack[MAX_LOOP_DEPTH];
static Operand* break_label_stack[MAX_LOOP_DEPTH];
static int loop_stack_top = -1;

static void push_loop_labels(Operand* continue_label, Operand* break_label) {
    if (loop_stack_top + 1 >= MAX_LOOP_DEPTH) return; // 简化：不做溢出报错，超深嵌套极少见
    loop_stack_top++;
    continue_label_stack[loop_stack_top] = continue_label;
    break_label_stack[loop_stack_top] = break_label;
}
static void pop_loop_labels() {
    if (loop_stack_top < 0) return;
    loop_stack_top--;
}

// ==========================================
// 1. 顶层驱动：遍历抽象语法树寻找可翻译的节点
// ==========================================
void translate_tree(struct Node* node) {
    if (!node) return;

    if (strcmp(node->name, "ExtDef") == 0) {
        translate_extdef(node);
        translate_tree(node->brother);
        return;
    }
    
    translate_tree(node->child);
    translate_tree(node->brother);
}

// ==========================================
// 2. 翻译全局定义 (主要捕获函数定义 FunDec)
// ==========================================
void translate_extdef(struct Node* extdef_node) {
    struct Node* fundec = get_child(extdef_node, "FunDec");
    struct Node* compst = get_child(extdef_node, "CompSt");
    
    if (fundec && compst) {
        // 生成 FUNCTION label :
        struct Node* id_node = get_child(fundec, "ID");
        InterCode* f_code = (InterCode*)malloc(sizeof(InterCode));
        f_code->kind = IR_FUNCTION;
        f_code->u.one_op.op = new_variable(id_node->val_str);
        append_code(f_code);

        // 处理函数形参 (PARAM x)
        struct Node* varlist = get_child(fundec, "VarList");
        if (varlist) {
            struct Node* paramdec = get_child(varlist, "ParamDec");
            while (paramdec) {
                struct Node* vardec = get_child(paramdec, "VarDec");
                struct Node* param_id = get_child(vardec, "ID"); // 简化：假设参数是一维变量
                if (param_id) {
                    InterCode* p_code = (InterCode*)malloc(sizeof(InterCode));
                    p_code->kind = IR_PARAM;
                    p_code->u.one_op.op = new_variable(param_id->val_str);
                    append_code(p_code);
                }
                
                struct Node* comma = get_brother(paramdec, "COMMA");
                if (comma) {
                    struct Node* next_varlist = get_brother(comma, "VarList");
                    paramdec = get_child(next_varlist, "ParamDec");
                } else {
                    break;
                }
            }
        }
        // 翻译函数体
        translate_compst(compst);
    }
}

// ==========================================
// 3. 翻译语句块 (CompSt)
// ==========================================
void translate_compst(struct Node* compst_node) {
    // 先处理局部变量/数组声明 (DefList)，再处理语句序列 (StmtList)
    struct Node* deflist = get_child(compst_node, "DefList");
    translate_deflist(deflist);

    struct Node* stmtlist = get_child(compst_node, "StmtList");
    struct Node* stmt = get_child(stmtlist, "Stmt");
    while (stmt) {
        translate_stmt(stmt);
        struct Node* next_stmtlist = get_brother(stmt, "StmtList");
        if (next_stmtlist) stmt = get_child(next_stmtlist, "Stmt");
        else break;
    }
}

// ==========================================
// 3.1 翻译局部声明列表 (DefList -> Def DefList | 空)
//     结构和 StmtList 一致：每个 DefList 节点包含一个 Def 和(可能为空的)下一个 DefList
// ==========================================
void translate_deflist(struct Node* deflist_node) {
    if (!deflist_node) return;
    struct Node* def = get_child(deflist_node, "Def");
    while (def) {
        translate_def(def);
        struct Node* next_deflist = get_brother(def, "DefList");
        if (next_deflist) def = get_child(next_deflist, "Def");
        else break;
    }
}

// ==========================================
// 3.2 翻译单条定义 (Def -> Specifier DecList SEMI)
//     Specifier 本身在当前无类型化 IR 设计下不需要参与翻译，
//     浮点/整型的区分完全由字面量节点自身的名字 (INT/FLOAT) 决定。
// ==========================================
void translate_def(struct Node* def_node) {
    struct Node* declist = get_child(def_node, "DecList");
    translate_declist(declist);
}

// ==========================================
// 3.3 翻译声明列表 (DecList -> Dec | Dec COMMA DecList)
//     结构和 VarList/ParamDec 一致，用 COMMA 做链式遍历
// ==========================================
void translate_declist(struct Node* declist_node) {
    if (!declist_node) return;
    struct Node* dec = get_child(declist_node, "Dec");
    while (dec) {
        translate_dec(dec);
        struct Node* comma = get_brother(dec, "COMMA");
        if (comma) {
            struct Node* next_declist = get_brother(comma, "DecList");
            dec = get_child(next_declist, "Dec");
        } else {
            break;
        }
    }
}

// ==========================================
// 3.4 辅助函数：解析 VarDec，返回基础变量名，
//     并通过 out_size 返回数组总元素个数 (非数组则为 0)。
//     VarDec 是左递归结构: VarDec -> ID | VarDec LB INT RB
//     仅支持维度为常量 INT 的情况 (文法本身也只允许这样)。
// ==========================================
static char* extract_vardec_info(struct Node* vardec_node, int* out_size) {
    struct Node* inner_vardec = get_child(vardec_node, "VarDec");
    if (inner_vardec) {
        // VarDec -> VarDec LB INT RB，说明这一层是数组维度
        struct Node* int_node = get_child(vardec_node, "INT");
        int dim = int_node ? atoi(int_node->val_str) : 0;
        int inner_size = 0;
        char* name = extract_vardec_info(inner_vardec, &inner_size);
        *out_size = (inner_size > 0 ? inner_size : 1) * dim;
        return name;
    } else {
        struct Node* id_node = get_child(vardec_node, "ID");
        *out_size = 0;
        return id_node ? id_node->val_str : NULL;
    }
}

// ==========================================
// 3.5 翻译单个声明 (Dec -> VarDec | VarDec ASSIGNOP Exp)
// ==========================================
void translate_dec(struct Node* dec_node) {
    struct Node* vardec = get_child(dec_node, "VarDec");
    if (!vardec) return;

    int array_elems = 0;
    char* var_name = extract_vardec_info(vardec, &array_elems);
    if (!var_name) return;

    if (array_elems > 0) {
        // 数组：只需要申请栈空间 (简化：每个元素按 4 字节计算，float/int 统一处理)
        // 数组不支持声明时整体初始化，C-- 语义本身也不允许，这里直接忽略可能存在的 ASSIGNOP
        InterCode* dec_code = (InterCode*)malloc(sizeof(InterCode));
        dec_code->kind = IR_DEC;
        dec_code->u.dec.op = new_variable(var_name);
        dec_code->u.dec.size = array_elems * 4;
        append_code(dec_code);
        return;
    }

    // 标量变量：如果带初始化表达式，翻译成一次赋值；否则不生成任何指令
    struct Node* assignop = get_child(dec_node, "ASSIGNOP");
    if (assignop) {
        struct Node* exp = get_brother(assignop, "Exp");
        Operand* var_op = new_variable(var_name);
        translate_exp(exp, var_op);
    }
}

// ==========================================
// 4. 翻译单条语句 (Stmt)
// ==========================================
void translate_stmt(struct Node* stmt_node) {
    if (!stmt_node) return;

    struct Node* first_child = stmt_node->child;
    
    // 1) Stmt -> Exp SEMI
    if (strcmp(first_child->name, "Exp") == 0) {
        translate_exp(first_child, NULL); // 不需要记录结果
    }
    // 2) Stmt -> CompSt
    else if (strcmp(first_child->name, "CompSt") == 0) {
        translate_compst(first_child);
    }
    // 3) Stmt -> RETURN Exp SEMI
    else if (strcmp(first_child->name, "RETURN") == 0) {
        struct Node* exp = get_brother(first_child, "Exp");
        Operand* t1 = new_temp();
        translate_exp(exp, t1);
        
        InterCode* code = (InterCode*)malloc(sizeof(InterCode));
        code->kind = IR_RETURN;
        code->u.one_op.op = t1;
        append_code(code);
    }
    // 4) Stmt -> IF LP Exp RP Stmt [ELSE Stmt]
    else if (strcmp(first_child->name, "IF") == 0) {
        struct Node* exp = get_brother(first_child, "Exp");
        struct Node* stmt1 = get_brother(get_brother(exp, "RP"), "Stmt");
        struct Node* else_node = get_brother(stmt1, "ELSE");

        Operand* label_true = new_label();
        Operand* label_false = new_label();

        if (!else_node) {
            // 没有 ELSE 分支
            translate_cond(exp, label_true, label_false);
            
            // LABEL true :
            InterCode* l_true = (InterCode*)malloc(sizeof(InterCode)); l_true->kind = IR_LABEL; l_true->u.one_op.op = label_true; append_code(l_true);
            translate_stmt(stmt1);
            
            // LABEL false :
            InterCode* l_false = (InterCode*)malloc(sizeof(InterCode)); l_false->kind = IR_LABEL; l_false->u.one_op.op = label_false; append_code(l_false);
        } else {
            // 有 ELSE 分支
            struct Node* stmt2 = get_brother(else_node, "Stmt");
            Operand* label_end = new_label();

            translate_cond(exp, label_true, label_false);
            
            // LABEL true :
            InterCode* l_true = (InterCode*)malloc(sizeof(InterCode)); l_true->kind = IR_LABEL; l_true->u.one_op.op = label_true; append_code(l_true);
            translate_stmt(stmt1);
            
            // GOTO end
            InterCode* g_end = (InterCode*)malloc(sizeof(InterCode)); g_end->kind = IR_GOTO; g_end->u.one_op.op = label_end; append_code(g_end);
            
            // LABEL false :
            InterCode* l_false = (InterCode*)malloc(sizeof(InterCode)); l_false->kind = IR_LABEL; l_false->u.one_op.op = label_false; append_code(l_false);
            translate_stmt(stmt2);
            
            // LABEL end :
            InterCode* l_end = (InterCode*)malloc(sizeof(InterCode)); l_end->kind = IR_LABEL; l_end->u.one_op.op = label_end; append_code(l_end);
        }
    }
    // 5) Stmt -> WHILE LP Exp RP Stmt
    else if (strcmp(first_child->name, "WHILE") == 0) {
        struct Node* exp = get_brother(first_child, "Exp");
        struct Node* stmt1 = get_brother(get_brother(exp, "RP"), "Stmt");

        Operand* label_cond = new_label();
        Operand* label_body = new_label();
        Operand* label_end = new_label();

        // LABEL cond :
        InterCode* l_cond = (InterCode*)malloc(sizeof(InterCode)); l_cond->kind = IR_LABEL; l_cond->u.one_op.op = label_cond; append_code(l_cond);
        
        translate_cond(exp, label_body, label_end);
        
        // LABEL body :
        InterCode* l_body = (InterCode*)malloc(sizeof(InterCode)); l_body->kind = IR_LABEL; l_body->u.one_op.op = label_body; append_code(l_body);
        
        // continue 应跳回条件重新判断处 (label_cond)，break 应跳到循环结束处 (label_end)
        push_loop_labels(label_cond, label_end);
        translate_stmt(stmt1);
        pop_loop_labels();
        
        // GOTO cond
        InterCode* g_cond = (InterCode*)malloc(sizeof(InterCode)); g_cond->kind = IR_GOTO; g_cond->u.one_op.op = label_cond; append_code(g_cond);
        
        // LABEL end :
        InterCode* l_end = (InterCode*)malloc(sizeof(InterCode)); l_end->kind = IR_LABEL; l_end->u.one_op.op = label_end; append_code(l_end);
    }
    // 6) Stmt -> BREAK SEMI
    else if (strcmp(first_child->name, "BREAK") == 0) {
        if (loop_stack_top >= 0) {
            InterCode* g = (InterCode*)malloc(sizeof(InterCode));
            g->kind = IR_GOTO;
            g->u.one_op.op = break_label_stack[loop_stack_top];
            append_code(g);
        }
        // 不在循环内的 break 理论上语义分析阶段就应该报错拦下，这里不做处理
    }
    // 7) Stmt -> CONTINUE SEMI
    else if (strcmp(first_child->name, "CONTINUE") == 0) {
        if (loop_stack_top >= 0) {
            InterCode* g = (InterCode*)malloc(sizeof(InterCode));
            g->kind = IR_GOTO;
            g->u.one_op.op = continue_label_stack[loop_stack_top];
            append_code(g);
        }
    }
}

// ==========================================
// 4.1 辅助函数：计算数组元素 arr[index] 的地址
//     返回值是一个临时变量，里面存放算好的地址值。
//     仅支持一维数组访问，且基址必须是一个纯变量名 (Exp -> ID)。
//     生成序列: t_addr := &arr; t_off := idx * 4; t_addr := t_addr + t_off
// ==========================================
static Operand* translate_array_addr(struct Node* array_exp_node) {
    struct Node* first = array_exp_node->child;   // 数组名对应的 Exp 节点
    struct Node* lb_node = first->brother;         // LB
    struct Node* index_exp = lb_node->brother;     // 真正的下标 Exp

    struct Node* id_node = get_child(first, "ID");
    if (!id_node) return NULL; // 暂不支持多维数组或更复杂的基址表达式

    // t_addr := &arr
    Operand* t_addr = new_temp();
    InterCode* addr_code = (InterCode*)malloc(sizeof(InterCode));
    addr_code->kind = IR_GET_ADDR;
    addr_code->u.assign.left = t_addr;
    addr_code->u.assign.right = new_variable(id_node->val_str);
    append_code(addr_code);

    // t_idx := index
    Operand* t_idx = new_temp();
    translate_exp(index_exp, t_idx);

    // t_off := t_idx * 4
    Operand* t_off = new_temp();
    InterCode* mul_code = (InterCode*)malloc(sizeof(InterCode));
    mul_code->kind = IR_MUL;
    mul_code->u.binop.result = t_off;
    mul_code->u.binop.op1 = t_idx;
    mul_code->u.binop.op2 = new_constant(4);
    append_code(mul_code);

    // t_final := t_addr + t_off
    Operand* t_final = new_temp();
    InterCode* add_code = (InterCode*)malloc(sizeof(InterCode));
    add_code->kind = IR_ADD;
    add_code->u.binop.result = t_final;
    add_code->u.binop.op1 = t_addr;
    add_code->u.binop.op2 = t_off;
    append_code(add_code);

    return t_final;
}

// ==========================================
// 4.2 辅助函数：翻译实参列表 (Args -> Exp | Exp COMMA Args)
//     结构和 VarList/DecList 一致，用 COMMA 做链式遍历。
//     每个实参先算到一个临时变量，再生成一条 ARG 指令，按从左到右的顺序。
// ==========================================
static void translate_args(struct Node* args_node) {
    if (!args_node) return;
    struct Node* exp = get_child(args_node, "Exp");
    while (exp) {
        Operand* t = new_temp();
        translate_exp(exp, t);

        InterCode* arg_code = (InterCode*)malloc(sizeof(InterCode));
        arg_code->kind = IR_ARG;
        arg_code->u.one_op.op = t;
        append_code(arg_code);

        struct Node* comma = get_brother(exp, "COMMA");
        if (comma) {
            struct Node* next_args = get_brother(comma, "Args");
            exp = get_child(next_args, "Exp");
        } else {
            break;
        }
    }
}

// ==========================================
// 5. 翻译表达式 (Exp)
// ==========================================
void translate_exp(struct Node* exp_node, Operand* place) {
    if (!exp_node) return;

    struct Node* first = exp_node->child;

    // 0) Exp -> LP Exp RP (括号，直接透传给内部表达式)
    if (strcmp(first->name, "LP") == 0) {
        struct Node* inner = first->brother;
        translate_exp(inner, place);
    }
    // 1) Exp -> INT | FLOAT
    else if (strcmp(first->name, "INT") == 0 || strcmp(first->name, "FLOAT") == 0) {
        if (place) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            code->kind = IR_ASSIGN;
            code->u.assign.left = place;
            if (strcmp(first->name, "FLOAT") == 0) {
                code->u.assign.right = new_float_constant((float)first->val_float);
            } else {
                code->u.assign.right = new_constant(atoi(first->val_str));
            }
            append_code(code);
        }
    }
    // 2) Exp -> ID
    else if (strcmp(first->name, "ID") == 0 && !first->brother) {
        if (place) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            code->kind = IR_ASSIGN;
            code->u.assign.left = place;
            code->u.assign.right = new_variable(first->val_str);
            append_code(code);
        }
    }
    // 2.1) Exp -> ID LP Args RP | ID LP RP (函数调用)
    //      和上面 "裸变量 ID" 的区别就是 ID 后面紧跟 LP，这里必须放在
    //      裸变量分支之后判断，因为裸变量分支要求 !first->brother，
    //      函数调用天然有 brother (LP)，两者不会冲突。
    else if (strcmp(first->name, "ID") == 0 && first->brother &&
             strcmp(first->brother->name, "LP") == 0) {
        struct Node* args_node = get_child(exp_node, "Args");
        translate_args(args_node); // 无参调用时 args_node 为 NULL，函数直接返回，不生成 ARG

        // IR_CALL 的语义规定必须有 result 操作数用于打印；
        // 如果调用方不关心返回值 (place 为 NULL，比如 "foo();" 单独作为一条语句)，
        // 就分配一个从未被使用的临时变量承接，后端可以直接忽略它。
        Operand* result = place ? place : new_temp();

        InterCode* call_code = (InterCode*)malloc(sizeof(InterCode));
        call_code->kind = IR_CALL;
        call_code->u.call.result = result;
        call_code->u.call.func_name = strdup(first->val_str);
        append_code(call_code);
    }
    // 3) Exp -> Exp ASSIGNOP Exp
    else if (get_child(exp_node, "ASSIGNOP")) {
        struct Node* exp1 = first;
        struct Node* exp2 = get_brother(get_child(exp_node, "ASSIGNOP"), "Exp");

        if (get_child(exp1, "LB")) {
            // 左值是数组元素: arr[i] = exp2
            Operand* addr = translate_array_addr(exp1);
            if (addr) {
                Operand* t_val = new_temp();
                translate_exp(exp2, t_val);

                InterCode* code1 = (InterCode*)malloc(sizeof(InterCode));
                code1->kind = IR_WRITE_ADDR;
                code1->u.assign.left = addr;
                code1->u.assign.right = t_val;
                append_code(code1);

                if (place) {
                    InterCode* code2 = (InterCode*)malloc(sizeof(InterCode));
                    code2->kind = IR_ASSIGN;
                    code2->u.assign.left = place;
                    code2->u.assign.right = t_val;
                    append_code(code2);
                }
            }
        } else {
            // 左值是普通变量
            struct Node* id_node = get_child(exp1, "ID");
            if (id_node) {
                Operand* var_left = new_variable(id_node->val_str);
                Operand* t1 = new_temp();
                translate_exp(exp2, t1); // 计算右边
                
                // var := t1
                InterCode* code1 = (InterCode*)malloc(sizeof(InterCode));
                code1->kind = IR_ASSIGN;
                code1->u.assign.left = var_left;
                code1->u.assign.right = t1;
                append_code(code1);

                // 如果还需要把结果向上层传递：place := var
                if (place) {
                    InterCode* code2 = (InterCode*)malloc(sizeof(InterCode));
                    code2->kind = IR_ASSIGN;
                    code2->u.assign.left = place;
                    code2->u.assign.right = var_left;
                    append_code(code2);
                }
            }
        }
    }
    // 4) Exp -> MINUS Exp (一元负号)
    //    必须放在下面的二元 PLUS/MINUS/STAR/DIV 判断之前！
    //    一元负号 "Exp -> MINUS Exp" 的第一个子节点就是 MINUS 本身，
    //    如果先判断二元分支的 get_child(exp_node, "MINUS")，
    //    会把这个 MINUS token 误判成"存在一个 MINUS 子节点"从而当成二元运算符处理，
    //    进而把 MINUS token 自身当表达式递归下去，导致空指针崩溃。
    else if (strcmp(first->name, "MINUS") == 0) {
        struct Node* exp = first->brother;
        Operand* t1 = new_temp();
        translate_exp(exp, t1);
        if (place) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            code->kind = IR_SUB;
            code->u.binop.result = place;
            code->u.binop.op1 = new_constant(0);
            code->u.binop.op2 = t1;
            append_code(code);
        }
    }
    // 5) Exp -> Exp PLUS/MINUS/STAR/DIV Exp (二元运算符)
    //    走到这里说明 first->name 不是 "MINUS"（已被上面的分支排除），
    //    所以这里匹配到的 MINUS 一定是第二个子节点（真正的二元减号）。
    else if (get_child(exp_node, "PLUS") || get_child(exp_node, "MINUS") || 
             get_child(exp_node, "STAR") || get_child(exp_node, "DIV")) {
        struct Node* op_node = first->brother;
        struct Node* exp2 = op_node->brother;
        
        Operand* t1 = new_temp();
        Operand* t2 = new_temp();
        translate_exp(first, t1);
        translate_exp(exp2, t2);

        if (place) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            if (strcmp(op_node->name, "PLUS") == 0) code->kind = IR_ADD;
            else if (strcmp(op_node->name, "MINUS") == 0) code->kind = IR_SUB;
            else if (strcmp(op_node->name, "STAR") == 0) code->kind = IR_MUL;
            else if (strcmp(op_node->name, "DIV") == 0) code->kind = IR_DIV;
            
            code->u.binop.result = place;
            code->u.binop.op1 = t1;
            code->u.binop.op2 = t2;
            append_code(code);
        }
    }
    // 6) Exp -> Exp LB Exp RB (数组读取)
    else if (get_child(exp_node, "LB")) {
        Operand* addr = translate_array_addr(exp_node);
        if (place && addr) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            code->kind = IR_READ_ADDR;
            code->u.assign.left = place;
            code->u.assign.right = addr;
            append_code(code);
        }
    }
    // 7) 控制流/布尔逻辑 (AND/OR/RELOP/NOT) -> 统一委托给 translate_cond
    //    translate_cond 内部已针对 AND/OR/NOT/RELOP 分别处理，不会再递归回本函数，
    //    所以这里的委托是安全的。
    else if (get_child(exp_node, "AND") || get_child(exp_node, "OR") || 
             get_child(exp_node, "RELOP") || strcmp(first->name, "NOT") == 0) {
        Operand* label_true = new_label();
        Operand* label_false = new_label();
        
        // 初始化 place 为 0
        if (place) {
            InterCode* code_init = (InterCode*)malloc(sizeof(InterCode));
            code_init->kind = IR_ASSIGN; code_init->u.assign.left = place; code_init->u.assign.right = new_constant(0); append_code(code_init);
        }

        translate_cond(exp_node, label_true, label_false);
        
        // LABEL true :
        InterCode* l_true = (InterCode*)malloc(sizeof(InterCode)); l_true->kind = IR_LABEL; l_true->u.one_op.op = label_true; append_code(l_true);
        
        // place := 1
        if (place) {
            InterCode* code_set = (InterCode*)malloc(sizeof(InterCode));
            code_set->kind = IR_ASSIGN; code_set->u.assign.left = place; code_set->u.assign.right = new_constant(1); append_code(code_set);
        }

        // LABEL false :
        InterCode* l_false = (InterCode*)malloc(sizeof(InterCode)); l_false->kind = IR_LABEL; l_false->u.one_op.op = label_false; append_code(l_false);
    }
}

// ==========================================
// 6. 翻译条件表达式 (专门处理控制流短路)
// ==========================================
void translate_cond(struct Node* exp_node, Operand* label_true, Operand* label_false) {
    if (!exp_node) return;
    
    struct Node* first = exp_node->child;

    // 0) Exp -> LP Exp RP (括号，直接透传)
    if (strcmp(first->name, "LP") == 0) {
        struct Node* inner = first->brother;
        translate_cond(inner, label_true, label_false);
    }
    // 1) Exp -> Exp AND Exp  (短路求值：exp1 为假直接跳 false，为真才继续判断 exp2)
    else if (get_child(exp_node, "AND")) {
        struct Node* and_node = get_child(exp_node, "AND");
        struct Node* exp1 = first;
        struct Node* exp2 = and_node->brother;

        Operand* label_mid = new_label();

        translate_cond(exp1, label_mid, label_false);

        // LABEL mid :
        InterCode* l_mid = (InterCode*)malloc(sizeof(InterCode));
        l_mid->kind = IR_LABEL;
        l_mid->u.one_op.op = label_mid;
        append_code(l_mid);

        translate_cond(exp2, label_true, label_false);
    }
    // 2) Exp -> Exp OR Exp  (短路求值：exp1 为真直接跳 true，为假才继续判断 exp2)
    else if (get_child(exp_node, "OR")) {
        struct Node* or_node = get_child(exp_node, "OR");
        struct Node* exp1 = first;
        struct Node* exp2 = or_node->brother;

        Operand* label_mid = new_label();

        translate_cond(exp1, label_true, label_mid);

        // LABEL mid :
        InterCode* l_mid = (InterCode*)malloc(sizeof(InterCode));
        l_mid->kind = IR_LABEL;
        l_mid->u.one_op.op = label_mid;
        append_code(l_mid);

        translate_cond(exp2, label_true, label_false);
    }
    // 3) Exp -> NOT Exp  (直接交换 true/false 标签，不需要额外指令)
    else if (strcmp(first->name, "NOT") == 0) {
        struct Node* exp1 = first->brother;
        translate_cond(exp1, label_false, label_true);
    }
    // 4) Exp -> Exp RELOP Exp
    else if (get_child(exp_node, "RELOP")) {
        struct Node* relop_node = get_child(exp_node, "RELOP");
        struct Node* exp1 = first;
        struct Node* exp2 = relop_node->brother;

        Operand* t1 = new_temp();
        Operand* t2 = new_temp();
        translate_exp(exp1, t1);
        translate_exp(exp2, t2);

        // IF t1 relop t2 GOTO label_true
        InterCode* code_if = (InterCode*)malloc(sizeof(InterCode));
        code_if->kind = IR_IF_GOTO;
        code_if->u.if_goto.x = t1;
        code_if->u.if_goto.y = t2;
        code_if->u.if_goto.relop = strdup(relop_node->val_str);
        code_if->u.if_goto.label = label_true;
        append_code(code_if);

        // GOTO label_false
        InterCode* code_goto = (InterCode*)malloc(sizeof(InterCode));
        code_goto->kind = IR_GOTO;
        code_goto->u.one_op.op = label_false;
        append_code(code_goto);
    }
    // 5) 兜底方案：不是逻辑/比较表达式的普通值，判断其是否 != 0
    //    注意：这里的 exp_node 一定不含 LP/AND/OR/RELOP，也不是 NOT 开头，
    //    所以 translate_exp 不会再委托回 translate_cond，不会产生递归死循环。
    else {
        Operand* t1 = new_temp();
        translate_exp(exp_node, t1);

        InterCode* code_if = (InterCode*)malloc(sizeof(InterCode));
        code_if->kind = IR_IF_GOTO;
        code_if->u.if_goto.x = t1;
        code_if->u.if_goto.y = new_constant(0);
        code_if->u.if_goto.relop = strdup("!=");
        code_if->u.if_goto.label = label_true;
        append_code(code_if);

        InterCode* code_goto = (InterCode*)malloc(sizeof(InterCode));
        code_goto->kind = IR_GOTO;
        code_goto->u.one_op.op = label_false;
        append_code(code_goto);
    }
}