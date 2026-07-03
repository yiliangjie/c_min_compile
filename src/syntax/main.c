#include <stdio.h>
#include <stdlib.h>
#include "../common/tree.h"       // 语法树支持
#include "../semantic/semantic.h"   // 语义分析支持
#include "../ir/ir.h"         // 中间代码支持

// 声明来自 Flex/Bison 的全局变量和函数
extern FILE* yyin;
extern int yyparse(void);
extern void init_semantic(void);
extern void analyze_tree(struct Node* node);
// 假设你的语法树根节点叫 root，在 syntax.y 或 tree.c 中定义
extern struct Node* root; 
extern int semantic_error_count;
extern int yylineno;



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