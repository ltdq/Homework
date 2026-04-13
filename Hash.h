#ifndef HASH_H
#define HASH_H

/* 这里只做前向声明，避免头文件之间相互包含造成循环依赖。 */
typedef struct DataNode DataNode;

/*
 * 哈希表桶数量。
 * 这里选择较大的质数，目的是降低冲突概率，让平均查询更稳定。
 */
#define HASH_TABLE_SIZE 100003

/* 清空整个哈希表，并把所有桶头指针重置为 NULL。 */
void hash_init(void);

/* 把一个数据节点插入哈希表。 */
void hash_add(DataNode* node);

/* 按 ID 在哈希表中查找对应节点。 */
DataNode* hash_find(const char* id);

/* 把一个已知节点从哈希表中摘除。 */
void hash_remove(DataNode* node);

#endif
