%{
#include "tree.h"
#include <stdio.h>

void yyerror(const char *s);
int yylex(void);
void yyrestart(FILE* f);

extern int yylineno;
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
    $$ = create_node("Program", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); 
    root = $$; 
}
;

ExtDefList : /* 空 */ { $$ = NULL; }
    | ExtDef ExtDefList { 
        $$ = create_node("ExtDefList", @$.first_line, NODE_SYNTAX, NULL); 
        insert_child($$, $1); insert_child($$, $2); 
    }
    ;

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

/* 2. 函数与参数 */
FunDec : ID LP VarList RP {
    $$ = create_node("FunDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID LP RP {
    $$ = create_node("FunDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

VarList : ParamDec { $$ = create_node("VarList", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1); }
    | ParamDec COMMA VarList {
        $$ = create_node("VarList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
    }
    ;

ParamDec : Specifier VarDec {
    $$ = create_node("ParamDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
;

/* 3. 复合语句与局部定义 (支持数组和普通变量) */
CompSt : LC DefList StmtList RC {
    $$ = create_node("CompSt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
;

DefList : /* 空 */ { $$ = NULL; }
    | Def DefList {
        $$ = create_node("DefList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2);
    }
    ;

Def : Specifier DecList SEMI {
    $$ = create_node("Def", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

DecList : Dec { $$ = create_node("DecList", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1); }
    | Dec COMMA DecList {
        $$ = create_node("DecList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
    }
    ;

Dec : VarDec {
    $$ = create_node("Dec", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| VarDec ASSIGNOP Exp {
    $$ = create_node("Dec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
;

VarDec : ID {
    $$ = create_node("VarDec", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| VarDec LB INT RB {
    $$ = create_node("VarDec", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
;

/* 4. 语句 (Stmt) */
StmtList : /* 空 */ { $$ = NULL; }
    | Stmt StmtList {
        $$ = create_node("StmtList", @$.first_line, NODE_SYNTAX, NULL);
        insert_child($$, $1); insert_child($$, $2);
    }
    ;

Stmt : Exp SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); insert_child($$, $2);
}
| CompSt {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| RETURN Exp SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL); 
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| IF LP Exp RP Stmt %prec LOWER_THAN_ELSE {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5);
}
| IF LP Exp RP Stmt ELSE Stmt {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5); insert_child($$, $6); insert_child($$, $7);
}
| WHILE LP Exp RP Stmt {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4); insert_child($$, $5);
}
| BREAK SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| CONTINUE SEMI {
    $$ = create_node("Stmt", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
;

/* 5. 表达式 (Exp) */
Exp : Exp ASSIGNOP Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp AND Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp OR Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp RELOP Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp PLUS Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp MINUS Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp STAR Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp DIV Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| LP Exp RP {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| MINUS Exp %prec UMINUS {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| NOT Exp {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2);
}
| ID LP Args RP {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID LP RP {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp LB Exp RB {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3); insert_child($$, $4);
}
| ID {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| INT {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
| FLOAT {
    $$ = create_node("Exp", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
;

/* 函数调用参数 */
Args : Exp COMMA Args {
    $$ = create_node("Args", @$.first_line, NODE_SYNTAX, NULL);
    insert_child($$, $1); insert_child($$, $2); insert_child($$, $3);
}
| Exp {
    $$ = create_node("Args", @$.first_line, NODE_SYNTAX, NULL); insert_child($$, $1);
}
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error type B at Line %d: %s\n", yylineno, s);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        FILE* f = fopen(argv[1], "r");
        if (!f) { perror(argv[1]); return 1; }
        yyrestart(f);
    }
    yylineno = 1;
    yyparse(); 
    if (root != NULL) {
        printf("--- 抽象语法树 (AST) ---\n");
        print_tree(root, 0);
    }
    return 0;
}