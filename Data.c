#include "Data.h"

#include <stdlib.h>
#include <string.h>

#include "Hash.h"
#include "List.h"
#include "Stack.h"
#include "Trie.h"
#include "memory.h"
#include "yyjson.h"

/*
 * file_modified 用来记录“当前内存数据是否和磁盘文件不同”。
 * 只要发生用户可见的数据变更，就把它设为 1。
 * 成功导入 JSON、成功保存加密文件、成功新建空文件后，再把它清回 0。
 */
static int file_modified = 0;

int data_is_modified(void) {
  /* 直接返回当前脏标记。 */
  return file_modified;
}

static int is_non_empty(const char* text) {
  /* text 非空且首字符不是 '\0' 时，说明这是一个有效的非空字符串。 */
  return text != NULL && text[0] != '\0';
}

static DataNode* create_list_node(const char* name, const char* id,
                                  const char* value) {
  /* 先为节点结构体本身分配内存。 */
  DataNode* node = malloc(sizeof(*node));

  /* 检查节点分配是否成功。 */
  memory_check(node);

  /* 深拷贝姓名，避免外部缓冲区后续变化影响节点内部数据。 */
  node->name = strdup(name);

  /* 检查姓名复制是否成功。 */
  memory_check(node->name);

  /* 深拷贝 ID。 */
  node->id = strdup(id);

  /* 检查 ID 复制是否成功。 */
  memory_check(node->id);

  /* 深拷贝 value。 */
  node->value = strdup(value);

  /* 检查 value 复制是否成功。 */
  memory_check(node->value);

  /* 新节点还没有进入哈希桶链，所以先把 hash_next 清空。 */
  node->hash_next = NULL;

  /* 新节点还没有进入双向链表，所以 prev 先置空。 */
  node->prev = NULL;

  /* 新节点还没有进入双向链表，所以 next 也先置空。 */
  node->next = NULL;

  /* 返回构造好的完整节点。 */
  return node;
}

static StackNode* create_stack_node(int type, const char* id, const char* name,
                                    const char* value) {
  /* 为栈节点分配内存。 */
  StackNode* node = malloc(sizeof(*node));

  /* op 是一个临时操作记录，后面会整体拷贝进 node->data。 */
  Operator op;

  /* 检查栈节点分配是否成功。 */
  memory_check(node);

  /* 记录操作类型。 */
  op.type = type;

  /* 深拷贝 ID，保证撤销时仍然能访问原始内容。 */
  op.id = strdup(id);

  /* 检查 ID 拷贝是否成功。 */
  memory_check(op.id);

  /* 深拷贝姓名。 */
  op.name = strdup(name);

  /* 检查姓名拷贝是否成功。 */
  memory_check(op.name);

  /* 深拷贝 value。 */
  op.value = strdup(value);

  /* 检查 value 拷贝是否成功。 */
  memory_check(op.value);

  /* 把完整的操作记录写入栈节点。 */
  node->data = op;

  /* 新栈节点还没有压栈，所以 next 先置空。 */
  node->next = NULL;

  /* 返回构造完成的撤销节点。 */
  return node;
}

static void delete_node(DataNode* node) {
  /* 先从双向链表里摘掉节点，维护顺序结构的一致性。 */
  list_remove(node);

  /* 再从哈希表里移除节点，避免后续还能按 ID 查到它。 */
  hash_remove(node);

  /* 再从姓名 Trie 中删除对应指针，避免前缀搜索命中悬空记录。 */
  trie_delete(node);

  /* 释放节点内部的姓名字符串。 */
  free(node->name);

  /* 释放节点内部的 ID 字符串。 */
  free(node->id);

  /* 释放节点内部的 value 字符串。 */
  free(node->value);

  /* 最后释放节点本身。 */
  free(node);
}

static void free_string_arrays(const char** names, const char** ids,
                               const char** values) {
  /* names / ids / values 都只是指针数组，数组里的字符串由 yyjson 文档统一管理。 */
  free(names);
  free(ids);
  free(values);
}

static DataStatus import_doc(yyjson_doc* doc) {
  /* root 是 JSON 根节点。 */
  yyjson_val* root;

  /* count 是数组元素总数。 */
  size_t count;

  /* names / ids / values 暂存每条记录的字符串指针。 */
  const char** names = NULL;
  const char** ids = NULL;
  const char** values = NULL;

  /* idx 和 max 用于 yyjson 的数组遍历宏。 */
  size_t idx;
  size_t max;

  /* item 表示当前遍历到的数组元素。 */
  yyjson_val* item;

  /* 取出根节点。 */
  root = yyjson_doc_get_root(doc);

  /* 根节点不是数组时，格式不符合系统约定。 */
  if (!yyjson_is_arr(root)) {
    return DATA_FORMAT_ERROR;
  }

  /* 获取数组中的记录数量。 */
  count = yyjson_arr_size(root);

  /* 有记录时才为临时指针数组分配空间。 */
  if (count > 0) {
    names = malloc(sizeof(*names) * count);
    ids = malloc(sizeof(*ids) * count);
    values = malloc(sizeof(*values) * count);
    memory_check(names);
    memory_check(ids);
    memory_check(values);
  }

  /* idx 由 yyjson_arr_foreach 宏自动维护。 */
  idx = 0;

  /* 逐项检查 JSON 数组中的每条记录。 */
  yyjson_arr_foreach(root, idx, max, item) {
    /* 取出当前对象的 name 字段。 */
    yyjson_val* name_val = yyjson_obj_get(item, "name");

    /* 取出当前对象的 id 字段。 */
    yyjson_val* id_val = yyjson_obj_get(item, "id");

    /* 取出当前对象的 value 字段。 */
    yyjson_val* value_val = yyjson_obj_get(item, "value");

    /* 只接受三个字段都存在且都是非空字符串的记录。 */
    if (!yyjson_is_str(name_val) || !yyjson_is_str(id_val) ||
        !yyjson_is_str(value_val) || !is_non_empty(yyjson_get_str(name_val)) ||
        !is_non_empty(yyjson_get_str(id_val)) ||
        !is_non_empty(yyjson_get_str(value_val))) {
      free_string_arrays(names, ids, values);
      return DATA_FORMAT_ERROR;
    }

    /* 暂存 name 字符串指针。 */
    names[idx] = yyjson_get_str(name_val);

    /* 暂存 id 字符串指针。 */
    ids[idx] = yyjson_get_str(id_val);

    /* 暂存 value 字符串指针。 */
    values[idx] = yyjson_get_str(value_val);
  }

  /* 先把当前内存结构重置为空，准备装载新文件内容。 */
  data_init();

  /* 把所有记录逐条插入系统内部结构。 */
  /* 重复 ID 直接复用哈希表的唯一性校验。 */
  for (size_t i = 0; i < count; ++i) {
    DataStatus status = data_insert(names[i], ids[i], values[i], 0);

    /* 任意一条插入失败都视为整体加载失败。 */
    if (status != DATA_OK) {
      /* 回滚到空状态，避免出现半加载状态。 */
      data_init();

      /* 释放临时数组。 */
      free_string_arrays(names, ids, values);

      /* 内部如果报重复 ID，统一归并为文件格式错误。 */
      return status == DATA_DUPLICATE_ID ? DATA_FORMAT_ERROR : status;
    }
  }

  /* 临时数组已经用完，可以释放。 */
  free_string_arrays(names, ids, values);

  /* 导入成功后，内存与文件同步，脏标记清零。 */
  file_modified = 0;

  /* 返回成功。 */
  return DATA_OK;
}

void data_init(void) {
  /* 重建哈希索引。 */
  hash_init();

  /* 重建双向链表，同时释放旧节点。 */
  list_init();

  /* 清空撤销栈。 */
  stack_init();

  /* 重建 Trie 索引。 */
  trie_init();

  /* 初始化后的内存状态视为干净。 */
  file_modified = 0;
}

void data_mark_clean(void) {
  /* 保存成功后，把当前脏标记清零。 */
  file_modified = 0;
}

DataStatus data_insert(const char* name, const char* id, const char* value,
                       int op_user) {
  /* new_node 保存真正要插入三套结构中的记录节点。 */
  DataNode* new_node;

  /* 任一字段为空都判定为非法输入。 */
  if (!is_non_empty(name) || !is_non_empty(id) || !is_non_empty(value)) {
    return DATA_INVALID_ARGUMENT;
  }

  /* ID 已存在时直接拒绝，保证主键唯一。 */
  if (hash_find(id)) {
    return DATA_DUPLICATE_ID;
  }

  /* 为新记录创建独立节点。 */
  new_node = create_list_node(name, id, value);

  /* 用户主动操作时，把这次插入记入撤销栈。 */
  if (op_user) {
    stack_push(create_stack_node(OP_INSERT, id, name, value));
  }

  /* 加入哈希表，支持按 ID 快速查找。 */
  hash_add(new_node);

  /* 加入双向链表头部，维护整体遍历顺序。 */
  list_push_front(new_node);

  /* 加入 Trie，支持按姓名前缀查询。 */
  trie_insert(new_node);

  /* 插入会改变数据内容，所以标记为已修改。 */
  file_modified = 1;

  /* 返回成功状态。 */
  return DATA_OK;
}

const DataNode* data_find_by_id(const char* id) {
  /* 空 ID 没有查找意义，直接返回 NULL。 */
  if (!is_non_empty(id)) {
    return NULL;
  }

  /* 其余情况直接交给哈希表精确查找。 */
  return hash_find(id);
}

DataStatus data_delete(const char* id, int op_user) {
  /* node 用来保存查找到的目标节点。 */
  DataNode* node;

  /* 空 ID 视为非法参数。 */
  if (!is_non_empty(id)) {
    return DATA_INVALID_ARGUMENT;
  }

  /* 先按 ID 精确定位目标节点。 */
  node = hash_find(id);

  /* 没找到时返回 DATA_NOT_FOUND。 */
  if (!node) {
    return DATA_NOT_FOUND;
  }

  /* 用户主动删除时，先把旧内容压入撤销栈。 */
  if (op_user) {
    stack_push(create_stack_node(OP_DELETE, id, node->name, node->value));
  }

  /* 真正删除节点并维护三套结构。 */
  delete_node(node);

  /* 删除同样会让内存状态变脏。 */
  file_modified = 1;

  /* 返回成功。 */
  return DATA_OK;
}

DataStatus data_modify(const char* id, const char* new_value, int op_user) {
  /* node 保存待修改的目标节点。 */
  DataNode* node;

  /* ID 或新值为空都算非法输入。 */
  if (!is_non_empty(id) || !is_non_empty(new_value)) {
    return DATA_INVALID_ARGUMENT;
  }

  /* 按 ID 查找要修改的记录。 */
  node = hash_find(id);

  /* 没找到时返回未找到状态。 */
  if (!node) {
    return DATA_NOT_FOUND;
  }

  /* 用户主动修改时，把旧值压入撤销栈。 */
  if (op_user) {
    stack_push(create_stack_node(OP_MODIFY, node->id, node->name, node->value));
  }

  /* 先释放旧 value，避免内存泄漏。 */
  free(node->value);

  /* 再复制新 value。 */
  node->value = strdup(new_value);

  /* 检查新字符串分配是否成功。 */
  memory_check(node->value);

  /* 修改后标记为脏。 */
  file_modified = 1;

  /* 返回成功。 */
  return DATA_OK;
}

DataStatus data_export_json(char** out_json, size_t* out_len) {
  /* doc 是待输出的可写 JSON 文档。 */
  yyjson_mut_doc* doc;

  /* root 是文档根数组。 */
  yyjson_mut_val* root;

  /* node 用来从链表尾向前遍历所有记录。 */
  DataNode* node;

  /* json_text 保存序列化后的 JSON 字符串。 */
  char* json_text;

  /* 输出参数为空时直接拒绝。 */
  if (!out_json || !out_len) {
    return DATA_INVALID_ARGUMENT;
  }

  /* 先把输出指针清成空，避免调用方读到脏值。 */
  *out_json = NULL;
  *out_len = 0;

  /* 创建一个新的可写 JSON 文档。 */
  doc = yyjson_mut_doc_new(NULL);

  /* 创建根数组。 */
  root = yyjson_mut_arr(doc);

  /* 把根数组设置到文档里。 */
  yyjson_mut_doc_set_root(doc, root);

  /* 从链表尾部开始向前遍历，保持保存顺序与最初插入顺序一致。 */
  node = list_tail();
  while (node) {
    /* 为当前记录在根数组里追加一个对象元素。 */
    yyjson_mut_val* item = yyjson_mut_arr_add_obj(doc, root);

    /* 写入 name 字段。 */
    yyjson_mut_obj_add_str(doc, item, "name", node->name);

    /* 写入 id 字段。 */
    yyjson_mut_obj_add_str(doc, item, "id", node->id);

    /* 写入 value 字段。 */
    yyjson_mut_obj_add_str(doc, item, "value", node->value);

    /* 继续访问更早插入的一条记录。 */
    node = node->prev;
  }

  /* 把完整文档序列化成一段动态字符串。 */
  json_text = yyjson_mut_write(doc, 0, out_len);

  /* 写完后释放 JSON 文档对象。 */
  yyjson_mut_doc_free(doc);

  /* 序列化失败说明内存申请没有成功。 */
  memory_check(json_text);

  /* 把结果交给调用方。 */
  *out_json = json_text;

  /* 返回成功。 */
  return DATA_OK;
}

DataStatus data_import_json(const char* json_text, size_t json_len) {
  /* doc 保存从文件解析出来的只读 JSON 文档。 */
  yyjson_doc* doc;

  /* status 保存最终导入结果。 */
  DataStatus status;

  /* 文本指针为空时直接报参数错误。 */
  if (!json_text) {
    return DATA_INVALID_ARGUMENT;
  }

  /* 当前有未保存修改时，拒绝直接加载。 */
  if (file_modified) {
    return DATA_DIRTY_BLOCKED;
  }

  /* 读取并解析 JSON 文本。 */
  doc = yyjson_read(json_text, json_len, 0);

  /* JSON 无法解析时，统一返回格式错误。 */
  if (!doc) {
    return DATA_FORMAT_ERROR;
  }

  /* 把真实导入逻辑交给公共帮助函数。 */
  status = import_doc(doc);

  /* 释放 JSON 文档。 */
  yyjson_doc_free(doc);

  /* 返回最终状态。 */
  return status;
}

DataStatus data_undo(void) {
  /* op 指向当前栈顶操作。 */
  StackNode* op;

  /* status 保存执行逆操作后的结果。 */
  DataStatus status = DATA_INVALID_ARGUMENT;

  /* 没有历史记录时无法撤销。 */
  if (stack_is_empty()) {
    return DATA_EMPTY_UNDO;
  }

  /* 读取栈顶操作内容。 */
  op = stack_get_top();

  /* 根据原操作类型执行对应的逆操作。 */
  switch (op->data.type) {
    case OP_INSERT:
      /* 撤销插入就是把刚插入的记录删除掉。 */
      status = data_delete(op->data.id, 0);
      break;

    case OP_DELETE:
      /* 撤销删除就是按原始内容重新插回去。 */
      status = data_insert(op->data.name, op->data.id, op->data.value, 0);
      break;

    case OP_MODIFY:
      /* 撤销修改就是把旧 value 写回去。 */
      status = data_modify(op->data.id, op->data.value, 0);
      break;
  }

  /* 逆操作成功后，原历史记录已经消费完成，可以弹栈。 */
  if (status == DATA_OK) {
    stack_pop();
  }

  /* 把最终状态返回给上层。 */
  return status;
}
