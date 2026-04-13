#include "Hash.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Data.h"
#include "rapidhash.h"

/* hash_table 是哈希表的桶数组，每个桶头都是一条单链表。 */
static DataNode* hash_table[HASH_TABLE_SIZE];

static unsigned int hash_key(const char* key) {
  /* rapidhash 返回 64 位哈希值。 */
  uint64_t hash = rapidhash(key, strlen(key));

  /* 对桶数量取模，把哈希值映射成合法数组下标。 */
  return (unsigned int)(hash % HASH_TABLE_SIZE);
}

void hash_init(void) {
  /* 逐桶清空桶头指针。 */
  for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
    hash_table[i] = NULL;
  }
}

void hash_add(DataNode* node) {
  /* 根据节点 ID 计算它应该进入哪个桶。 */
  unsigned int idx = hash_key(node->id);

  /* 头插法把新节点挂到对应桶链表的最前面。 */
  node->hash_next = hash_table[idx];

  /* 更新桶头。 */
  hash_table[idx] = node;
}

DataNode* hash_find(const char* id) {
  /* 先定位到目标桶。 */
  DataNode* node = hash_table[hash_key(id)];

  /* 沿着冲突链逐个比较字符串内容。 */
  while (node) {
    /* 找到完全相同的 ID 时直接返回。 */
    if (strcmp(node->id, id) == 0) {
      return node;
    }

    /* 继续检查同桶的下一个节点。 */
    node = node->hash_next;
  }

  /* 整条链遍历结束依旧没命中，说明不存在。 */
  return NULL;
}

void hash_remove(DataNode* node) {
  /* 先根据节点自己的 ID 找到它所在的桶。 */
  unsigned int idx = hash_key(node->id);

  /* 目标节点刚好是桶头时，直接把桶头挪到下一个节点。 */
  if (hash_table[idx] == node) {
    hash_table[idx] = node->hash_next;
    return;
  }

  /* prev 用来在桶链表中寻找目标节点的前一个节点。 */
  DataNode* prev = hash_table[idx];

  /* 一直向后走，直到找到“后继就是目标节点”的位置。 */
  while (prev && prev->hash_next != node) {
    prev = prev->hash_next;
  }

  /* 成功找到前驱时，把前驱直接连到目标节点的后继。 */
  if (prev) {
    prev->hash_next = node->hash_next;
  }
}
