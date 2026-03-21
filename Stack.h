#ifndef STACK_H
#define STACK_H

enum {
  OP_INSERT = 1,
  OP_DELETE = 2,
  OP_MODIFY = 3,
};

enum {
  IDX_NAME = 0,
  IDX_ID = 1,
};

typedef struct Operator {
  int type;  // 1 增加 2 删除 3 修改
  char* id;
  char* name;
  char* value;
} Operator;

typedef struct StackNode {
  Operator data;
  struct StackNode* next;
} StackNode;

void stack_push(StackNode* node);
StackNode* stack_get_top(void);
void stack_pop(void);
int stack_is_empty(void);
void stack_init(void);

#endif
