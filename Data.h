#ifndef DATA_H
#define DATA_H

typedef enum DataStatus {
  DATA_OK = 0,
  DATA_INVALID_ARGUMENT,
  DATA_DUPLICATE_ID,
  DATA_NOT_FOUND,
  DATA_DIRTY_BLOCKED,
  DATA_IO_ERROR,
  DATA_FORMAT_ERROR,
  DATA_EMPTY_UNDO,
} DataStatus;

// 数据节点结构体定义
typedef struct DataNode {
  char* name;
  char* id;
  char* value;
  struct DataNode* hash_next;
  struct DataNode* prev;
  struct DataNode* next;
} DataNode;

// 数据操作函数声明
void data_init(void);
DataStatus data_insert(const char* name, const char* id, const char* value,
                       int op_user);
const DataNode* data_find_by_id(const char* id);
DataStatus data_delete(const char* id, int op_user);
DataStatus data_modify(const char* id, const char* new_value, int op_user);
DataStatus data_new(const char* filename);
DataStatus data_save(const char* filename);
DataStatus data_load(const char* filename);
DataStatus data_undo(void);

// 获取文件修改状态
int data_is_modified(void);

#endif
