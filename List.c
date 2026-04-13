#include "List.h"

#include <stdlib.h>

#include "Data.h"

/* head 始终指向链表头节点，也就是最近插入的记录。 */
static DataNode* head = NULL;

/* tail 始终指向链表尾节点，也就是最早插入的记录。 */
static DataNode* tail = NULL;

void list_init(void) {
  /* 这里会把旧链表里的所有节点完整释放掉。 */
  while (head) {
    /* 先保存后继，避免当前节点释放后丢失链表剩余部分。 */
    DataNode* next = head->next;

    /* 释放节点内部动态申请的姓名字符串。 */
    free(head->name);

    /* 释放节点内部动态申请的 ID 字符串。 */
    free(head->id);

    /* 释放节点内部动态申请的 value 字符串。 */
    free(head->value);

    /* 释放节点本身。 */
    free(head);

    /* 继续处理下一个节点。 */
    head = next;
  }

  /* 链表已经彻底清空，头指针恢复为空。 */
  head = NULL;

  /* 尾指针也同步恢复为空。 */
  tail = NULL;
}

void list_push_front(DataNode* node) {
  /* 新头节点前面一定没有别的节点。 */
  node->prev = NULL;

  /* 先把后继清空，后续再根据链表状态决定是否连接。 */
  node->next = NULL;

  /* 空链表时，新节点同时成为头和尾。 */
  if (!head) {
    head = node;
    tail = node;
    return;
  }

  /* 非空链表时，新节点的后继是当前头节点。 */
  node->next = head;

  /* 旧头节点的前驱更新为新节点。 */
  head->prev = node;

  /* 最后更新 head，让新节点正式成为表头。 */
  head = node;
}

void list_remove(DataNode* node) {
  /* 目标节点有前驱时，让前驱绕过当前节点直接连向后继。 */
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    /* 当前节点本身是表头时，表头需要移动到后继。 */
    head = node->next;
  }

  /* 目标节点有后继时，让后继绕过当前节点直接连向前驱。 */
  if (node->next) {
    node->next->prev = node->prev;
  } else {
    /* 当前节点本身是表尾时，表尾需要移动到前驱。 */
    tail = node->prev;
  }
}

DataNode* list_head(void) {
  /* 返回当前链表头节点。 */
  return head;
}

DataNode* list_tail(void) {
  /* 返回当前链表尾节点。 */
  return tail;
}
