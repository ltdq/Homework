#include "Data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "List.h"
#include "Stack.h"
#include "memory.h"
#include "yyjson.h"

extern int file_modified;

// 创建新的数据节点
static DataNode* create_list_node(const char* name, const char* id,
                                  const char* value) {
  DataNode* node = (DataNode*)malloc(sizeof(DataNode));
  memory_check(node);
  node->name = strdup(name);
  memory_check(node->name);
  node->id = strdup(id);
  memory_check(node->id);
  node->value = strdup(value);
  memory_check(node->value);
  node->hash_name_next = NULL;
  node->hash_id_next = NULL;
  node->prev = NULL;
  node->next = NULL;
  return node;
}

static StackNode* create_stack_node(int type, const char* id, const char* name,
                                    const char* value) {
  StackNode* node = (StackNode*)malloc(sizeof(StackNode));
  memory_check(node);
  Operator op;
  op.type = type;
  op.id = strdup(id);
  memory_check(op.id);
  op.name = strdup(name);
  memory_check(op.name);
  op.value = strdup(value);
  memory_check(op.value);
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
  file_modified = 0;
  printf("所有数据结构已初始化\n");
}

// 插入数据
void data_insert(const char* name, const char* id, const char* value,
                 int op_user) {
  if (name == NULL || id == NULL || value == NULL) {
    printf("输入参数无效\n");
    return;
  }
  // 通过id，因为id是唯一的，name可能重复
  DataNode* existing_node = hash_find_by_id(id);
  while (existing_node) {
    if (strcmp(existing_node->id, id) == 0) {
      printf("'%s' 已经存在\n", id);
      return;
    }
    existing_node = existing_node->hash_id_next;
  }

  DataNode* new_node = create_list_node(name, id, value);
  if (op_user) {
    StackNode* op = create_stack_node(OP_INSERT, id, name, value);
    stack_push(op);
  }
  hash_add(new_node);
  list_push_front(new_node);
  file_modified = 1;
  printf("添加：名字： '%s', ID: '%s', 值: '%s'\n", name, id, value);
}

// 获取数据
void data_get(const char* key) {
  if (key == NULL) {
    printf("输入内容非法\n");
    return;
  }
  int is_already_printed = 0;
  DataNode* node = hash_find_by_name(key);
  // 可能存在多个同名的节点，所以需要遍历桶链表
  while (node) {
    if (strcmp(node->name, key) == 0) {
      printf("'%s' 的值: '%s'\n", key, node->value);
      list_push_visited(node);
      is_already_printed = 1;
    }
    node = node->hash_name_next;
  }

  // id是唯一的，所以直接返回第一个匹配的节点即可
  DataNode* node_id = hash_find_by_id(key);
  while (node_id) {
    if (strcmp(node_id->id, key) == 0 && is_visited(node_id) == 0) {
      printf("'%s' 的值: '%s'\n", key, node_id->value);
      list_push_visited(node_id);
      return;
    }
    node_id = node_id->hash_id_next;
  }
  if (!is_already_printed) {
    printf("'%s'没有找到 \n", key);
  }
  list_clear_visited();
}

// 删除数据
void data_delete(const char* id, int op_user) {
  if (id == NULL) {
    printf("输入内容非法\n");
    return;
  }
  DataNode* node = hash_find_by_id(id);
  while (node) {
    if (strcmp(node->id, id) == 0) {
      if (op_user) {
        StackNode* op =
            create_stack_node(OP_DELETE, id, node->name, node->value);
        stack_push(op);
      }
      delete_node(node);
      file_modified = 1;
      printf("'%s' 已被删除\n", id);
      return;
    }
    node = node->hash_id_next;
  }
  printf("'%s' 没有找到用于删除\n", id);
}

// 修改数据
void data_modify(const char* key, const char* new_value, int op_user) {
  if (key == NULL || new_value == NULL) {
    printf("输入内容非法\n");
    return;
  }
  DataNode* node = hash_find_by_name(key);
  // 判断的节点
  if (node) {
    int count = 0;
    while (node) {
      if (strcmp(node->name, key) == 0) {
        // 对重名节点进行计数
        count++;
      }
      node = node->hash_name_next;
    }
    // 如果存在多个同名节点，提示用户使用id进行修改
    if (count > 1) {
      printf("'%s' 存在多个条目。请使用ID来修改特定的条目。\n", key);
      return;
    } else if (count == 1) {
      // 如果只有一个同名节点，直接修改
      node = hash_find_by_name(key);
      while (node) {
        if (strcmp(node->name, key) == 0) {
          break;
        }
        node = node->hash_name_next;
      }
    } else {
      // 如果没有正确name（仅 Hash 冲突）的节点，再尝试通过id查找
      node = hash_find_by_id(key);
      while (node) {
        if (strcmp(node->id, key) == 0) {
          break;
        }
        node = node->hash_id_next;
      }
    }
  } else {
    // 如果没有name的节点，再尝试通过id查找
    node = hash_find_by_id(key);
    while (node) {
      if (strcmp(node->id, key) == 0) {
        break;
      }
      node = node->hash_id_next;
    }
  }
  // 如果找到了节点，进行修改
  if (node) {
    if (op_user) {
      StackNode* op =
          create_stack_node(OP_MODIFY, node->id, node->name, node->value);
      stack_push(op);
    }
    free(node->value);
    node->value = strdup(new_value);
    memory_check(node->value);
    file_modified = 1;
    printf("'%s' 的值已修改为: '%s'\n", key, new_value);
    return;
  }
  printf("'%s' 没有找到用于修改\n", key);
}

// 创建新的数据文件
void data_new(const char* filename) {
  if (file_modified) {
    printf("数据已被修改，请在创建新数据文件前保存数据\n");
    return;
  }
  yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val* root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);
  int status = yyjson_mut_write_file(filename, doc, 0, NULL, NULL);
  if (!status) {
    printf("创建 %s 失败\n", filename);
  } else {
    printf("数据文件创建成功: %s\n", filename);
    data_load(filename);
  }
  yyjson_mut_doc_free(doc);
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
    printf("保存数据到 %s 失败\n", filename);
  } else {
    file_modified = 0;
    printf("数据已成功保存到 %s\n", filename);
  }
  yyjson_mut_doc_free(doc);
}

// 从文件加载数据
void data_load(const char* filename) {
  if (file_modified) {
    printf("数据已被修改，请在加载数据前保存数据\n");
    return;
  } else if (list_head()) {
    printf("当前数据不为空但已保存，将被替换\n");
  }
  yyjson_doc* doc = yyjson_read_file(filename, 0, NULL, NULL);
  if (!doc) {
    printf("从 %s 加载数据失败\n", filename);
    return;
  }
  yyjson_val* root = yyjson_doc_get_root(doc);
  if (!yyjson_is_arr(root)) {
    printf("输入文件 %s 格式不正确\n", filename);
    yyjson_doc_free(doc);
    return;
  }
  // 加载数据前先清空当前数据
  data_init();
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
  printf("数据已从 %s 加载\n", filename);
  yyjson_doc_free(doc);
  file_modified = 0;
}

// 撤销上一次操作
void data_undo() {
  if (stack_is_empty()) {
    printf("没有可撤销的操作\n");
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
    case OP_MODIFY:
      // 这里直接通过id进行修改，因为id是唯一的
      data_modify(op->data.id, op->data.value, 0);
      break;
    default:
      printf("未知操作类型: %d\n", op->data.type);
      break;
  }
  stack_pop();
}
