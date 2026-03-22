#include <stdio.h>
#include <string.h>

#include "Data.h"
#include "Display.h"

int file_modified = 0;
static DisplayContext* g_display = NULL;

// 全局状态更新函数
static void update_display_state(DisplayContext* ctx, const char* filename) {
  if (ctx) {
    display_set_filename(ctx, filename);
    display_set_file_status(ctx,
                            file_modified ? FILE_STATUS_MODIFIED : FILE_STATUS_CLEAN);
  }
}

// 菜单回调函数
static void on_menu_select(DisplayContext* ctx, MenuItem item, const char* input) {
  switch (item) {
    case MENU_CREATE_FILE:
      if (strlen(input) > 0) {
        printf("创建文件: %s\n", input);
        data_new(input);
        file_modified = 0;
        update_display_state(ctx, input);
      } else {
        printf("请输入文件名\n");
      }
      break;

    case MENU_LOAD_FILE:
      if (strlen(input) > 0) {
        printf("加载文件: %s\n", input);
        data_load(input);
        file_modified = 0;
        update_display_state(ctx, input);
      } else {
        printf("请输入文件名\n");
      }
      break;

    case MENU_SAVE_FILE: {
      const char* filename = display_get_filename(ctx);
      if (strlen(filename) > 0) {
        printf("保存文件: %s\n", filename);
        data_save(filename);
        file_modified = 0;
        update_display_state(ctx, filename);
      } else if (strlen(input) > 0) {
        printf("保存文件: %s\n", input);
        data_save(input);
        file_modified = 0;
        update_display_state(ctx, input);
      } else {
        printf("请输入文件名\n");
      }
      break;
    }

    case MENU_INSERT_DATA: {
      // 解析格式: name,id,value
      char buf[1024];
      strncpy(buf, input, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      char* name = strtok(buf, ",");
      char* id = strtok(NULL, ",");
      char* value = strtok(NULL, "");
      if (name && id && value) {
        // 去除前导空格
        while (*id == ' ') id++;
        data_insert(name, id, value, 1);
        file_modified = 1;
        update_display_state(ctx, display_get_filename(ctx));
      } else {
        printf("格式错误，请使用: 名字,ID,值\n");
        printf("例如: 张三,20240001,计算机科学与技术\n");
      }
      break;
    }

    case MENU_QUERY_DATA:
      if (strlen(input) > 0) {
        printf("查询: %s\n", input);
        data_get(input);
      } else {
        printf("请输入要查询的名字或ID\n");
      }
      break;

    case MENU_DELETE_DATA:
      if (strlen(input) > 0) {
        printf("删除: %s\n", input);
        data_delete(input, 1);
        file_modified = 1;
        update_display_state(ctx, display_get_filename(ctx));
      } else {
        printf("请输入要删除的ID\n");
      }
      break;

    case MENU_MODIFY_DATA: {
      // 解析格式: key,new_value
      char buf[1024];
      strncpy(buf, input, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      char* key = strtok(buf, ",");
      char* new_value = strtok(NULL, "");
      if (key && new_value) {
        // 去除前导空格
        while (*new_value == ' ') new_value++;
        printf("修改: %s -> %s\n", key, new_value);
        data_modify(key, new_value, 1);
        file_modified = 1;
        update_display_state(ctx, display_get_filename(ctx));
      } else {
        printf("格式错误，请使用: 键,新值\n");
        printf("例如: 20240001,软件工程\n");
      }
      break;
    }

    default:
      break;
  }
}

int main(void) {
  // 初始化数据系统
  data_init();

  // 初始化显示系统
  g_display = display_init();
  if (!g_display) {
    fprintf(stderr, "无法初始化显示系统\n");
    return 1;
  }

  // 设置菜单回调
  display_set_callback(g_display, on_menu_select);

  // 设置初始状态
  display_set_filename(g_display, "");
  display_set_file_status(g_display, FILE_STATUS_NONE);

  // 运行主循环
  display_run(g_display);

  // 清理
  display_cleanup(g_display);
  data_init();  // 清理数据

  return 0;
}
