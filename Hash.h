#ifndef HASH_H
#define HASH_H

typedef struct DataNode DataNode;

// 哈希表大小定义
#define HASH_TABLE_SIZE 100003

void hash_init(void);
void hash_add(DataNode* node);
DataNode* hash_find_by_name(const char* name);
DataNode* hash_find_by_id(const char* id);
void hash_remove(DataNode* node);

#endif
