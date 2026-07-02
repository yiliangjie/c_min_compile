%{
#include "tree.h"
#include "semantic.h"
#include "ir.h"
#include <stdio.h>

void yyerror(const char *s);
int yylex(void);
void yyrestart(FILE* f);

extern int yylineno;
extern int semantic_error_count; // 语义错误计数器
struct Node* root = NULL; // 语法树根节点
%}

%union {
    struct Node* node;
}

/* 终结符声明 */
%token <node> TYPE STRUCT RETURN IF ELSE WHILE BREAK CONTINUE
%token <node> INT FLOAT ID SEMI COMMA ASSIGNOP RELOP
%token <node> PLUS MINUS STAR DIV AND OR DOT NOT
%token <node> LP RP LB RB LC RC

/* 非终结符声明 */
%type <node> Program ExtDefList ExtDef Specifier FunDec VarList ParamDec CompSt StmtList Stmt Exp Args DefList Def DecList Dec VarDec

/* 优先级定义 (解决移进-规约冲突，按从低到高排列) */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT UMINUS
%left LP RP LB RB DOT

%%

/* 1. 顶层结构 */
Program : ExtDefList { 
    if ($1 == NULL) {
        fprintf(stderr, "Warning: empty ExtDefList, no top-level definitions parsed.\n");
        $$ = create_node("Program", yylineno, NODE_SYNTAX, NULL);
    } else {
        $$ = create_node("Program", $1->line, NODE_SYNTAX, NULL); 
        insert_child($$, $1); 
    }
    root = $$;  
    root = $$; 
}
;

ExtDefList : /* 空 */ { $$ = NULL; }
    | ExtDef ExtDefList { 
        $$ = create_node("ExtDefList", $1->line, NODE_SYNTAX, NULL); 
        insert_child($$, $1); insert_child($$, $2); 
    }
    ;

// 函数定义
ExtDef : Specifier FunDec CompSt {
    $$ = create_node("ExtDef", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

// 类型系统
Specifier : TYPE { 
    $$ = create_node("Specifier", $1->line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); 
}
;

// 函数结构
FunDec : ID LP VarList RP {
    $$ = create_node("FunDec", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID LP RP {
    $$ = create_node("FunDec", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

// 参数列表
VarList : ParamDec { $$ = create_node("VarList", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1); }
    | ParamDec COMMA VarList {
        $$ = create_node("VarList", $1->line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
    }
    ;

// 单个参数 ParamDec -> int a, float x
ParamDec : Specifier VarDec {
    $$ = create_node("ParamDec", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
;

// 代码块 (作用域)
CompSt : LC DefList StmtList RC {
    $$ = create_node("CompSt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
;

// 变量定义系统
DefList : /* 空 */ { $$ = NULL; }
    | Def DefList {
        $$ = create_node("DefList", $1->line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2);
    }
    ;
// int a, b
Def : Specifier DecList SEMI {
    $$ = create_node("Def", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

DecList : Dec { $$ = create_node("DecList", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1); }
    | Dec COMMA DecList {
        $$ = create_node("DecList", $1->line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
    }
    ;

// 变量定义加初始化
Dec : VarDec {
    $$ = create_node("Dec", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| VarDec ASSIGNOP Exp {
    $$ = create_node("Dec", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

// a, arr[10]
VarDec : ID {
    $$ = create_node("VarDec", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| VarDec LB INT RB {
    $$ = create_node("VarDec", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
;

/* 4. 语句 (Stmt) */
StmtList : /* 空 */ { $$ = NULL; }
    | Stmt StmtList {
        $$ = create_node("StmtList", $1->line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2);
    }
    ;
// a + b;
Stmt : Exp SEMI {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); insert_child($$, $2);
} // 代码块
| CompSt {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| RETURN Exp SEMI {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| IF LP Exp RP Stmt %prec LOWER_THAN_ELSE {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5);
}
| IF LP Exp RP Stmt ELSE Stmt {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5); insert_child($$, $6); insert_child($$, $7);
}
| WHILE LP Exp RP Stmt {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5);
}
| BREAK SEMI {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| CONTINUE SEMI {
    $$ = create_node("Stmt", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
;

/* 5. 表达式 (Exp) */
Exp : Exp ASSIGNOP Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp AND Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp OR Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp RELOP Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp PLUS Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp MINUS Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp STAR Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp DIV Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| LP Exp RP {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| MINUS Exp %prec UMINUS {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| NOT Exp {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| ID LP Args RP {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID LP RP {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp LB Exp RB {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| INT {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| FLOAT {
    $$ = create_node("Exp", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
;

/* 函数调用参数 */
Args : Exp COMMA Args {
    $$ = create_node("Args", $1->line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp {
    $$ = create_node("Args", $1->line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error type B at Line %d: %s\n", yylineno, s);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            perror(argv[1]);
            return 1;
        }
        yyrestart(f);
    }

    yylineno = 1;
    yyparse();

    if (root != NULL) {
        printf("--- AST ---\n");
        print_tree(root, 0);

        printf("\n--- Semantic Analysis ---\n");

        init_semantic();
        analyze_tree(root);

        if (semantic_error_count > 0) {
            printf("\n🛑 Semantic errors detected. IR generation aborted.\n");
            return 1; 
        }

        printf("\n--- Intermediate Code Generation ---\n");
        printf("Translating AST to IR...\n");


        // 2. 触发顶层驱动，遍历 AST 生成 IR 双向链表
        translate_tree(root); 

        // 3. 决定输出文件名
        // 如果运行时传了第二个参数（如 ./parser input.cmm output.ir），就用它的名字
        // 否则默认输出到当前目录下的 "output.ir"
        const char* ir_output_file = (argc > 2) ? argv[2] : "output.ir";

        // 4. 将链表数据打印到文件中
        print_ir(ir_output_file);
        printf("🎉 IR code successfully generated in '%s'!\n", ir_output_file);

    }

    return 0;
}