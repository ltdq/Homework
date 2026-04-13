#ifndef STACK_H
#define STACK_H

/*
 * 这三个常量描述可撤销操作的类型。
 * 数据层会把每次用户操作编码成其中一种类型压入栈中。
 */
enum {
  /* 记录一次插入操作。 */
  OP_INSERT = 1,
  /* 记录一次删除操作。 */
  OP_DELETE = 2,
  /* 记录一次修改操作。 */
  OP_MODIFY = 3,
};

/*
 * 这组索引常量保留给需要按字段顺序处理数据的场景。
 * 当前主流程里没有大量使用，保留下来有利于后续扩展。
 */
enum {
  /* 字段顺序中的姓名位置。 */
  IDX_NAME = 0,
  /* 字段顺序中的 ID 位置。 */
  IDX_ID = 1,
};

/*
 * Operator 描述一条可撤销的历史记录。
 * 对于删除和修改，name / value 保存的是执行前的旧内容。
 * 对于插入，撤销时只需要按 id 删除，name / value 主要用于日志与恢复统一性。
 */
typedef struct Operator {
  /* 操作类型，取值来自 OP_INSERT / OP_DELETE / OP_MODIFY。 */
  int type;
  /* 目标记录的 ID。 */
  char* id;
  /* 目标记录的姓名。 */
  char* name;
  /* 目标记录的 value 内容。 */
  char* value;
} Operator;

/*
 * StackNode 是单链表栈节点。
 * 栈顶永远指向最近一次用户操作。
 */
typedef struct StackNode {
  /* 当前栈节点保存的一条操作记录。 */
  Operator data;
  /* 指向下一个更旧的栈节点。 */
  struct StackNode* next;
} StackNode;

/* 把一个已经构造好的节点压入栈顶。 */
void stack_push(StackNode* node);

/* 返回栈顶节点，栈为空时返回 NULL。 */
StackNode* stack_get_top(void);

/* 弹出栈顶节点，并释放其内部字符串和节点本身。 */
void stack_pop(void);

/* 返回栈是否为空。 */
int stack_is_empty(void);

/* 清空整个操作栈。 */
void stack_init(void);

#endif
