#ifndef TRIE_H
#define TRIE_H

typedef struct DataNode DataNode;

// 子节点链表
typedef struct ChildNode {
  unsigned char byte;
  struct TrieNode* child;
  struct ChildNode* next;
} ChildNode;

// 前缀树节点定义
typedef struct TrieNode {
  ChildNode* children;
  DataNode** data;
  int data_count;
} TrieNode;

// 前缀树操作函数声明
void trie_init(void);
void trie_insert(DataNode* node);
void trie_delete(DataNode* node);
DataNode** trie_search(const char* prefix, int* count);

#endif
