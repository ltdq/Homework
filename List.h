#ifndef LIST_H
#define LIST_H

typedef struct DataNode DataNode;

void list_init(void);
void list_push_front(DataNode* node);
void list_remove(DataNode* node);
DataNode* list_head(void);
DataNode* list_tail(void);

#endif
