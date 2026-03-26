#include "Trie.h"

#include <stdlib.h>
#include <string.h>

#include "Data.h"
#include "Display.h"
#include "memory.h"

static TrieNode* root = NULL;

// 创建前缀树节点
static TrieNode* create_trie_node(void) {
  TrieNode* node = (TrieNode*)calloc(1, sizeof(TrieNode));
  memory_check(node);
  return node;
}

// 递归定位前缀节点，按 unsigned char 作为 children 下标逐字节下钻
static TrieNode* find_node_recursive(TrieNode* node, const unsigned char* key) {
  if (!node) return NULL;
  if (*key == '\0') return node;
  return find_node_recursive(node->children[*key], key + 1);
}

// 递归释放前缀树节点
static void free_trie_node(TrieNode* node) {
  if (!node) return;
  for (int i = 0; i < 256; i++) {
    free_trie_node(node->children[i]);
  }
  free(node->data);
  free(node);
}

// 检查节点是否为空
static int trie_node_is_empty(TrieNode* node) {
  if (node->data_count != 0) return 0;
  for (int i = 0; i < 256; i++) {
    if (node->children[i]) return 0;
  }
  return 1;
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
    if (!cur->children[key[i]]) {
      cur->children[key[i]] = create_trie_node();
    }
    cur = cur->children[key[i]];
  }
  cur->data = realloc(cur->data, sizeof(DataNode*) * (cur->data_count + 1));
  memory_check(cur->data);
  cur->data[cur->data_count++] = node;
}

// 删除数据节点
void trie_delete(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  size_t key_len = strlen((const char*)key);
  size_t path_len = key_len ? key_len : 1;
  TrieNode** path = malloc(sizeof(TrieNode*) * path_len);
  unsigned char* indices = malloc(sizeof(unsigned char) * path_len);
  memory_check(path);
  memory_check(indices);
  int depth = 0;
  TrieNode* cur = root;
  for (int i = 0; key[i]; i++) {
    TrieNode* next = cur->children[key[i]];
    if (!next) {
      free(path);
      free(indices);
      return;
    }
    path[depth] = cur;
    indices[depth] = key[i];
    depth++;
    cur = next;
  }
  // 找到并移除数据节点
  int found = 0;
  for (int i = 0; i < cur->data_count; i++) {
    if (cur->data[i] == node) {
      cur->data[i] = cur->data[cur->data_count - 1];
      cur->data_count--;
      found = 1;
      break;
    }
  }
  if (!found) {
    free(path);
    free(indices);
    return;
  }
  // 回溯：释放空节点
  for (int i = depth - 1; i >= 0; i--) {
    if (trie_node_is_empty(cur)) {
      free(cur->data);
      free(cur);
      path[i]->children[indices[i]] = NULL;
      cur = path[i];
    } else {
      break;
    }
  }
  free(path);
  free(indices);
}

// 收集节点及其子树下所有数据节点
static void collect(TrieNode* node, DataNode*** result, int* count, int* cap) {
  if (!node) return;
  for (int i = 0; i < node->data_count; i++) {
    if (*count >= *cap) {
      *cap = (*cap < 8) ? 8 : (*cap + (*cap >> 2));
      *result = realloc(*result, sizeof(DataNode*) * (*cap));
      memory_check(*result);
    }
    (*result)[(*count)++] = node->data[i];
  }
  for (int i = 0; i < 256; i++) {
    collect(node->children[i], result, count, cap);
  }
}

// 前缀搜索，返回匹配的数据节点数组
DataNode** trie_search(const char* prefix, int* count) {
  const unsigned char* key = (const unsigned char*)prefix;
  *count = 0;
  TrieNode* cur = find_node_recursive(root, key);
  if (!cur) return NULL;
  DataNode** result = NULL;
  int cap = 0;
  collect(cur, &result, count, &cap);
  return result;
}
