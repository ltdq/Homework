#include "Data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "List.h"
#include "Stack.h"
#include "yyjson.h"

// 创建新的数据节点
static DataNode* create_list_node(const char* name, const char* id,
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

static StackNode* create_stack_node(int type, const char* id, const char* name,
                                    const char* value) {
  StackNode* node = (StackNode*)malloc(sizeof(StackNode));
  Operator op;
  op.type = type;
  op.id = strdup(id);
  op.name = strdup(name);
  op.value = strdup(value);
  node->data = op;
  node->next = NULL;
  return node;
}

// 删除数据节点并更新哈希表和链表指针
static void delete_node(DataNode* node) {
  list_remove(node);
  hash_remove(node);
  free(node->name);
  free(node->id);
  free(node->value);
  free(node);
}

// 初始化数据结构
void data_init() {
  hash_init();
  list_init();
  stack_init();
  printf("Hash Done, size: %d\n", HASH_TABLE_SIZE);
}

// 插入数据
void data_insert(const char* name, const char* id, const char* value,
                 int op_user) {
  unsigned int idx_name = hash_key(name);
  if (hash_find_exact(name, id)) {
    printf(
        "Name: '%s' already exists at index: %u, should be modified instead "
        "of inserted\n",
        name, idx_name);
    return;
  }

  DataNode* new_node = create_list_node(name, id, value);
  if (op_user) {
    StackNode* op = create_stack_node(OP_INSERT, id, name, value);
    stack_push(op);
  }
  hash_add(new_node);
  list_push_front(new_node);
  printf("Insert name: '%s', id: '%s', value: '%s' at index: %u\n", name, id,
         value, idx_name);
}

// 获取数据
char* data_get(const char* key) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_find_by_name(key);
  if (node) {
    printf("Key: '%s' found at index: %u, value: '%s'\n", key, idx,
           node->value);
    return node->value;
  }

  node = hash_find_by_id(key);
  if (node) {
    printf("Key: '%s' found at index: %u, value: '%s'\n", key, idx,
           node->value);
    return node->value;
  }

  printf("Key: '%s' not found at index: %u\n", key, idx);
  return NULL;
}

// 删除数据
void data_delete(const char* id, int op_user) {
  unsigned int idx = hash_key(id);
  DataNode* node = hash_find_by_id(id);
  if (node) {
    if (op_user) {
      StackNode* op = create_stack_node(OP_DELETE, id, node->name, node->value);
      stack_push(op);
    }
    delete_node(node);
    printf("Key: '%s' deleted at index: %u\n", id, idx);
    return;
  }
  printf("Key: '%s' not found for deletion at index: %u\n", id, idx);
}

// 修改数据
void data_modify(const char* key, const char* new_value, int op_user) {
  unsigned int idx = hash_key(key);
  DataNode* node = hash_find_by_name(key);
  if (node) {
    if (op_user) {
      StackNode* op =
          create_stack_node(OP_MODIFY_NAME, node->id, node->name, node->value);
      stack_push(op);
    }
    free(node->value);
    node->value = strdup(new_value);
    printf("Key: '%s' modified at index: %u, new value: '%s'\n", key, idx,
           new_value);
    return;
  }

  node = hash_find_by_id(key);
  if (node) {
    if (op_user) {
      StackNode* op =
          create_stack_node(OP_MODIFY_ID, node->id, node->name, node->value);
      stack_push(op);
    }
    free(node->value);
    node->value = strdup(new_value);
    printf("Key: '%s' modified at index: %u, new value: '%s'\n", key, idx,
           new_value);
    return;
  }

  printf("Key: '%s' not found for modification at index: %u\n", key, idx);
}

// 保存数据到文件
void data_save(const char* filename) {
  yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val* root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);
  DataNode* node = list_tail();
  while (node) {
    yyjson_mut_val* item = yyjson_mut_arr_add_obj(doc, root);
    yyjson_mut_obj_add_str(doc, item, "name", node->name);
    yyjson_mut_obj_add_str(doc, item, "id", node->id);
    yyjson_mut_obj_add_str(doc, item, "value", node->value);
    node = node->prev;
  }
  int status = yyjson_mut_write_file(filename, doc, 0, NULL, NULL);
  if (!status) {
    printf("Failed to save data to %s\n", filename);
  } else {
    printf("Data saved to %s successfully\n", filename);
  }
  yyjson_mut_doc_free(doc);
}

// 从文件加载数据
void data_load(const char* filename) {
  yyjson_doc* doc = yyjson_read_file(filename, 0, NULL, NULL);
  if (!doc) {
    printf("Failed to load data from %s\n", filename);
    return;
  }
  yyjson_val* root = yyjson_doc_get_root(doc);
  if (!yyjson_is_arr(root)) {
    printf("Invalid data format in %s\n", filename);
    yyjson_doc_free(doc);
    return;
  }
  printf(
      "Current data will be replaced with data from %s, are you sure? (y/N): ",
      filename);
  int choice = getchar();
  int ch;
  while ((ch = getchar()) != '\n' && ch != EOF) {
  }
  if (choice != 'y' && choice != 'Y') {
    printf("Operation cancelled.\n");
    yyjson_doc_free(doc);
    return;
  }
  data_exit();
  size_t idx, max;
  yyjson_val* item;
  yyjson_arr_foreach(root, idx, max, item) {
    yyjson_val* name_val = yyjson_obj_get(item, "name");
    yyjson_val* id_val = yyjson_obj_get(item, "id");
    yyjson_val* value_val = yyjson_obj_get(item, "value");
    if (yyjson_is_str(name_val) && yyjson_is_str(id_val) &&
        yyjson_is_str(value_val)) {
      data_insert(yyjson_get_str(name_val), yyjson_get_str(id_val),
                  yyjson_get_str(value_val), 0);
    }
  }
  printf("Data loaded from %s\n", filename);
  yyjson_doc_free(doc);
}

// 撤销上一次操作
void data_undo() {
  if (stack_is_empty()) {
    printf("No operations to undo\n");
    return;
  }
  StackNode* op = stack_get_top();
  switch (op->data.type) {
    case OP_INSERT:
      data_delete(op->data.id, 0);
      break;
    case OP_DELETE:
      data_insert(op->data.name, op->data.id, op->data.value, 0);
      break;
    case OP_MODIFY_ID:
      data_modify(op->data.id, op->data.value, 0);
      break;
    case OP_MODIFY_NAME:
      data_modify(op->data.name, op->data.value, 0);
      break;
    default:
      printf("Unknown operation type: %d\n", op->data.type);
      break;
  }
  stack_pop();
}

// 退出程序并释放内存
void data_exit() {
  DataNode* node = list_head();
  while (node) {
    DataNode* next = node->next;
    free(node->name);
    free(node->id);
    free(node->value);
    free(node);
    node = next;
  }
  hash_init();
  list_init();
  stack_init();
  printf("Data exited and memory freed\n");
}
