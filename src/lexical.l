%{
#include "tree.h"
#include "syntax.tab.h" // 此时由 Bison 自动生成此头文件

extern int yylineno;

// 辅助宏：帮我们把 Flex 抓到的文字打包成树节点传递给 Bison
#define SAVE_NODE(name) yylval.node = create_node(#name, yylineno, NODE_TOKEN, yytext)
%}

/* 让 Flex 自动追踪行号位置 */
%option yylineno

digit       [0-9]
letter      [a-zA-Z_]
id          {letter}({letter}|{digit})*
relop       ">="|"<="|"=="|"!="|">"|"<"

%%
"\n"            { /* %option yylineno 会自动处理，这里不写 */ }
[ \t\r]+        { /* 忽略空白符 */ }

"int"|"float"   { SAVE_NODE(TYPE); return TYPE; }
"struct"        { SAVE_NODE(STRUCT); return STRUCT; }
"return"        { SAVE_NODE(RETURN); return RETURN; }
"if"            { SAVE_NODE(IF); return IF; }
"else"          { SAVE_NODE(ELSE); return ELSE; }
"while"         { SAVE_NODE(WHILE); return WHILE; }

{id}            { SAVE_NODE(ID); return ID; }
{digit}+        { SAVE_NODE(INT); return INT; }
{digit}+\.{digit}+ { SAVE_NODE(FLOAT); return FLOAT; }

";"             { SAVE_NODE(SEMI); return SEMI; }
","             { SAVE_NODE(COMMA); return COMMA; }
"="             { SAVE_NODE(ASSIGNOP); return ASSIGNOP; }
{relop}         { SAVE_NODE(RELOP); return RELOP; }
"+"             { SAVE_NODE(PLUS); return PLUS; }
"-"             { SAVE_NODE(MINUS); return MINUS; }
"*"             { SAVE_NODE(STAR); return STAR; }
"/"             { SAVE_NODE(DIV); return DIV; }
"&&"            { SAVE_NODE(AND); return AND; }
"||"            { SAVE_NODE(OR); return OR; }
"."             { SAVE_NODE(DOT); return DOT; }
"!"             { SAVE_NODE(NOT); return NOT; }
"("             { SAVE_NODE(LP); return LP; }
")"             { SAVE_NODE(RP); return RP; }
"["             { SAVE_NODE(LB); return LB; }
"]"             { SAVE_NODE(RB); return RB; }
"{"             { SAVE_NODE(LC); return LC; }
"}"             { SAVE_NODE(RC); return RC; }

.               { printf("Error type A at Line %d: Mysterious characters \'%s\'\n", yylineno, yytext); }

%%

int yywrap() {
    return 1;
}