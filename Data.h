#ifndef DATA_H
#define DATA_H

// 数据节点结构体定义
typedef struct DataNode {
  char* name;
  char* id;
  char* value;
  struct DataNode* hash_name_next;
  struct DataNode* hash_id_next;
  struct DataNode* prev;
  struct DataNode* next;
} DataNode;

// 数据操作函数声明
void data_init(void);
void data_insert(const char* name, const char* id, const char* value,
                 int op_user);
char* data_get(const char* key);
void data_delete(const char* id, int op_user);
void data_modify(const char* key, const char* new_value, int op_user);
void data_save(const char* filename);
void data_load(const char* filename);
void data_undo(void);
void data_exit(void);

#endif
