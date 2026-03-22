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

  if (log_count < LOG_MAX_LINES) {
    strncpy(log_buffer[(log_start + log_count) % LOG_MAX_LINES], temp,
            LOG_MAX_LINE_LENGTH - 1);
    log_buffer[(log_start + log_count) % LOG_MAX_LINES][LOG_MAX_LINE_LENGTH - 1] =
        '\0';
    log_count++;
  } else {
    strncpy(log_buffer[log_start], temp, LOG_MAX_LINE_LENGTH - 1);
    log_buffer[log_start][LOG_MAX_LINE_LENGTH - 1] = '\0';
    log_start = (log_start + 1) % LOG_MAX_LINES;
  }
}

int log_get_line_count(void) { return log_count; }

const char* log_get_line(int index) {
  if (index < 0 || index >= log_count) {
    return "";
  }
  return log_buffer[(log_start + index) % LOG_MAX_LINES];
}
