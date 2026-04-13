#include "Log.h"

#include <stdarg.h>
#include <stdio.h>

static char log_buffer[LOG_MAX_LINES][LOG_MAX_LINE_LENGTH];
static int log_start = 0;
static int log_count = 0;

void log_init(void) {
  log_start = 0;
  log_count = 0;
}

void log_print(const char* format, ...) {
  int idx = (log_start + log_count) % LOG_MAX_LINES;
  va_list args;
  va_start(args, format);
  vsnprintf(log_buffer[idx], LOG_MAX_LINE_LENGTH, format, args);
  va_end(args);

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
