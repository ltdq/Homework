#include "Data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "HashKey.h"

// 初始化哈希表和链表头尾指针
DataNode* hash_table_name[HASH_TABLE_SIZE];
DataNode* hash_table_id[HASH_TABLE_SIZE];
static DataNode* head = NULL;
static DataNode* tail = NULL;

// 创建新的数据节点
static DataNode* create_node(const char* name, const char* id,
                             const char* value) {
  DataNode* node = (DataNode*)malloc(sizeof(DataNode));
  node->name = strdup(name);
  node->id = strdup(id);
  node->value = strdup(value);
  node->hash_name_next = NULL;
  node->hash_id_next = NULL;
  node->prev = NULL;
  node->next = NULL;
  return node;
}

// 删除数据节点并更新哈希表和链表指针
static void delete_node(DataNode* node) {
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

  unsigned int idx_name = hash_key(node->name);
  unsigned int idx_id = hash_key(node->id);

  if (hash_table_name[idx_name] == node) {
    hash_table_name[idx_name] = node->hash_name_next;
  } else {
    DataNode* prev = hash_table_name[idx_name];
    while (prev && prev->hash_name_next != node) {
      prev = prev->hash_name_next;
    }
    if (prev) {
      prev->hash_name_next = node->hash_name_next;
    }
  }

  if (hash_table_id[idx_id] == node) {
    hash_table_id[idx_id] = node->hash_id_next;
  } else {
    DataNode* prev = hash_table_id[idx_id];
    while (prev && prev->hash_id_next != node) {
      prev = prev->hash_id_next;
    }
    if (prev) {
      prev->hash_id_next = node->hash_id_next;
    }
  }

  free(node->name);
  free(node->id);
  free(node->value);
  free(node);
}

// 初始化数据结构
void data_init() {
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hash_table_name[i] = NULL;
    hash_table_id[i] = NULL;
  }
  head = NULL;
  tail = NULL;
  printf("Hash Done, size: %d\n", HASH_TABLE_SIZE);
}

// 插入数据
void data_insert(const char* name, const char* id, const char* value) {
  unsigned int idx_name = hash_key(name);
  unsigned int idx_id = hash_key(id);
  DataNode* node = hash_table_name[idx_name];
  while (node) {
    if (strcmp(node->name, name) == 0 && strcmp(node->id, id) == 0) {
      printf(
          "Name: '%s' already exists at index: %u, should be modified instead "
          "of inserted\n",
          name, idx_name);
      return;
    }
    node = node->hash_name_next;
  }
  DataNode* new_node = create_node(name, id, value);
  new_node->hash_name_next = hash_table_name[idx_name];
  new_node->hash_id_next = hash_table_id[idx_id];
  hash_table_name[idx_name] = new_node;
  hash_table_id[idx_id] = new_node;
  if (head == NULL) {
    head = new_node;
    tail = new_node;
  } else {
    new_node->next = head;
    head->prev = new_node;
    head = new_node;
  }
  printf("Insert name: '%s', id: '%s', value: '%s' at index: %u\n", name, id,
         value, idx_name);
}

// 获取数据
char* data_get(const char* key) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_table_name[idx];
  while (node) {
    if (strcmp(node->name, key) == 0) {
      printf("Key: '%s' found at index: %u, value: '%s'\n", key, idx,
             node->value);
      return node->value;
    }
    node = node->hash_name_next;
  }
  node = hash_table_id[idx];
  while (node) {
    if (strcmp(node->id, key) == 0) {
      printf("Key: '%s' found at index: %u, value: '%s'\n", key, idx,
             node->value);
      return node->value;
    }
    node = node->hash_id_next;
  }
  printf("Key: '%s' not found at index: %u\n", key, idx);
  return NULL;
}

// 删除数据
void data_delete(const char* id) {
  unsigned int idx = hash_key(id);
  DataNode* node = hash_table_id[idx];
  while (node) {
    if (strcmp(node->id, id) == 0) {
      delete_node(node);
      printf("Key: '%s' deleted at index: %u\n", id, idx);
      return;
    }
    node = node->hash_id_next;
  }
  printf("Key: '%s' not found for deletion at index: %u\n", id, idx);
}

// 修改数据
void data_modify(const char* key, const char* new_value) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_table_name[idx];
  while (node) {
    if (strcmp(node->name, key) == 0) {
      free(node->value);
      node->value = strdup(new_value);
      printf("Key: '%s' modified at index: %u, new value: '%s'\n", key, idx,
             new_value);
      return;
    }
    node = node->hash_name_next;
  }
  node = hash_table_id[idx];
  while (node) {
    if (strcmp(node->id, key) == 0) {
      free(node->value);
      node->value = strdup(new_value);
      printf("Key: '%s' modified at index: %u, new value: '%s'\n", key, idx,
             new_value);
      return;
    }
    node = node->hash_id_next;
  }
  printf("Key: '%s' not found for modification at index: %u\n", key, idx);
}

// 保存数据到文件
void data_save(const char* filename) {
  FILE* file = fopen(filename, "w");
  if (!file) {
    perror("Failed to open file for saving");
    return;
  }
  DataNode* node = head;
  while (node) {
    fprintf(file, "%s,%s,%s\n", node->name, node->id, node->value);
    node = node->next;
  }
  fclose(file);
  printf("Data saved to %s\n", filename);
}

// 从文件加载数据
void data_load(const char* filename) {
  FILE* file = fopen(filename, "r");
  if (!file) {
    perror("Failed to open file for loading");
    return;
  }
  char line[256];
  while (fgets(line, sizeof(line), file)) {
    char* name = strtok(line, ",");
    char* id = strtok(NULL, ",");
    char* value = strtok(NULL, "\n");
    if (name && id && value) {
      data_insert(name, id, value);
    }
  }
  fclose(file);
  printf("Data loaded from %s\n", filename);
}

// 退出程序并释放内存
void data_exit() {
  DataNode* node = head;
  while (node) {
    DataNode* next = node->next;
    free(node->name);
    free(node->id);
    free(node->value);
    free(node);
    node = next;
  }
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hash_table_name[i] = NULL;
    hash_table_id[i] = NULL;
  }
  head = NULL;
  tail = NULL;
  printf("Data exited and memory freed\n");
  exit(EXIT_SUCCESS);
}
