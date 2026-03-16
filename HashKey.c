#include "HashKey.h"

#include <stdint.h>
#include <string.h>

#include "Hash.h"
#include "rapidhash.h"

// 计算哈希值并返回哈希表索引
unsigned int hash_key(const char* key) {
  uint64_t hash = rapidhash(key, strlen(key));
  return (unsigned int)(hash % HASH_TABLE_SIZE);
}