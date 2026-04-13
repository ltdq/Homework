#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

#include "Log.h"

void memory_check(void* ptr) {
  /* ptr 非空说明内存申请成功，函数直接返回。 */
  if (ptr) {
    return;
  }

  /* 记录一条日志，方便界面层或测试代码看到失败原因。 */
  log_print("内存分配失败");

  /* 使用失败退出码立即终止程序，避免在空指针状态下继续运行。 */
  exit(EXIT_FAILURE);
}
