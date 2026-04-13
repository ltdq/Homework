#include "List.h"

#include <stdlib.h>

#include "Data.h"

static DataNode* head = NULL;
static DataNode* tail = NULL;

// 初始化链表
void list_init(void) {
  while (head) {
    DataNode* next = head->next;
    free(head->name);
    free(head->id);
    free(head->value);
    free(head);
    head = next;
  }
  head = NULL;
  tail = NULL;
}

// 将节点添加到链表头部
void list_push_front(DataNode* node) {
  node->prev = NULL;
  node->next = NULL;
  if (!head) {
    head = node;
    tail = node;
    return;
  }
  node->next = head;
  head->prev = node;
  head = node;
}

// 从链表中移除节点
void list_remove(DataNode* node) {
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    head = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  } else {
    tail = node->prev;
  }
}

// 获取链表头部节点
DataNode* list_head(void) { return head; }

// 获取链表尾部节点
DataNode* list_tail(void) { return tail; }
