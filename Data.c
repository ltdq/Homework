#include "Data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "HashKey.h"

// 初始化哈希表和链表头尾指针
DataNode* hash_table[HASH_TABLE_SIZE];
DataNode* head = NULL;
DataNode* tail = NULL;

// 创建新的数据节点
static DataNode* create_node(const char* key, const char* value) {
  DataNode* node = (DataNode*)malloc(sizeof(DataNode));
  node->key = strdup(key);
  node->value = strdup(value);
  node->hash_next = NULL;
  node->prev = NULL;
  node->next = NULL;
  return node;
}

// 删除数据节点并更新哈希表和链表指针
static void delete_node(DataNode* node, unsigned int idx, DataNode* prev) {
  if (prev) {
    prev->hash_next = node->hash_next;
  } else {
    hash_table[idx] = node->hash_next;
  }
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    head = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  } else {
    tail = node->prev;
  }
  free(node->key);
  free(node->value);
  free(node);
}

// 初始化数据结构
void data_init() {
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hash_table[i] = NULL;
  }
  head = NULL;
  tail = NULL;
  printf("Hash Done, size: %d\n", HASH_TABLE_SIZE);
}

// 插入数据
void data_insert(const char* key, const char* value) {
  unsigned int idx = hash_key(key);
  if (hash_table[idx]) {
    printf(
        "'%s' has existing entry at index: %u, please use data_modify to "
        "update\n",
        key, idx);
    return;
  }
  DataNode* new_node = create_node(key, value);
  new_node->hash_next = hash_table[idx];
  hash_table[idx] = new_node;
  if (head == NULL) {
    head = new_node;
    tail = new_node;
  } else {
    new_node->next = head;
    head->prev = new_node;
    head = new_node;
  }
  printf("Insert key: '%s', value: '%s' at index: %u\n", key, value, idx);
}

// 获取数据
char* data_get(const char* key) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_table[idx];
  while (node) {
    if (strcmp(node->key, key) == 0) {
      printf("Key: '%s' found at index: %u, value: '%s'\n", key, idx,
             node->value);
      return node->value;
    }
    node = node->hash_next;
  }
  printf("Key: '%s' not found at index: %u\n", key, idx);
  return NULL;
}

// 删除数据
void data_delete(const char* key) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_table[idx];
  DataNode* prev = NULL;
  while (node) {
    if (strcmp(node->key, key) == 0) {
      delete_node(node, idx, prev);
      printf("Key: '%s' deleted at index: %u\n", key, idx);
      return;
    }
    prev = node;
    node = node->hash_next;
  }
  printf("Key: '%s' not found for deletion at index: %u\n", key, idx);
}

// 修改数据
void data_modify(const char* key, const char* new_value) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_table[idx];
  while (node) {
    if (strcmp(node->key, key) == 0) {
      free(node->value);
      node->value = strdup(new_value);
      printf("Key: '%s' modified at index: %u, new value: '%s'\n", key, idx,
             new_value);
      return;
    }
    node = node->hash_next;
  }
  printf("Key: '%s' not found for modification at index: %u\n", key, idx);
}

void data_save(const char* filename) {
  FILE* file = fopen(filename, "w");
  if (!file) {
    perror("Failed to open file for saving");
    return;
  }
  DataNode* node = head;
  while (node) {
    fprintf(file, "%s=%s\n", node->key, node->value);
    node = node->next;
  }
  fclose(file);
  printf("Data saved to %s\n", filename);
}

void data_load(const char* filename) {
  FILE* file = fopen(filename, "r");
  if (!file) {
    perror("Failed to open file for loading");
    return;
  }
  char line[256];
  while (fgets(line, sizeof(line), file)) {
    char* equals_pos = strchr(line, '=');
    if (equals_pos) {
      *equals_pos = '\0';
      char* key = line;
      char* value = equals_pos + 1;
      value[strcspn(value, "\n")] = '\0';  // Remove newline
      data_insert(key, value);
    }
  }
  fclose(file);
  printf("Data loaded from %s\n", filename);
}