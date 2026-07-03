# 阶段性进展汇报：C-- 编译器前端扩展与优化总结

## 一、 今日核心工作与成果

在原有 C-- 编译器前端的基础上，今日完成了对词法分析（Flex）与语法分析（Bison）的深度功能扩充与输出结构优化，成功跑通了更复杂的文法闭环，并实现了高可读性的 AST 输出。

* 文法功能全覆盖：
  * 第一阶段（必备）：完美支持了 `void` 类型、`float` 表达式、括号优先级 `()`、单目负号 `-`、除法 `/`、逻辑非 `!` 以及 `if-else` 分支和 `while` 循环控制。
  * 第二阶段（扩展）：成功引入了一维数组（通过新增 `VarDec` 节点抽象）及带参/无参函数调用，并支持了 `break` 与 `continue` 语句。
* AST 视觉与数据双优化：重构了 `tree.c` 中的树遍历打印逻辑。使得 `TYPE`、`ID`、`INT`、`FLOAT`、`RELOP` 等核心 Token 能在打印时动态显现具体字面值（如 `TYPE: float`、`RELOP: <=`），大幅提升了语法树的层次感与工程直观性。

---

## 二、 核心模块源码架构

### 1. 词法分析扩充 (src/lexical.l)
通过 `%option yylineno` 实现行号的自动化追踪，并利用全局联合体 `yylval` 实现了词法叶子节点向语法分析的高效打包传递。

```lex
"int"|"float"|"void" { SAVE_NODE(TYPE); return TYPE; }
"while"         { SAVE_NODE(WHILE); return WHILE; }
"break"         { SAVE_NODE(BREAK); return BREAK; }
"continue"      { SAVE_NODE(CONTINUE); return CONTINUE; }
{id}            { SAVE_NODE(ID); return ID; }
{digit}+\.{digit}+ { SAVE_NODE(FLOAT); return FLOAT; }
```

### 2. 语法分析与二义性消除 (src/syntax.y)
在 Bison 中通过明确算术运算符、逻辑运算符的优先级，以及引入 `%nonassoc LOWER_THAN_ELSE` / `%nonassoc ELSE` 显式化解了经典的 `dangling-else`（悬空 else）移进-规约冲突。

针对数组声明，重新抽象了 `VarDec` 产生式，使之完美相容普通变量与数组定义：
```yacc
VarDec : ID { $$ = create_node("VarDec", ..., NODE_SYNTAX, NULL); insert_child($$, $1); }
       | VarDec LB INT RB { ... }
       ;
```

---

## 三、 关键测试验证（优化后 AST 输出示例）

编写了涵盖函数、变量初始化、多层 if-while 嵌套、数组访问及控制流的综合测试用例，解析器无冲突稳定运行。

部分核心树形输出片段：
```text
Program (1)
  ExtDefList (1)
    ExtDef (1)
      Specifier (1)
        TYPE: int
      FunDec (1)
        ID: main
        LP
        RP
      CompSt (1)
        LC
        DefList (1)
          Def (1)
            Specifier (1)
              TYPE: int
            DecList (1)
              Dec (1)
                VarDec (1)
                  ID: a
                ASSIGNOP
                Exp (1)
                  INT: 10
        StmtList (1)
          Stmt (1)
            IF
            LP
            Exp (1)
              Exp (1)
                ID: a
              RELOP: >
              Exp (1)
                INT: 5
```

---

## 四、 心得体会与后续展望

1. 对前后端边界的深刻认知：通过本次调整，明确了 Flex（词法）只负责基于正则的单字识别，而 Bison（语法）负责基于上下文无关文法的句子结构拼装。当前的 AST 已经不是干瘪的骨架，而是反映代码真实面貌的完整拓扑结构。
2. 后续规划：当前生成的带有精准行号（如节点旁的 `(1)`）以及具体字面值的抽象语法树，为接下来的第四阶段（语义分析与符号表构建）打下了坚实的数据结构基础。下一步将着手设计符号表并进行全方位的静态类型检查。