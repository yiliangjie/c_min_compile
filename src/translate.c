#include "ir.h"

// ==========================================
// 函数声明区
// ==========================================
void translate_tree(struct Node* node);
void translate_extdef(struct Node* node);
void translate_compst(struct Node* node);
void translate_stmt(struct Node* node);
void translate_cond(struct Node* exp_node, Operand* label_true, Operand* label_false);
void translate_exp(struct Node* exp_node, Operand* place);

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
        
        translate_stmt(stmt1);
        
        // GOTO cond
        InterCode* g_cond = (InterCode*)malloc(sizeof(InterCode)); g_cond->kind = IR_GOTO; g_cond->u.one_op.op = label_cond; append_code(g_cond);
        
        // LABEL end :
        InterCode* l_end = (InterCode*)malloc(sizeof(InterCode)); l_end->kind = IR_LABEL; l_end->u.one_op.op = label_end; append_code(l_end);
    }
}

// ==========================================
// 5. 翻译表达式 (Exp)
// ==========================================
void translate_exp(struct Node* exp_node, Operand* place) {
    if (!exp_node) return;

    struct Node* first = exp_node->child;

    // 1) Exp -> INT
    if (strcmp(first->name, "INT") == 0) {
        if (place) {
            InterCode* code = (InterCode*)malloc(sizeof(InterCode));
            code->kind = IR_ASSIGN;
            code->u.assign.left = place;
            code->u.assign.right = new_constant(atoi(first->val_str));
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
    // 3) Exp -> Exp ASSIGNOP Exp
    else if (get_child(exp_node, "ASSIGNOP")) {
        struct Node* exp1 = first;
        struct Node* exp2 = get_brother(get_child(exp_node, "ASSIGNOP"), "Exp");

        // 左值暂且只考虑 ID
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
    // 4) Exp -> MINUS Exp (负号)
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
    // 5) Exp -> Exp PLUS/MINUS/STAR/DIV Exp
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
    // 6) 控制流/布尔逻辑 (AND/OR/RELOP/NOT) -> 统一委托给 translate_cond
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

    // 1) Exp -> Exp AND Exp  (短路求值：exp1 为假直接跳 false，为真才继续判断 exp2)
    if (get_child(exp_node, "AND")) {
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
    //    注意：这里的 exp_node 一定不含 AND/OR/RELOP，也不是 NOT 开头，
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