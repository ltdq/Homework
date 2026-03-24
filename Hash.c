#include "Hash.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Data.h"
#include "rapidhash.h"

static DataNode* hash_table_name[HASH_TABLE_SIZE];
static DataNode* hash_table_id[HASH_TABLE_SIZE];

// 计算哈希值并返回索引
static unsigned int hash_key(const char* key) {
  uint64_t hash = rapidhash(key, strlen(key));
  return (unsigned int)(hash % HASH_TABLE_SIZE);
}

// 初始化哈希表
void hash_init(void) {
  for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
    hash_table_name[i] = NULL;
    hash_table_id[i] = NULL;
  }
}

// 添加数据节点到哈希表
void hash_add(DataNode* node) {
  unsigned int idx_name = hash_key(node->name);
  unsigned int idx_id = hash_key(node->id);
  node->hash_name_next = hash_table_name[idx_name];
  node->hash_id_next = hash_table_id[idx_id];
  hash_table_name[idx_name] = node;
  hash_table_id[idx_id] = node;
}

// 查找数据节点(通过name)
DataNode* hash_find_by_name(const char* name) {
  return hash_table_name[hash_key(name)];
}

// 查找数据节点(通过id)
DataNode* hash_find_by_id(const char* id) {
  return hash_table_id[hash_key(id)];
}

// 删除数据节点并更新哈希表和链表指针
void hash_remove(DataNode* node) {
  unsigned int idx_name = hash_key(node->name);
  unsigned int idx_id = hash_key(node->id);

  if (hash_table_name[idx_name] == node) {
    hash_table_name[idx_name] = node->hash_name_next;
  } else {
    DataNode* prev = hash_table_name[idx_name];
    while (prev && prev->hash_name_next != node) {
      prev = prev->hash_name_next;
    }
    if (prev) {
      prev->hash_name_next = node->hash_name_next;
    }
  }

  if (hash_table_id[idx_id] == node) {
    hash_table_id[idx_id] = node->hash_id_next;
  } else {
    DataNode* prev = hash_table_id[idx_id];
    while (prev && prev->hash_id_next != node) {
      prev = prev->hash_id_next;
    }
    if (prev) {
      prev->hash_id_next = node->hash_id_next;
    }
  }
}
