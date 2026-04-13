#ifndef LOG_H
#define LOG_H

// 日志缓冲区大小
#define LOG_MAX_LINES 100
#define LOG_MAX_LINE_LENGTH 256

// 初始化日志系统
void log_init(void);

// 添加日志消息
void log_print(const char* format, ...);

// 获取日志行数
int log_get_line_count(void);

// 获取指定行的日志内容
// 返回: 日志内容指针，越界时返回 NULL
const char* log_get_line(int index);

#endif
