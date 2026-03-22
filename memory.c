#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

void memory_check(void* ptr) {
  if (!ptr) {
    printf("内存分配失败\n");
    exit(EXIT_FAILURE);
  }
}