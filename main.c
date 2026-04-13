#include "Data.h"
#include "Display.h"

int main(void) {
  /* 初始化显示层，同时也会顺带初始化日志系统和数据层。 */
  display_init();

  /* 进入主循环，直到用户在菜单中选择退出。 */
  display_run();

  /* 恢复终端状态，例如主屏幕和光标显示。 */
  display_cleanup();

  /* 按标准约定返回 0，表示程序正常结束。 */
  return 0;
}
