#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>

// 菜单选项枚举
typedef enum {
  MENU_CREATE_FILE = 0,
  MENU_LOAD_FILE,
  MENU_SAVE_FILE,
  MENU_INSERT_DATA,
  MENU_QUERY_DATA,
  MENU_DELETE_DATA,
  MENU_MODIFY_DATA,
  MENU_COUNT
} MenuItem;

// 文件状态枚举
typedef enum {
  FILE_STATUS_NONE,
  FILE_STATUS_CLEAN,
  FILE_STATUS_MODIFIED
} FileStatus;

// 显示上下文（不透明指针）
typedef struct DisplayContext DisplayContext;

// 菜单回调函数类型
typedef void (*MenuCallback)(DisplayContext* ctx, MenuItem item, const char* input);

// 初始化显示系统，返回上下文
DisplayContext* display_init(void);

// 清理显示系统
void display_cleanup(DisplayContext* ctx);

// 主循环，处理用户输入
int display_run(DisplayContext* ctx);

// 设置状态栏信息
void display_set_filename(DisplayContext* ctx, const char* filename);
void display_set_file_status(DisplayContext* ctx, FileStatus status);
const char* display_get_filename(DisplayContext* ctx);

// 获取用户输入（用于回调）
const char* display_get_input(DisplayContext* ctx);

// 设置菜单回调函数
void display_set_callback(DisplayContext* ctx, MenuCallback callback);

// 请求刷新显示
void display_refresh(DisplayContext* ctx);

#endif
