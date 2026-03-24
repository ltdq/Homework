#include "Trie.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Data.h"
#include "Display.h"

static TrieNode* root = NULL;

// 创建前缀树节点
static TrieNode* create_trie_node(void) {
  return (TrieNode*)calloc(1, sizeof(TrieNode));
}

// 递归释放前缀树节点
static void free_trie_node(TrieNode* node) {
  if (!node) return;
  for (int i = 0; i < TRIE_CHILDREN; i++) {
    if (node->children[i]) {
      free_trie_node(node->children[i]);
    }
  }
  free(node->data);
  free(node);
}

// 初始化前缀树
void trie_init(void) {
  free_trie_node(root);
  root = create_trie_node();
}

// 插入数据节点，用 node->name 作为 key
void trie_insert(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  TrieNode* cur = root;
  for (int i = 0; key[i]; i++) {
    unsigned char b = key[i];
    if (!cur->children[b]) {
      cur->children[b] = create_trie_node();
    }
    cur = cur->children[b];
  }
  cur->data = realloc(cur->data, sizeof(DataNode*) * (cur->data_count + 1));
  cur->data[cur->data_count++] = node;
}

// 删除数据节点
void trie_delete(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  TrieNode* cur = root;
  for (int i = 0; key[i]; i++) {
    unsigned char b = key[i];
    if (!cur->children[b]) return;
    cur = cur->children[b];
  }
  for (int i = 0; i < cur->data_count; i++) {
    if (cur->data[i] == node) {
      cur->data[i] = cur->data[cur->data_count - 1];
      cur->data_count--;
      return;
    }
  }
}

// 收集节点及其子树下所有数据节点
static void collect(TrieNode* node, DataNode*** result, int* count) {
  if (!node) return;
  for (int i = 0; i < node->data_count; i++) {
    *result = realloc(*result, sizeof(DataNode*) * (*count + 1));
    (*result)[(*count)++] = node->data[i];
  }
  for (int i = 0; i < TRIE_CHILDREN; i++) {
    if (node->children[i]) {
      collect(node->children[i], result, count);
    }
  }
}

// 前缀搜索，返回匹配的数据节点数组
DataNode** trie_search(const char* prefix, int* count) {
  const unsigned char* key = (const unsigned char*)prefix;
  TrieNode* cur = root;
  *count = 0;
  for (int i = 0; key[i]; i++) {
    unsigned char b = key[i];
    if (!cur->children[b]) return NULL;
    cur = cur->children[b];
  }
  DataNode** result = NULL;
  collect(cur, &result, count);
  return result;
}
