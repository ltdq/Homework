#include "Trie.h"

#include <stdio.h>
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

// 查找子节点
static TrieNode* find_child(TrieNode* node, unsigned char b) {
  for (ChildNode* c = node->children; c; c = c->next) {
    if (c->byte == b) return c->child;
  }
  return NULL;
}

// 获取或创建子节点
static TrieNode* get_or_create_child(TrieNode* node, unsigned char b) {
  for (ChildNode* c = node->children; c; c = c->next) {
    if (c->byte == b) return c->child;
  }
  ChildNode* c = malloc(sizeof(ChildNode));
  memory_check(c);
  c->byte = b;
  c->child = create_trie_node();
  c->next = node->children;
  node->children = c;
  return c->child;
}

// 删除子节点
static void remove_child(TrieNode* node, unsigned char b) {
  ChildNode** pp = &node->children;
  while (*pp) {
    if ((*pp)->byte == b) {
      ChildNode* tmp = *pp;
      *pp = tmp->next;
      free(tmp);
      return;
    }
    pp = &(*pp)->next;
  }
}

// 递归释放前缀树节点
static void free_trie_node(TrieNode* node) {
  if (!node) return;
  ChildNode* c = node->children;
  while (c) {
    ChildNode* tmp = c;
    c = c->next;
    free_trie_node(tmp->child);
    free(tmp);
  }
  free(node->data);
  free(node);
}

// 检查节点是否为空
static int trie_node_is_empty(TrieNode* node) {
  return node->data_count == 0 && node->children == NULL;
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
    cur = get_or_create_child(cur, key[i]);
  }
  cur->data = realloc(cur->data, sizeof(DataNode*) * (cur->data_count + 1));
  memory_check(cur->data);
  cur->data[cur->data_count++] = node;
}

// 删除数据节点
void trie_delete(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  TrieNode* path[256];
  unsigned char indices[256];
  int depth = 0;
  TrieNode* cur = root;
  for (int i = 0; key[i]; i++) {
    TrieNode* next = find_child(cur, key[i]);
    if (!next) return;
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
  if (!found) return;
  // 回溯：释放空节点
  for (int i = depth - 1; i >= 0; i--) {
    if (trie_node_is_empty(cur)) {
      free(cur->data);
      free(cur);
      remove_child(path[i], indices[i]);
      cur = path[i];
    } else {
      break;
    }
  }
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
  for (ChildNode* c = node->children; c; c = c->next) {
    collect(c->child, result, count, cap);
  }
}

// 前缀搜索，返回匹配的数据节点数组
DataNode** trie_search(const char* prefix, int* count) {
  const unsigned char* key = (const unsigned char*)prefix;
  TrieNode* cur = root;
  *count = 0;
  for (int i = 0; key[i]; i++) {
    cur = find_child(cur, key[i]);
    if (!cur) return NULL;
  }
  DataNode** result = NULL;
  int cap = 0;
  collect(cur, &result, count, &cap);
  return result;
}
