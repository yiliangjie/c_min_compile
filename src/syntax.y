%{
#include "tree.h"
#include <stdio.h>

void yyerror(const char *s);
int yylex(void);

struct Node* root = NULL; // 语法树的根节点
%}

/* 声明用到的联合体类型，也就是节点在 Bison 内部的代号 */
%union {
    struct Node* node;
}

/* 声明所有从 Flex 传过来的终结符 (Token) */
%token <node> TYPE STRUCT RETURN IF ELSE WHILE
%token <node> INT FLOAT ID SEMI COMMA ASSIGNOP RELOP
%token <node> PLUS MINUS STAR DIV AND OR DOT NOT
%token <node> LP RP LB RB LC RC

/* 声明非终结符的类型 */
%type <node> Program ExtDefList ExtDef Specifier FunDec VarList ParamDec CompSt StmtList Stmt Exp Args

/* 优先级定义：解决加减乘除以及赋值的冲突 */
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT

%%

/* 1. 整个程序的入口 */
Program : ExtDefList { 
    $$ = create_node("Program", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); 
    root = $$; // 挂载根节点
}
;

ExtDefList : /* 空 */ { $$ = NULL; }
    | ExtDef ExtDefList { 
        $$ = create_node("ExtDefList", @$.first_line, NODE_SYNTAX, NULL); 
        insert_child($$, $1); insert_child($$, $2); 
    }
    ;

/* 2. 外部定义 (这里最基础的：函数定义) */
ExtDef : Specifier FunDec CompSt {
    $$ = create_node("ExtDef", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

Specifier : TYPE { 
    $$ = create_node("Specifier", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); 
}
;

/* 3. 函数头与参数 */
FunDec : ID LP VarList RP {
    $$ = create_node("FunDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $3);
}
| ID LP RP {
    $$ = create_node("FunDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1);
}
;

VarList : ParamDec { $$ = $1; }
    | ParamDec COMMA VarList {
        $$ = create_node("VarList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $3);
    }
    ;

ParamDec : Specifier ID {
    $$ = create_node("ParamDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
;

/* 4. 复合语句与常规语句 */
CompSt : LC StmtList RC {
    $$ = create_node("CompSt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $2);
}
;

StmtList : /* 空 */ { $$ = NULL; }
    | Stmt StmtList {
        $$ = create_node("StmtList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2);
    }
    ;

Stmt : Exp SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| RETURN Exp SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); insert_child($$, $2);
}
;

/* 5. 表达式 (核心计算逻辑) */
Exp : Exp ASSIGNOP Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp PLUS Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp STAR Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| ID {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| INT {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error type B at Line %d: %s\n", yylineno, s);
}

// 主函数：读取文件，调用解析，打印树
int main(int argc, char** argv) {
    if (argc > 1) {
        FILE* f = fopen(argv[1], "r");
        if (!f) { perror(argv[1]); return 1; }
        yyrestart(f);
    }
    yylineno = 1;
    yyparse(); // 启动 Bison 语法分析
    if (root != NULL) {
        printf("--- 抽象语法树 (AST) ---\n");
        print_tree(root, 0);
    }
    return 0;
}