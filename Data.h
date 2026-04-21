#ifndef DATA_H
#define DATA_H

#include <stddef.h>

/*
 * DataStatus 用来描述数据层操作的最终结果。
 * 显示层只需要读取这个状态码，就可以决定提示用户什么信息。
 */
typedef enum DataStatus {
  /* 操作执行成功。 */
  DATA_OK = 0,
  /* 传入的参数为空指针、空字符串，或者整体不符合要求。 */
  DATA_INVALID_ARGUMENT,
  /* 插入时发现 ID 已经存在，数据层拒绝重复主键。 */
  DATA_DUPLICATE_ID,
  /* 按指定 ID 查找、删除、修改时没有找到对应记录。 */
  DATA_NOT_FOUND,
  /* 当前有未保存修改，数据层阻止覆盖式加载或新建。 */
  DATA_DIRTY_BLOCKED,
  /* 文件格式不符合系统预期，例如 JSON 结构错误或字段缺失。 */
  DATA_FORMAT_ERROR,
  /* 用户请求撤销时，操作栈已经为空。 */
  DATA_EMPTY_UNDO,
} DataStatus;

/*
 * DataNode 是系统里最核心的记录结构。
 * 同一个节点会同时被三套结构共享：
 * 1. 哈希表通过 hash_next 把同桶元素串起来。
 * 2. 双向链表通过 prev / next 维护整体遍历顺序。
 * 3. Trie 的叶子节点会保存这个节点的指针，用于按姓名前缀检索。
 */
typedef struct DataNode {
  /* 学生姓名。 */
  char* name;
  /* 学号或唯一 ID。 */
  char* id;
  /* 附加值，这个项目里通常是成绩或说明文本。 */
  char* value;
  /* 哈希桶冲突链的后继指针。 */
  struct DataNode* hash_next;
  /* 双向链表中的前驱指针。 */
  struct DataNode* prev;
  /* 双向链表中的后继指针。 */
  struct DataNode* next;
} DataNode;

/*
 * 初始化整个数据层。
 * 这个函数会重建哈希表、链表、栈和 Trie，并清空“已修改”标记。
 */
void data_init(void);

/*
 * 插入一条新记录。
 * op_user 为 1 时表示这是用户主动操作，需要记录到撤销栈。
 * op_user 为 0 时表示这是系统内部恢复动作，例如导入 JSON 或撤销时回放。
 */
DataStatus data_insert(const char* name, const char* id, const char* value,
                       int op_user);

/*
 * 按 ID 精确查找记录。
 * 成功时返回只读节点指针，失败时返回 NULL。
 */
const DataNode* data_find_by_id(const char* id);

/*
 * 按 ID 删除记录。
 * op_user 的含义与 data_insert 相同。
 */
DataStatus data_delete(const char* id, int op_user);

/*
 * 按 ID 修改记录的 value 字段。
 * op_user 的含义与 data_insert 相同。
 */
DataStatus data_modify(const char* id, const char* new_value, int op_user);

/*
 * 把当前链表中的所有记录导出成一段 JSON 文本。
 * 调用方负责释放 out_json 返回的动态缓冲区。
 */
DataStatus data_export_json(char** out_json, size_t* out_len);

/*
 * 从一段 JSON 文本加载数据，并重建当前内存结构。
 * 仅在当前没有未保存修改时允许执行。
 */
DataStatus data_import_json(const char* json_text, size_t json_len);

/*
 * 在保存成功后，把当前内存状态标记成“已保存”。
 */
void data_mark_clean(void);

/*
 * 撤销最近一次用户操作。
 * 支持撤销插入、删除、修改。
 */
DataStatus data_undo(void);

/*
 * 返回当前数据是否发生了尚未保存的修改。
 * 返回 1 表示已修改，返回 0 表示内容与最近一次保存/加载后的状态一致。
 */
int data_is_modified(void);

#endif
