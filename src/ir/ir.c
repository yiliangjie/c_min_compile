#include "ir.h"

// 初始化全局链表指针
InterCodes* ir_head = NULL;
InterCodes* ir_tail = NULL;

// 计数器计数
static int temp_counter = 0;
static int label_counter = 0;

// 生成一个新的临时变量 (t0, t1, t2...)
Operand* new_temp() {
    Operand* op = (Operand*)malloc(sizeof(Operand));
    op->kind = OP_TEMPORARY;
    op->is_float = 0;
    op->u.no = temp_counter++;
    return op;
}

// 生成一个新的跳转标签 (label0, label1...)
Operand* new_label() {
    Operand* op = (Operand*)malloc(sizeof(Operand));
    op->kind = OP_LABEL;
    op->is_float = 0;
    op->u.no = label_counter++;
    return op;
}

// 生成一个整型常量操作数
Operand* new_constant(int val) {
    Operand* op = (Operand*)malloc(sizeof(Operand));
    op->kind = OP_CONSTANT;
    op->is_float = 0;
    op->u.val = val;
    return op;
}

// 生成一个浮点型常量操作数
Operand* new_float_constant(float val) {
    Operand* op = (Operand*)malloc(sizeof(Operand));
    op->kind = OP_CONSTANT;
    op->is_float = 1;
    op->u.fval = val;
    return op;
}

// 生成一个普通变量操作数
Operand* new_variable(char* name) {
    Operand* op = (Operand*)malloc(sizeof(Operand));
    op->kind = OP_VARIABLE;
    op->is_float = 0;
    op->u.name = strdup(name);
    return op;
}

// 将一条中间代码指令追加到双向链表的末尾
void append_code(InterCode* code) {
    InterCodes* node = (InterCodes*)malloc(sizeof(InterCodes));
    node->code = code;
    node->prev = NULL;
    node->next = NULL;

    if (!ir_head) {
        ir_head = node;
        ir_tail = node;
    } else {
        ir_tail->next = node;
        node->prev = ir_tail;
        ir_tail = node;
    }
}

// 内部辅助函数：打印单个操作数
static void print_operand(FILE* f, Operand* op) {
    if (!op) return;
    switch (op->kind) {
        case OP_VARIABLE:  fprintf(f, "%s", op->u.name); break;
        case OP_CONSTANT:
            if (op->is_float) fprintf(f, "#%g", op->u.fval);
            else fprintf(f, "#%d", op->u.val);
            break;
        case OP_TEMPORARY: fprintf(f, "t%d", op->u.no); break;
        case OP_LABEL:     fprintf(f, "label%d", op->u.no); break;
        case OP_ADDRESS:   fprintf(f, "*t%d", op->u.no); break; // 简化处理
    }
}

// 将整条链表翻译并输出到指定的 IR 文本文件中
void print_ir(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Failed to open IR output file");
        return;
    }

    InterCodes* curr = ir_head;
    while (curr) {
        InterCode* code = curr->code;
        switch (code->kind) {
            case IR_LABEL:
                fprintf(f, "LABEL "); print_operand(f, code->u.one_op.op); fprintf(f, " :\n");
                break;
            case IR_FUNCTION:
                fprintf(f, "FUNCTION %s :\n", code->u.one_op.op->u.name);
                break;
            case IR_ASSIGN:
                print_operand(f, code->u.assign.left); fprintf(f, " := "); print_operand(f, code->u.assign.right); fprintf(f, "\n");
                break;
            case IR_ADD:
                print_operand(f, code->u.binop.result); fprintf(f, " := "); print_operand(f, code->u.binop.op1); fprintf(f, " + "); print_operand(f, code->u.binop.op2); fprintf(f, "\n");
                break;
            case IR_SUB:
                print_operand(f, code->u.binop.result); fprintf(f, " := "); print_operand(f, code->u.binop.op1); fprintf(f, " - "); print_operand(f, code->u.binop.op2); fprintf(f, "\n");
                break;
            case IR_MUL:
                print_operand(f, code->u.binop.result); fprintf(f, " := "); print_operand(f, code->u.binop.op1); fprintf(f, " * "); print_operand(f, code->u.binop.op2); fprintf(f, "\n");
                break;
            case IR_DIV:
                print_operand(f, code->u.binop.result); fprintf(f, " := "); print_operand(f, code->u.binop.op1); fprintf(f, " / "); print_operand(f, code->u.binop.op2); fprintf(f, "\n");
                break;
            case IR_GET_ADDR:
                print_operand(f, code->u.assign.left); fprintf(f, " := &"); print_operand(f, code->u.assign.right); fprintf(f, "\n");
                break;
            case IR_READ_ADDR:
                print_operand(f, code->u.assign.left); fprintf(f, " := *"); print_operand(f, code->u.assign.right); fprintf(f, "\n");
                break;
            case IR_WRITE_ADDR:
                fprintf(f, "*"); print_operand(f, code->u.assign.left); fprintf(f, " := "); print_operand(f, code->u.assign.right); fprintf(f, "\n");
                break;
            case IR_GOTO:
                fprintf(f, "GOTO "); print_operand(f, code->u.one_op.op); fprintf(f, "\n");
                break;
            case IR_IF_GOTO:
                fprintf(f, "IF "); print_operand(f, code->u.if_goto.x); fprintf(f, " %s ", code->u.if_goto.relop); print_operand(f, code->u.if_goto.y); fprintf(f, " GOTO "); print_operand(f, code->u.if_goto.label); fprintf(f, "\n");
                break;
            case IR_RETURN:
                fprintf(f, "RETURN "); print_operand(f, code->u.one_op.op); fprintf(f, "\n");
                break;
            case IR_DEC:
                fprintf(f, "DEC "); print_operand(f, code->u.dec.op); fprintf(f, " %d\n", code->u.dec.size);
                break;
            case IR_ARG:
                fprintf(f, "ARG "); print_operand(f, code->u.one_op.op); fprintf(f, "\n");
                break;
            case IR_CALL:
                print_operand(f, code->u.call.result); fprintf(f, " := CALL %s\n", code->u.call.func_name);
                break;
            case IR_PARAM:
                fprintf(f, "PARAM "); print_operand(f, code->u.one_op.op); fprintf(f, "\n");
                break;
        }
        curr = curr->next;
    }

    fclose(f);
}