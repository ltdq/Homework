#include "Stack.h"

#include <stdio.h>
#include <stdlib.h>

static StackNode* top = NULL;

// 入栈
void stack_push(StackNode* node) {
  if (!top) {
    top = node;
    return;
  }
  node->next = top;
  top = node;
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
