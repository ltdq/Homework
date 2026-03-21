#ifndef LIST_H
#define LIST_H

typedef struct DataNode DataNode;

typedef struct ListNode {
  DataNode* data;
  struct ListNode* next;
} ListNode;

// 链表操作函数声明
void list_init(void);
void list_push_front(DataNode* node);
void list_remove(DataNode* node);
DataNode* list_head(void);
DataNode* list_tail(void);

/* 为已访问节点开个链表存一下，防止在神人把id存
  进name，然后再存一份到id导致的打印两份
 */
void list_push_visited(DataNode* node);
int is_visited(DataNode* node);
void list_clear_visited(void);

#endif
