#include "Trie.h"

#include <stdlib.h>
#include <string.h>

#include "Data.h"
#include "memory.h"

static TrieNode* root = NULL;

static TrieNode* create_trie_node(void) {
  TrieNode* node = calloc(1, sizeof(*node));
  memory_check(node);
  return node;
}

static TrieNode* trie_walk(TrieNode* node, const unsigned char* key) {
  if (!node) {
    return NULL;
  }
  if (*key == '\0') {
    return node;
  }
  return trie_walk(node->children[*key], key + 1);
}

static TrieNode* trie_ensure_path(TrieNode* node, const unsigned char* key) {
  if (*key == '\0') {
    return node;
  }
  if (!node->children[*key]) {
    node->children[*key] = create_trie_node();
  }
  return trie_ensure_path(node->children[*key], key + 1);
}

static void free_trie_node(TrieNode* node) {
  if (!node) return;
  for (int i = 0; i < 256; ++i) {
    free_trie_node(node->children[i]);
  }
  free(node->data);
  free(node);
}

static int trie_node_is_empty(TrieNode* node) {
  if (node->data_count != 0) {
    return 0;
  }
  for (int i = 0; i < 256; ++i) {
    if (node->children[i]) {
      return 0;
    }
  }
  return 1;
}

void trie_init(void) {
  free_trie_node(root);
  root = create_trie_node();
}

void trie_insert(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  TrieNode* cur = trie_ensure_path(root, key);
  cur->data = realloc(cur->data, sizeof(DataNode*) * (cur->data_count + 1));
  memory_check(cur->data);
  cur->data[cur->data_count++] = node;
}

static int trie_delete_recursive(TrieNode* current, const unsigned char* key,
                                 DataNode* node) {
  if (!current) {
    return 0;
  }

  if (*key == '\0') {
    for (int i = 0; i < current->data_count; ++i) {
      if (current->data[i] == node) {
        current->data[i] = current->data[current->data_count - 1];
        current->data_count--;
        return 1;
      }
    }
    return 0;
  }

  {
    unsigned char index = *key;
    TrieNode* child = current->children[index];
    int deleted = trie_delete_recursive(child, key + 1, node);
    if (!deleted) {
      return 0;
    }
    if (child && trie_node_is_empty(child)) {
      free_trie_node(child);
      current->children[index] = NULL;
    }
    return 1;
  }
}

void trie_delete(DataNode* node) {
  const unsigned char* key = (const unsigned char*)node->name;
  trie_delete_recursive(root, key, node);
}

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
  for (int i = 0; i < 256; ++i) {
    collect(node->children[i], result, count, cap);
  }
}

DataNode** trie_search(const char* prefix, int* count) {
  const unsigned char* key = (const unsigned char*)prefix;
  TrieNode* cur;
  *count = 0;
  cur = trie_walk(root, key);
  if (!cur) return NULL;
  DataNode** result = NULL;
  int cap = 0;
  collect(cur, &result, count, &cap);
  return result;
}
