#include "Log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char log_buffer[LOG_MAX_LINES][LOG_MAX_LINE_LENGTH];
static int log_start = 0;
static int log_count = 0;

void log_init(void) {
  log_start = 0;
  log_count = 0;
}

void log_print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char temp[LOG_MAX_LINE_LENGTH];
  vsnprintf(temp, LOG_MAX_LINE_LENGTH, format, args);
  va_end(args);

  // 统一计算写入位置
  int idx = (log_start + log_count) % LOG_MAX_LINES;
  // 使用 snprintf 代替 strncpy 避免截断警告
  snprintf(log_buffer[idx], LOG_MAX_LINE_LENGTH, "%s", temp);

  // 更新计数或起始位置
  if (log_count < LOG_MAX_LINES) {
    ++log_count;
  } else {
    log_start = (log_start + 1) % LOG_MAX_LINES;
  }
}

int log_get_line_count(void) { return log_count; }

const char* log_get_line(int index) {
  if (index < 0 || index >= log_count) {
    return NULL;
  }
  return log_buffer[(log_start + index) % LOG_MAX_LINES];
}
