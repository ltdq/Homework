#include "List.h"

#include <stddef.h>
#include <stdlib.h>

#include "Data.h"

static DataNode* head = NULL;
static DataNode* tail = NULL;

// 已访问节点链表头指针
static DataNode* visited_head = NULL;

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
  if (head == NULL) {
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

// 将已访问节点添加到链表中
void list_push_visited(DataNode* node) {
  node->next = visited_head;
  visited_head = node;
}

// 检查节点是否已访问
int is_visited(DataNode* node) {
  DataNode* current = visited_head;
  while (current) {
    if (current == node) {
      return 1;
    }
    current = current->next;
  }
  return 0;
}

// 清空已访问节点链表
void list_clear_visited(void) {
  while (visited_head) {
    DataNode* next = visited_head->next;
    visited_head = next;
  }
  visited_head = NULL;
}
