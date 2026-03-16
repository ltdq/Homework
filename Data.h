#ifndef DATA_H
#define DATA_H

#include "Hash.h"

// 数据节点结构体定义
typedef struct DataNode {
  char* key;
  char* value;
  struct DataNode* hash_next;
  struct DataNode* prev;
  struct DataNode* next;
} DataNode;

// 哈希表和链表头尾指针声明
extern DataNode* hash_table[HASH_TABLE_SIZE];
extern DataNode* head;
extern DataNode* tail;

// 数据操作函数声明
void data_init();
void data_insert(const char* key, const char* value);
char* data_get(const char* key);
void data_delete(const char* key);
void data_modify(const char* key, const char* new_value);

#endif