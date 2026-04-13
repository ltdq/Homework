#include "Stack.h"

#include <stdlib.h>

/* top 指向最近一次用户操作，对应单链表栈顶。 */
static StackNode* top = NULL;

void stack_push(StackNode* node) {
  /* 新节点的 next 指向旧栈顶。 */
  node->next = top;

  /* 栈顶更新为新节点。 */
  top = node;
}

StackNode* stack_get_top(void) {
  /* 直接返回当前栈顶，空栈时这里自然是 NULL。 */
  return top;
}

void stack_pop(void) {
  /* 空栈时没有任何工作要做。 */
  if (!top) {
    return;
  }

  /* 先保存当前栈顶，便于后续释放。 */
  StackNode* node = top;

  /* 栈顶下移到更旧的一条操作。 */
  top = top->next;

  /* 释放这条历史记录内部复制出来的 ID。 */
  free(node->data.id);

  /* 释放这条历史记录内部复制出来的姓名。 */
  free(node->data.name);

  /* 释放这条历史记录内部复制出来的 value。 */
  free(node->data.value);

  /* 释放栈节点本身。 */
  free(node);
}

int stack_is_empty(void) {
  /* top 为 NULL 代表空栈，表达式结果会自动转换成 0 或 1。 */
  return !top;
}

void stack_init(void) {
  /* 连续弹栈直到所有历史记录都被清空。 */
  while (top) {
    stack_pop();
  }
}
