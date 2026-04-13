#ifndef TRIE_H
#define TRIE_H

typedef struct DataNode DataNode;

// 前缀树节点定义
typedef struct TrieNode {
  struct TrieNode* children[256];
  DataNode** data;
  int data_count;
} TrieNode;

// 前缀树操作函数声明
void trie_init(void);
void trie_insert(DataNode* node);
void trie_delete(DataNode* node);
DataNode** trie_search(const char* prefix, int* count);

#endif
