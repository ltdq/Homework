#ifndef DISPLAY_H
#define DISPLAY_H

// 菜单选项
typedef enum {
  MENU_INSERT = 0,
  MENU_GET,
  MENU_DELETE,
  MENU_MODIFY,
  MENU_SAVE,
  MENU_LOAD,
  MENU_NEW,
  MENU_UNDO,
  MENU_EXIT,
  MENU_COUNT
} MenuItem;

// UI 状态
typedef enum {
  STATE_MENU,
  STATE_INPUT_NAME,
  STATE_INPUT_ID,
  STATE_INPUT_FILENAME,
  STATE_EXIT
} UIState;

void display_init(void);
void display_cleanup(void);
void display_run(void);

// 内容输出（供 Data.c 调用，收集到弹窗）
void display_content_print(const char* format, ...);

// 显示/隐藏弹窗
void display_show_popup(int show);

// 清空弹窗内容
void display_clear_content(void);

#endif
