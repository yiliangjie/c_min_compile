# C-- 编译器前端设计与实现技术报告

## 一、 实验目标与总体架构

本实验基于 Ubuntu 环境，利用 Flex 与 Bison 工具链，实现了 C-- 语言（C语言子集）的编译器前端。
* 输入：C-- 源代码文件（`.cmm`）
* 输出：标准的抽象语法树（AST）层级结构化文本
* 前端流水线：
  $$\text{源代码 (.cmm)} \xrightarrow{\text{Flex 词法分析}} \text{Token 流} \xrightarrow{\text{Bison 语法分析}} \text{内存 AST 树} \xrightarrow{\text{遍历}} \text{层级文本输出}$$

---

## 二、 词法分析设计与实现 (Flex)

### 1. 核心模式匹配 (Regex)
利用正规表达式对 C-- 的关键字、标识符、常数及运算符进行全集覆盖：
* 标识符 (ID)：`[a-zA-Z_][a-zA-Z0-9_]*`
* 整型常量 (INT)：`[0-9]+`
* 浮点常量 (FLOAT)：`[0-9]+\.[0-9]+`

### 2. 动作为空与数据传递机制
在 `lexical.l` 中激活 `%option yylineno` 实现行号自动追踪。取消单纯的文本打印，通过自定义宏 `SAVE_NODE(name)` 触发 C 语言动作：

```c
#define SAVE_NODE(name) yylval.node = create_node(#name, yylineno, NODE_TOKEN, yytext)
```

每当识别到一个 Token，立即在堆区 `malloc` 构建一个叶子节点，并将其指针通过通用联合体 `yylval` 传递给下游的 Bison 解析器。

---

## 三、 语法分析与 AST 构建 (Bison)

### 1. 核心上下文无关文法 (CFG) 设计
在 `syntax.y` 中定义 C-- 的核心文法骨架，重点支持局部变量带初始化的声明以及条件分支分支控制。
主要产生式（部分）：

```yacc
CompSt   : LC DefList StmtList RC ;
Def      : Specifier DecList SEMI ;
Dec      : ID | ID ASSIGNOP Exp ;
Stmt     : Exp SEMI | RETURN Exp SEMI | IF LP Exp RP Stmt | CompSt ;
Exp      : Exp PLUS Exp | Exp RELOP Exp | ID | INT ;
```

### 2. 冲突解决 (二义性消除)
为规避加减乘除及赋值号带来的 移进-规约 (Shift-Reduce) 冲突，在 Bison 声明区显式规定了算术运算符的结合性与优先级：

```yacc
%right ASSIGNOP
%left RELOP
%left PLUS MINUS
%left STAR DIV
```

### 3. 多叉树的“左孩子右兄弟”存储实现
由于语法树每个非终结符节点的子节点数量动态不固定，工程上采用左孩子右兄弟 (Left-Child Right-Sibling) 链表结构定义树节点：

```c
struct Node {
    char* name;          // 语法单元名 (如 Exp, Stmt)
    int line;            // 行号 (用于后续报错定位)
    struct Node* child;   // 指向第一个子节点
    struct Node* brother; // 指向相邻的右侧兄弟节点
};
```

在 Bison 产生式的语义动作中，通过 `insert_child()` 动态挂载子节点。当分析至顶层非终结符 `Program` 时，全局根节点 `root` 挂载完毕。

---

## 四、 实验结果验证与分析

### 1. 正确用例测试
输入标准的条件分支控制源码：

```c
int main() {
    int a = 10;
    if (a > 5) { return a; }
}
```

运行结果：
编译器前端成功在内存中逆向构筑 AST，并以标准缩进打印出深层嵌套结构（缩进完整反映了 `Program -> ExtDef -> CompSt -> StmtList -> Stmt -> CompSt` 的控制流嵌套关系），未发生越界或节点丢失。

### 2. 错误处理测试
去掉代码中合法的分号后，解析器未崩溃。在遇到非法输入时，词法分析与语法分析分别能精准触发错误动作，输出预期的报错日志：
* 词法错误：`Error type A at Line X: Mysterious characters`
* 语法错误：`Error type B at Line X: syntax error`

---

## 五、 结论

本实验成功跑通了编译器前端的核心闭环。Flex 生成的 DFA 状态转移矩阵与 Bison 生成的 LALR(1) 分析表能够协同高效工作，输出的 AST 结构健壮、层级清晰，为后续阶段的符号表建立、语义检查以及中间代码生成奠定了坚实的数据结构基础。