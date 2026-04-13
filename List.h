#ifndef LIST_H
#define LIST_H

/* 前向声明可以让链表模块只依赖节点类型名，而不展开完整结构。 */
typedef struct DataNode DataNode;

/* 初始化双向链表，并释放旧链表中残留的所有节点。 */
void list_init(void);

/* 把节点插入到链表头部。 */
void list_push_front(DataNode* node);

/* 把指定节点从链表中摘掉。 */
void list_remove(DataNode* node);

/* 返回链表头节点，链表为空时返回 NULL。 */
DataNode* list_head(void);

/* 返回链表尾节点，链表为空时返回 NULL。 */
DataNode* list_tail(void);

#endif
