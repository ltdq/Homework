#ifndef LOG_H
#define LOG_H

/* 日志环形缓冲区最多保存 100 行。 */
#define LOG_MAX_LINES 100

/* 每一行日志最多保留 255 个可见字符和 1 个结尾空字符。 */
#define LOG_MAX_LINE_LENGTH 256

/* 初始化日志系统，把读写位置恢复到初始状态。 */
void log_init(void);

/* 追加一条格式化日志。 */
void log_print(const char* format, ...);

/* 返回当前日志区有效行数。 */
int log_get_line_count(void);

/* 按逻辑顺序获取第 index 行日志，越界时返回 NULL。 */
const char* log_get_line(int index);

#endif
