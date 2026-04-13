#ifndef TRIE_H
#define TRIE_H

/* 前向声明让 Trie 头文件避免直接依赖 Data.h 的完整定义。 */
typedef struct DataNode DataNode;

/*
 * TrieNode 按字节保存前缀树节点。
 * 这里直接使用 256 个子指针，对应一个字节的全部取值范围。
 * UTF-8 名称会按字节逐层展开，所以中文也可以正常建立前缀索引。
 */
typedef struct TrieNode {
  /* children[i] 指向当前字节为 i 的下一层节点。 */
  struct TrieNode* children[256];
  /* data 保存“完整走到这里的姓名”对应的所有记录指针。 */
  DataNode** data;
  /* data 数组中的有效元素个数。 */
  int data_count;
} TrieNode;

/* 初始化整棵 Trie，并释放旧树。 */
void trie_init(void);

/* 按姓名把一个数据节点插入 Trie。 */
void trie_insert(DataNode* node);

/* 按姓名路径把一个数据节点从 Trie 中删除。 */
void trie_delete(DataNode* node);

/*
 * 按前缀查找所有匹配节点。
 * 返回值是动态分配的结果数组，需要调用者负责 free。
 * count 会被写成匹配数量。
 */
DataNode** trie_search(const char* prefix, int* count);

#endif
