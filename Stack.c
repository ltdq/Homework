#include "Stack.h"

#include <stdio.h>
#include <stdlib.h>

#include "Log.h"

static StackNode* top = NULL;

// 入栈
void stack_push(StackNode* node) {
  if (!top) {
    top = node;
    return;
  }
  node->next = top;
  top = node;
  log_print("操作入栈: %s, 名字='%s', ID='%s', 值='%s'",
            node->data.type == OP_INSERT   ? "插入"
            : node->data.type == OP_DELETE ? "删除"
                                           : "修改",
            node->data.name, node->data.id, node->data.value);
}

// 获取栈顶元素
StackNode* stack_get_top(void) {
  if (!top) {
    return NULL;
  }
  return top;
}

// 出栈
void stack_pop(void) {
  if (!top) {
    return;
  }
  StackNode* node = top;
  top = top->next;
  free(node->data.id);
  free(node->data.name);
  free(node->data.value);
  free(node);
}

int stack_is_empty(void) { return top == NULL; }

// 清空栈
void stack_init(void) {
  while (top) {
    stack_pop();
  }
}
