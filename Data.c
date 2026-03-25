#include "Data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Display.h"
#include "Hash.h"
#include "List.h"
#include "Log.h"
#include "Stack.h"
#include "Trie.h"
#include "memory.h"
#include "yyjson.h"

// 文件修改状态
static int file_modified = 0;

// 获取文件修改状态
int data_is_modified(void) { return file_modified; }

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
  node->hash_next = NULL;
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
  trie_delete(node);
  free(node->name);
  free(node->id);
  free(node->value);
  free(node);
}

// 初始化数据结构
void data_init(void) {
  hash_init();
  list_init();
  stack_init();
  trie_init();
  file_modified = 0;
}

// 插入数据
void data_insert(const char* name, const char* id, const char* value,
                 int op_user) {
  if (!name || !id || !value) {
    log_print("输入参数无效");
    return;
  }
  // 通过id，因为id是唯一的，name可能重复
  if (hash_find(id)) {
    log_print("'%s' 已经存在", id);
    return;
  }

  DataNode* new_node = create_list_node(name, id, value);
  if (op_user) {
    StackNode* op = create_stack_node(OP_INSERT, id, name, value);
    stack_push(op);
  }
  hash_add(new_node);
  list_push_front(new_node);
  trie_insert(new_node);
  file_modified = 1;
  log_print("添加: 名字='%s', ID='%s', 值='%s'", name, id, value);
}

// 获取数据
void data_get(const char* id) {
  if (!id) {
    log_print("输入内容非法");
    return;
  }
  DataNode* node_id = hash_find(id);
  if (node_id) {
    display_content_print("姓名: %s\nID: %s\n值: %s\n\n", node_id->name,
                          node_id->id, node_id->value);
  }
}

// 删除数据
void data_delete(const char* id, int op_user) {
  if (!id) {
    log_print("输入内容非法");
    return;
  }
  DataNode* node = hash_find(id);
  if (node) {
    if (op_user) {
      StackNode* op =
          create_stack_node(OP_DELETE, id, node->name, node->value);
      stack_push(op);
    }
    delete_node(node);
    file_modified = 1;
    log_print("'%s' 已被删除", id);
  } else {
    log_print("'%s' 没有找到用于删除", id);
  }
}

// 修改数据
void data_modify(const char* id, const char* new_value, int op_user) {
  if (!id || !new_value) {
    log_print("输入内容非法");
    return;
  }
  DataNode* node = hash_find(id);
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
    log_print("'%s' 的值已修改为: '%s'", id, new_value);
    return;
  }
  log_print("'%s' 没有找到用于修改", id);
}

// 创建新的数据文件
void data_new(const char* filename) {
  if (file_modified) {
    log_print("数据已被修改，请在创建新数据文件前保存数据");
    return;
  }
  yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val* root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);
  int status = yyjson_mut_write_file(filename, doc, 0, NULL, NULL);
  if (!status) {
    log_print("创建 %s 失败", filename);
  } else {
    log_print("数据文件创建成功: %s", filename);
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
    log_print("保存数据到 %s 失败", filename);
  } else {
    file_modified = 0;
    log_print("数据已成功保存到 %s", filename);
  }
  yyjson_mut_doc_free(doc);
}

// 从文件加载数据
void data_load(const char* filename) {
  if (file_modified) {
    log_print("数据已被修改，请在加载数据前保存数据");
    return;
  } else if (list_head()) {
    log_print("当前数据不为空但已保存，将被替换");
  }
  yyjson_doc* doc = yyjson_read_file(filename, 0, NULL, NULL);
  if (!doc) {
    log_print("从 %s 加载数据失败", filename);
    return;
  }
  yyjson_val* root = yyjson_doc_get_root(doc);
  if (!yyjson_is_arr(root)) {
    log_print("输入文件 %s 格式不正确", filename);
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
  log_print("数据已从 %s 加载", filename);
  yyjson_doc_free(doc);
  file_modified = 0;
}

// 撤销上一次操作
void data_undo(void) {
  if (stack_is_empty()) {
    log_print("没有可撤销的操作");
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
      data_modify(op->data.id, op->data.value, 0);
      break;
    default:
      log_print("未知操作类型: %d", op->data.type);
      break;
  }
  stack_pop();
}
