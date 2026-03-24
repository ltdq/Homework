#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

#include "Log.h"

void memory_check(void* ptr) {
  if (!ptr) {
    log_print("内存分配失败");
    exit(EXIT_FAILURE);
  }
}