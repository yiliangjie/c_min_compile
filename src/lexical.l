
// C语言声明区
%{
#include <stdio.h>
#include <stdlib.h>

/* 在单独进行词法分析阶段，我们用 enum 临时替代 Bison 生成的 Token 编号 */
enum TokenType {
    // 0 EOF
    // 1 - 255 ASCII字符
    TYPE = 258, STRUCT, RETURN, IF, ELSE, WHILE,
    INT, FLOAT, ID, SEMI, COMMA, ASSIGNOP, RELOP,
    PLUS, MINUS, STAR, DIV, AND, OR, DOT, NOT,
    LP, RP, LB, RB, LC, RC
};

extern int yylineno; // 显式维护行号，用于错误处理
%}

// Flex定义区
/* 正则表达式定义 */
digit       [0-9]
letter      [a-zA-Z_]
id          {letter}({letter}|{digit})*
relop       ">="|"<="|"=="|"!="|">"|"<"

// 规则翻译区
%%
"\n"            { yylineno++; }
[ \t\r]+        { /* 忽略空白符 */ }

"int"|"float"   { printf("%s\t: TYPE\n", yytext); return TYPE; }
"struct"        { printf("%s\t: STRUCT\n", yytext); return STRUCT; }
"return"        { printf("%s\t: RETURN\n", yytext); return RETURN; }
"if"            { printf("%s\t: IF\n", yytext); return IF; }
"else"          { printf("%s\t: ELSE\n", yytext); return ELSE; }
"while"         { printf("%s\t: WHILE\n", yytext); return WHILE; }

{id}            { printf("%s\t: ID\n", yytext); return ID; }
{digit}+        { printf("%s\t: INT\n", yytext); return INT; }
{digit}+\.{digit}+ { printf("%s\t: FLOAT\n", yytext); return FLOAT; }
// . 在正则表达式中表示任意字符，所以要用 \. 来匹配小数点

";"             { printf("%s\t: SEMI\n", yytext); return SEMI; }
","             { printf("%s\t: COMMA\n", yytext); return COMMA; }
"="             { printf("%s\t: ASSIGNOP\n", yytext); return ASSIGNOP; }
{relop}         { printf("%s\t: RELOP\n", yytext); return RELOP; }
"+"             { printf("%s\t: PLUS\n", yytext); return PLUS; }
"-"             { printf("%s\t: MINUS\n", yytext); return MINUS; }
"*"             { printf("%s\t: STAR\n", yytext); return STAR; }
"/"             { printf("%s\t: DIV\n", yytext); return DIV; }
"&&"            { printf("%s\t: AND\n", yytext); return AND; }
"||"            { printf("%s\t: OR\n", yytext); return OR; }
"."             { printf("%s\t: DOT\n", yytext); return DOT; }
"!"             { printf("%s\t: NOT\n", yytext); return NOT; }
"("             { printf("%s\t: LP\n", yytext); return LP; }
")"             { printf("%s\t: RP\n", yytext); return RP; }
"["             { printf("%s\t: LB\n", yytext); return LB; }
"]"             { printf("%s\t: RB\n", yytext); return RB; }
"{"             { printf("%s\t: LC\n", yytext); return LC; }
"}"             { printf("%s\t: RC\n", yytext); return RC; }

// 以上都没有匹配的规则，作为错误处理
.               { printf("Error type A at Line %d: Mysterious characters \'%s\'\n", yylineno, yytext); }

// C语言代码区
%%

int yywrap() {
    return 1;
}

int main(int argc, char** argv) {
    if (argc > 1) {
        if (!(yyin = fopen(argv[1], "r"))) {
            perror(argv[1]);
            return 1;
        }
    }
    while (yylex() != 0); // 循环调用词法分析，直到文件结束
    return 0;
}