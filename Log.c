#include "Log.h"

#include <stdarg.h>
#include <stdio.h>

/* log_buffer 是固定大小的环形缓冲区，用来保存最近的日志内容。 */
static char log_buffer[LOG_MAX_LINES][LOG_MAX_LINE_LENGTH];

/* log_start 指向逻辑上的第一条日志在数组中的位置。 */
static int log_start = 0;

/* log_count 表示当前实际保存了多少条有效日志。 */
static int log_count = 0;

void log_init(void) {
  /* 重置起始位置，让下一次读取从数组开头开始。 */
  log_start = 0;

  /* 重置有效条数，相当于逻辑上清空整个日志区。 */
  log_count = 0;
}

void log_print(const char* format, ...) {
  /* 新日志默认写到当前尾部位置。 */
  int idx = (log_start + log_count) % LOG_MAX_LINES;

  /* 可变参数列表用于支持 printf 风格格式化。 */
  va_list args;

  /* 初始化参数读取。 */
  va_start(args, format);

  /* 把格式化后的字符串写入当前槽位。 */
  vsnprintf(log_buffer[idx], LOG_MAX_LINE_LENGTH, format, args);

  /* 结束参数读取。 */
  va_end(args);

  /* 缓冲区还没满时，直接增加有效条数。 */
  if (log_count < LOG_MAX_LINES) {
    ++log_count;
    return;
  }

  /* 缓冲区写满后，最旧的一条会被新日志覆盖，所以起点向前移动一格。 */
  log_start = (log_start + 1) % LOG_MAX_LINES;
}

int log_get_line_count(void) {
  /* 直接返回当前保存的有效日志数量。 */
  return log_count;
}

const char* log_get_line(int index) {
  /* 访问越界时返回 NULL，让调用者自己决定如何处理。 */
  if (index < 0 || index >= log_count) {
    return NULL;
  }

  /* 逻辑顺序需要加上起始偏移，再对数组长度取模。 */
  return log_buffer[(log_start + index) % LOG_MAX_LINES];
}
