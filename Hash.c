#include "Hash.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Data.h"
#include "rapidhash.h"

static DataNode* hash_table[HASH_TABLE_SIZE];

// 计算哈希值并返回索引
static unsigned int hash_key(const char* key) {
  uint64_t hash = rapidhash(key, strlen(key));
  return (unsigned int)(hash % HASH_TABLE_SIZE);
}

// 初始化哈希表
void hash_init(void) {
  for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
    hash_table[i] = NULL;
  }
}

// 添加数据节点到哈希表
void hash_add(DataNode* node) {
  unsigned int idx = hash_key(node->id);
  node->hash_next = hash_table[idx];
  hash_table[idx] = node;
}

// 查找数据节点
DataNode* hash_find(const char* id) { return hash_table[hash_key(id)]; }

// 删除数据节点并更新哈希表和链表指针
void hash_remove(DataNode* node) {
  unsigned int idx = hash_key(node->id);

  if (hash_table[idx] == node) {
    hash_table[idx] = node->hash_next;
  } else {
    DataNode* prev = hash_table[idx];
    while (prev && prev->hash_next != node) {
      prev = prev->hash_next;
    }
    if (prev) {
      prev->hash_next = node->hash_next;
    }
  }
}
