#ifndef MEMORY_H
#define MEMORY_H

/*
 * 检查内存分配是否成功。
 * 传入空指针时，函数会记录日志并直接终止程序。
 */
void memory_check(void* ptr);

#endif
