#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

void memory_check(void* ptr) {
  if (!ptr) {
    printf("Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
}