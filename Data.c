#include "Data.h"

#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "List.h"
#include "Stack.h"
#include "Trie.h"
#include "memory.h"
#include "yyjson.h"

static int file_modified = 0;

int data_is_modified(void) { return file_modified; }

static int is_non_empty(const char* text) {
  return text != NULL && text[0] != '\0';
}

static DataNode* create_list_node(const char* name, const char* id,
                                  const char* value) {
  DataNode* node = malloc(sizeof(*node));
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
  StackNode* node = malloc(sizeof(*node));
  Operator op;

  memory_check(node);

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

static void delete_node(DataNode* node) {
  list_remove(node);
  hash_remove(node);
  trie_delete(node);
  free(node->name);
  free(node->id);
  free(node->value);
  free(node);
}

static void free_string_arrays(const char** names, const char** ids,
                               const char** values) {
  free(names);
  free(ids);
  free(values);
}

void data_init(void) {
  hash_init();
  list_init();
  stack_init();
  trie_init();
  file_modified = 0;
}

DataStatus data_insert(const char* name, const char* id, const char* value,
                       int op_user) {
  DataNode* new_node;

  if (!is_non_empty(name) || !is_non_empty(id) || !is_non_empty(value)) {
    return DATA_INVALID_ARGUMENT;
  }
  if (hash_find(id)) {
    return DATA_DUPLICATE_ID;
  }

  new_node = create_list_node(name, id, value);
  if (op_user) {
    stack_push(create_stack_node(OP_INSERT, id, name, value));
  }
  hash_add(new_node);
  list_push_front(new_node);
  trie_insert(new_node);
  file_modified = 1;
  return DATA_OK;
}

const DataNode* data_find_by_id(const char* id) {
  if (!is_non_empty(id)) {
    return NULL;
  }
  return hash_find(id);
}

DataStatus data_delete(const char* id, int op_user) {
  DataNode* node;

  if (!is_non_empty(id)) {
    return DATA_INVALID_ARGUMENT;
  }

  node = hash_find(id);
  if (!node) {
    return DATA_NOT_FOUND;
  }

  if (op_user) {
    stack_push(create_stack_node(OP_DELETE, id, node->name, node->value));
  }
  delete_node(node);
  file_modified = 1;
  return DATA_OK;
}

DataStatus data_modify(const char* id, const char* new_value, int op_user) {
  DataNode* node;

  if (!is_non_empty(id) || !is_non_empty(new_value)) {
    return DATA_INVALID_ARGUMENT;
  }

  node = hash_find(id);
  if (!node) {
    return DATA_NOT_FOUND;
  }

  if (op_user) {
    stack_push(create_stack_node(OP_MODIFY, node->id, node->name, node->value));
  }
  free(node->value);
  node->value = strdup(new_value);
  memory_check(node->value);
  file_modified = 1;
  return DATA_OK;
}

DataStatus data_new(const char* filename) {
  yyjson_mut_doc* doc;
  yyjson_mut_val* root;
  int status;

  if (!is_non_empty(filename)) {
    return DATA_INVALID_ARGUMENT;
  }
  if (file_modified) {
    return DATA_DIRTY_BLOCKED;
  }

  doc = yyjson_mut_doc_new(NULL);
  root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);
  status = yyjson_mut_write_file(filename, doc, 0, NULL, NULL);
  yyjson_mut_doc_free(doc);
  if (!status) {
    return DATA_IO_ERROR;
  }

  data_init();
  file_modified = 0;
  return DATA_OK;
}

DataStatus data_save(const char* filename) {
  yyjson_mut_doc* doc;
  yyjson_mut_val* root;
  DataNode* node;
  int status;

  if (!is_non_empty(filename)) {
    return DATA_INVALID_ARGUMENT;
  }

  doc = yyjson_mut_doc_new(NULL);
  root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  node = list_tail();
  while (node) {
    yyjson_mut_val* item = yyjson_mut_arr_add_obj(doc, root);
    yyjson_mut_obj_add_str(doc, item, "name", node->name);
    yyjson_mut_obj_add_str(doc, item, "id", node->id);
    yyjson_mut_obj_add_str(doc, item, "value", node->value);
    node = node->prev;
  }

  status = yyjson_mut_write_file(filename, doc, 0, NULL, NULL);
  yyjson_mut_doc_free(doc);
  if (!status) {
    return DATA_IO_ERROR;
  }

  file_modified = 0;
  return DATA_OK;
}

DataStatus data_load(const char* filename) {
  yyjson_doc* doc;
  yyjson_val* root;
  size_t count;
  const char** names = NULL;
  const char** ids = NULL;
  const char** values = NULL;
  size_t idx;
  size_t max;
  yyjson_val* item;

  if (!is_non_empty(filename)) {
    return DATA_INVALID_ARGUMENT;
  }
  if (file_modified) {
    return DATA_DIRTY_BLOCKED;
  }

  doc = yyjson_read_file(filename, 0, NULL, NULL);
  if (!doc) {
    return DATA_IO_ERROR;
  }

  root = yyjson_doc_get_root(doc);
  if (!yyjson_is_arr(root)) {
    yyjson_doc_free(doc);
    return DATA_FORMAT_ERROR;
  }

  count = yyjson_arr_size(root);
  if (count > 0) {
    names = malloc(sizeof(*names) * count);
    ids = malloc(sizeof(*ids) * count);
    values = malloc(sizeof(*values) * count);
    memory_check(names);
    memory_check(ids);
    memory_check(values);
  }

  idx = 0;
  yyjson_arr_foreach(root, idx, max, item) {
    yyjson_val* name_val = yyjson_obj_get(item, "name");
    yyjson_val* id_val = yyjson_obj_get(item, "id");
    yyjson_val* value_val = yyjson_obj_get(item, "value");

    if (!yyjson_is_str(name_val) || !yyjson_is_str(id_val) ||
        !yyjson_is_str(value_val) || !is_non_empty(yyjson_get_str(name_val)) ||
        !is_non_empty(yyjson_get_str(id_val)) ||
        !is_non_empty(yyjson_get_str(value_val))) {
      free_string_arrays(names, ids, values);
      yyjson_doc_free(doc);
      return DATA_FORMAT_ERROR;
    }

    names[idx] = yyjson_get_str(name_val);
    ids[idx] = yyjson_get_str(id_val);
    values[idx] = yyjson_get_str(value_val);
  }

  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      if (strcmp(ids[i], ids[j]) == 0) {
        free_string_arrays(names, ids, values);
        yyjson_doc_free(doc);
        return DATA_FORMAT_ERROR;
      }
    }
  }

  data_init();
  for (size_t i = 0; i < count; ++i) {
    DataStatus status = data_insert(names[i], ids[i], values[i], 0);
    if (status != DATA_OK) {
      data_init();
      free_string_arrays(names, ids, values);
      yyjson_doc_free(doc);
      return status == DATA_DUPLICATE_ID ? DATA_FORMAT_ERROR : status;
    }
  }

  free_string_arrays(names, ids, values);
  yyjson_doc_free(doc);
  file_modified = 0;
  return DATA_OK;
}

DataStatus data_undo(void) {
  StackNode* op;
  DataStatus status = DATA_INVALID_ARGUMENT;

  if (stack_is_empty()) {
    return DATA_EMPTY_UNDO;
  }

  op = stack_get_top();
  switch (op->data.type) {
    case OP_INSERT:
      status = data_delete(op->data.id, 0);
      break;
    case OP_DELETE:
      status = data_insert(op->data.name, op->data.id, op->data.value, 0);
      break;
    case OP_MODIFY:
      status = data_modify(op->data.id, op->data.value, 0);
      break;
  }

  if (status == DATA_OK) {
    stack_pop();
  }
  return status;
}
