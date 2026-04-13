#include "Display.h"

#include <conio.h>
#include <locale.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "Data.h"
#include "Log.h"
#include "Trie.h"

#define CLEAR "\033[2J"
#define HOME "\033[H"
#define ALT_SCREEN "\033[?1049h"
#define MAIN_SCREEN "\033[?1049l"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define INVERT "\033[7m"
#define CYAN "\033[36m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"

#define MIN_TERM_WIDTH 72
#define MIN_TERM_HEIGHT 24
#define MENU_WIDTH 26
#define RESULT_LINE_CAP 8
#define RESULT_LINE_LENGTH 192
#define SEARCH_LIMIT 8

typedef enum MenuItem {
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

typedef enum UIState {
  UI_MENU = 0,
  UI_INPUT,
  UI_EXIT
} UIState;

typedef enum CurrentAction {
  ACTION_NONE = 0,
  ACTION_INSERT,
  ACTION_GET,
  ACTION_DELETE,
  ACTION_MODIFY,
  ACTION_SAVE,
  ACTION_LOAD,
  ACTION_NEW
} CurrentAction;

typedef enum InputKeyType {
  KEY_NONE = 0,
  KEY_ENTER,
  KEY_ESCAPE,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_HOME,
  KEY_END,
  KEY_BACKSPACE,
  KEY_CHAR
} InputKeyType;

typedef struct InputKey {
  InputKeyType type;
  unsigned char bytes[4];
  int byte_count;
} InputKey;

typedef struct DisplayContext {
  UIState state;
  int selected_menu;
  CurrentAction action;
  int step;

  char current_file[512];
  int file_loaded;

  char input_buf[512];
  int input_len;
  int input_cursor;

  char pending_name[512];
  char pending_id[512];

  char result_title[64];
  char result_lines[RESULT_LINE_CAP][RESULT_LINE_LENGTH];
  int result_line_count;

  const DataNode* search_results[SEARCH_LIMIT];
  int search_count;
  int search_total_count;
  int search_selected;

  int term_width;
  int term_height;
} DisplayContext;

static DisplayContext ctx = {
    .state = UI_MENU,
    .selected_menu = 0,
    .action = ACTION_NONE,
    .step = 0,
    .current_file = "未加载",
    .file_loaded = 0,
    .input_len = 0,
    .input_cursor = 0,
    .result_line_count = 0,
    .search_count = 0,
    .search_total_count = 0,
    .search_selected = -1,
    .term_width = 80,
    .term_height = 25,
};

static const char* menu_names[MENU_COUNT] = {
    "1. 添加数据", "2. 查询数据", "3. 删除数据", "4. 修改数据", "5. 保存文件",
    "6. 加载文件", "7. 新建文件", "8. 撤销操作", "9. 退出程序",
};

static const char* action_names[] = {
    "空闲", "添加数据", "查询数据", "删除数据", "修改数据", "保存文件", "加载文件", "新建文件",
};

static void move_cursor(int y, int x) { printf("\033[%d;%dH", y, x); }

static void print_repeat(const char* text, int count) {
  for (int i = 0; i < count; ++i) {
    printf("%s", text);
  }
}

static void update_term_size(void) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    ctx.term_width = info.srWindow.Right - info.srWindow.Left + 1;
    ctx.term_height = info.srWindow.Bottom - info.srWindow.Top + 1;
  }
}

static int utf8_char_len(const unsigned char* s) {
  if (*s < 0x80) return 1;
  if ((*s & 0xE0) == 0xC0) return 2;
  if ((*s & 0xF0) == 0xE0) return 3;
  if ((*s & 0xF8) == 0xF0) return 4;
  return 1;
}

static int utf8_char_width(const unsigned char* s) {
  if (*s < 0x80) return 1;
  if ((*s & 0xF0) == 0xE0 || (*s & 0xF8) == 0xF0) return 2;
  return 1;
}

static int prev_char_start(const char* text, int index) {
  int cursor = index;
  if (cursor <= 0) return 0;
  cursor--;
  while (cursor > 0 && ((unsigned char)text[cursor] & 0xC0) == 0x80) {
    cursor--;
  }
  return cursor;
}

static int next_char_start(const char* text, int index, int length) {
  if (index >= length) return length;
  return index + utf8_char_len((const unsigned char*)&text[index]);
}

static void print_clamped_text(const char* text, int width) {
  int used = 0;
  const unsigned char* p = (const unsigned char*)text;
  while (*p && used < width) {
    int char_len = utf8_char_len(p);
    int char_width = utf8_char_width(p);
    if (used + char_width > width) break;
    fwrite(p, 1, (size_t)char_len, stdout);
    p += char_len;
    used += char_width;
  }
  while (used < width) {
    putchar(' ');
    used++;
  }
}

static void draw_box(int x, int y, int width, int height, const char* title) {
  move_cursor(y, x);
  printf(DIM "┌");
  print_repeat("─", width - 2);
  printf("┐" RESET);
  for (int row = 1; row < height - 1; ++row) {
    move_cursor(y + row, x);
    printf(DIM "│" RESET);
    print_repeat(" ", width - 2);
    printf(DIM "│" RESET);
  }
  move_cursor(y + height - 1, x);
  printf(DIM "└");
  print_repeat("─", width - 2);
  printf("┘" RESET);
  if (title && title[0] != '\0') {
    move_cursor(y, x + 2);
    printf(BOLD);
    print_clamped_text(title, width - 4);
    printf(RESET);
  }
}

static void editor_reset(void) {
  ctx.input_len = 0;
  ctx.input_cursor = 0;
  ctx.input_buf[0] = '\0';
}

static void editor_set_text(const char* text) {
  snprintf(ctx.input_buf, sizeof(ctx.input_buf), "%s", text);
  ctx.input_len = (int)strlen(ctx.input_buf);
  ctx.input_cursor = ctx.input_len;
}

static void editor_move_left(void) {
  ctx.input_cursor = prev_char_start(ctx.input_buf, ctx.input_cursor);
}

static void editor_move_right(void) {
  ctx.input_cursor = next_char_start(ctx.input_buf, ctx.input_cursor, ctx.input_len);
}

static void editor_move_home(void) { ctx.input_cursor = 0; }

static void editor_move_end(void) { ctx.input_cursor = ctx.input_len; }

static void editor_backspace(void) {
  int start;
  if (ctx.input_cursor <= 0) return;
  start = prev_char_start(ctx.input_buf, ctx.input_cursor);
  memmove(ctx.input_buf + start, ctx.input_buf + ctx.input_cursor,
          (size_t)(ctx.input_len - ctx.input_cursor + 1));
  ctx.input_len -= ctx.input_cursor - start;
  ctx.input_cursor = start;
}

static void editor_insert_bytes(const unsigned char* bytes, int byte_count) {
  if (byte_count <= 0) return;
  if (ctx.input_len + byte_count >= (int)sizeof(ctx.input_buf)) return;
  memmove(ctx.input_buf + ctx.input_cursor + byte_count,
          ctx.input_buf + ctx.input_cursor,
          (size_t)(ctx.input_len - ctx.input_cursor + 1));
  memcpy(ctx.input_buf + ctx.input_cursor, bytes, (size_t)byte_count);
  ctx.input_cursor += byte_count;
  ctx.input_len += byte_count;
}

static InputKey read_input_key(void) {
  InputKey key = {.type = KEY_NONE, .byte_count = 0};
  int ch = _getch();

  if (ch == 0 || ch == 224) {
    int code = _getch();
    switch (code) {
      case 72:
        key.type = KEY_UP;
        break;
      case 80:
        key.type = KEY_DOWN;
        break;
      case 75:
        key.type = KEY_LEFT;
        break;
      case 77:
        key.type = KEY_RIGHT;
        break;
      case 71:
        key.type = KEY_HOME;
        break;
      case 79:
        key.type = KEY_END;
        break;
      default:
        key.type = KEY_NONE;
        break;
    }
    return key;
  }

  if (ch == 27) {
    int wait_count = 0;
    while (!_kbhit() && wait_count < 10) {
      Sleep(1);
      wait_count++;
    }
    if (!_kbhit()) {
      key.type = KEY_ESCAPE;
      return key;
    }

    ch = _getch();
    if (ch == '[' || ch == 'O') {
      int code = _getch();
      switch (code) {
        case 'A':
          key.type = KEY_UP;
          return key;
        case 'B':
          key.type = KEY_DOWN;
          return key;
        case 'C':
          key.type = KEY_RIGHT;
          return key;
        case 'D':
          key.type = KEY_LEFT;
          return key;
        case 'H':
          key.type = KEY_HOME;
          return key;
        case 'F':
          key.type = KEY_END;
          return key;
      }
    }

    key.type = KEY_ESCAPE;
    return key;
  }

  if (ch == '\r' || ch == '\n') {
    key.type = KEY_ENTER;
    return key;
  }
  if (ch == 8 || ch == 127) {
    key.type = KEY_BACKSPACE;
    return key;
  }
  if (ch >= 32) {
    int byte_count = 1;
    key.type = KEY_CHAR;
    key.bytes[0] = (unsigned char)ch;
    if ((ch & 0xE0) == 0xC0) {
      byte_count = 2;
    } else if ((ch & 0xF0) == 0xE0) {
      byte_count = 3;
    } else if ((ch & 0xF8) == 0xF0) {
      byte_count = 4;
    }
    for (int i = 1; i < byte_count; ++i) {
      int next = _getch();
      if (next == EOF || (next & 0xC0) != 0x80) {
        key.byte_count = i;
        return key;
      }
      key.bytes[i] = (unsigned char)next;
    }
    key.byte_count = byte_count;
  }
  return key;
}

static void result_clear(void) {
  ctx.result_title[0] = '\0';
  ctx.result_line_count = 0;
}

static void result_set_title(const char* title) {
  snprintf(ctx.result_title, sizeof(ctx.result_title), "%s", title);
}

static void result_add_line(const char* format, ...) {
  va_list args;
  if (ctx.result_line_count >= RESULT_LINE_CAP) return;
  va_start(args, format);
  vsnprintf(ctx.result_lines[ctx.result_line_count],
            sizeof(ctx.result_lines[ctx.result_line_count]), format, args);
  va_end(args);
  ctx.result_line_count++;
}

static void result_set_message(const char* title, const char* format, ...) {
  va_list args;
  result_clear();
  result_set_title(title);
  va_start(args, format);
  vsnprintf(ctx.result_lines[0], sizeof(ctx.result_lines[0]), format, args);
  va_end(args);
  ctx.result_line_count = 1;
}

static void result_show_record(const char* title, const DataNode* node) {
  result_clear();
  result_set_title(title);
  result_add_line("姓名: %s", node->name);
  result_add_line("ID: %s", node->id);
  result_add_line("值: %s", node->value);
}

static void clear_pending(void) {
  ctx.pending_name[0] = '\0';
  ctx.pending_id[0] = '\0';
}

static void clear_search(void) {
  ctx.search_count = 0;
  ctx.search_total_count = 0;
  ctx.search_selected = -1;
}

static void enter_menu(void) {
  ctx.state = UI_MENU;
  ctx.action = ACTION_NONE;
  ctx.step = 0;
  editor_reset();
  clear_pending();
  clear_search();
}

static int is_query_step(void) {
  if (ctx.action == ACTION_GET) return 1;
  if (ctx.action == ACTION_DELETE) return 1;
  if (ctx.action == ACTION_MODIFY && ctx.step == 0) return 1;
  return 0;
}

static CurrentAction action_from_menu(int menu) {
  switch (menu) {
    case MENU_INSERT:
      return ACTION_INSERT;
    case MENU_GET:
      return ACTION_GET;
    case MENU_DELETE:
      return ACTION_DELETE;
    case MENU_MODIFY:
      return ACTION_MODIFY;
    case MENU_SAVE:
      return ACTION_SAVE;
    case MENU_LOAD:
      return ACTION_LOAD;
    case MENU_NEW:
      return ACTION_NEW;
    default:
      return ACTION_NONE;
  }
}

static void update_input_summary(void) {
  result_clear();
  result_set_title(action_names[ctx.action]);
  switch (ctx.action) {
    case ACTION_INSERT:
      if (ctx.pending_name[0] != '\0') result_add_line("姓名: %s", ctx.pending_name);
      if (ctx.pending_id[0] != '\0') result_add_line("ID: %s", ctx.pending_id);
      result_add_line("步骤 %d/3", ctx.step + 1);
      break;
    case ACTION_SAVE:
    case ACTION_LOAD:
    case ACTION_NEW:
      result_add_line("请输入文件名后按 Enter。");
      break;
    case ACTION_MODIFY:
      if (ctx.step == 1) {
        result_add_line("输入新值后按 Enter。");
      } else {
        result_add_line("输入 ID 或姓名前缀。");
      }
      break;
    case ACTION_GET:
    case ACTION_DELETE:
      result_add_line("输入 ID 或姓名前缀。");
      break;
    default:
      result_add_line("使用上下键选择菜单项。");
      break;
  }
}

static void set_file_loaded(const char* filename) {
  snprintf(ctx.current_file, sizeof(ctx.current_file), "%s", filename);
  ctx.file_loaded = 1;
}

static void refresh_query_results(void) {
  const DataNode* exact_match;
  DataNode** matches;

  clear_search();
  result_clear();
  result_set_title(action_names[ctx.action]);

  if (ctx.input_len == 0) {
    result_add_line("输入 ID 或姓名前缀。");
    return;
  }

  ctx.input_buf[ctx.input_len] = '\0';
  exact_match = data_find_by_id(ctx.input_buf);
  if (exact_match) {
    ctx.search_results[0] = exact_match;
    ctx.search_count = 1;
    ctx.search_total_count = 1;
    ctx.search_selected = 0;
    result_show_record("精确匹配", exact_match);
    return;
  }

  matches = trie_search(ctx.input_buf, &ctx.search_total_count);
  if (ctx.search_total_count == 0) {
    result_add_line("没有匹配记录。");
    free(matches);
    return;
  }

  ctx.search_count = ctx.search_total_count < SEARCH_LIMIT ? ctx.search_total_count : SEARCH_LIMIT;
  for (int i = 0; i < ctx.search_count; ++i) {
    ctx.search_results[i] = matches[i];
  }
  ctx.search_selected = 0;
  free(matches);

  if (ctx.search_total_count == 1) {
    result_show_record("唯一匹配", ctx.search_results[0]);
    return;
  }

  result_set_title("候选记录");
  result_add_line("共 %d 条匹配，使用上下键选择。", ctx.search_total_count);
  if (ctx.search_total_count > SEARCH_LIMIT) {
    result_add_line("当前显示前 %d 条。", SEARCH_LIMIT);
  }
}

static void start_action(CurrentAction action) {
  ctx.state = UI_INPUT;
  ctx.action = action;
  ctx.step = 0;
  editor_reset();
  clear_pending();
  clear_search();
  update_input_summary();
}

static void log_status_for_insert(DataStatus status, const char* name, const char* id) {
  switch (status) {
    case DATA_OK:
      log_print("已添加 %s (%s)", name, id);
      break;
    case DATA_DUPLICATE_ID:
      log_print("ID '%s' 已存在", id);
      break;
    default:
      log_print("添加失败，请检查输入。");
      break;
  }
}

static void log_status_for_delete(DataStatus status, const char* name, const char* id) {
  switch (status) {
    case DATA_OK:
      log_print("已删除 %s (%s)", name, id);
      break;
    case DATA_NOT_FOUND:
      log_print("没有找到 ID '%s'", id);
      break;
    default:
      log_print("删除失败，请检查输入。");
      break;
  }
}

static void log_status_for_modify(DataStatus status, const char* id) {
  switch (status) {
    case DATA_OK:
      log_print("已更新 ID '%s'", id);
      break;
    case DATA_NOT_FOUND:
      log_print("没有找到 ID '%s'", id);
      break;
    default:
      log_print("修改失败，请检查输入。");
      break;
  }
}

static void log_status_for_file(CurrentAction action, DataStatus status, const char* filename) {
  if (action == ACTION_SAVE) {
    switch (status) {
      case DATA_OK:
        log_print("已保存到 %s", filename);
        break;
      case DATA_IO_ERROR:
        log_print("保存文件失败: %s", filename);
        break;
      default:
        log_print("保存失败，请检查文件名。");
        break;
    }
    return;
  }
  if (action == ACTION_LOAD) {
    switch (status) {
      case DATA_OK:
        log_print("已加载 %s", filename);
        break;
      case DATA_DIRTY_BLOCKED:
        log_print("当前数据已修改，先保存再加载。");
        break;
      case DATA_FORMAT_ERROR:
        log_print("文件格式错误: %s", filename);
        break;
      case DATA_IO_ERROR:
        log_print("加载文件失败: %s", filename);
        break;
      default:
        log_print("加载失败，请检查文件名。");
        break;
    }
    return;
  }
  switch (status) {
    case DATA_OK:
      log_print("已新建空文件 %s", filename);
      break;
    case DATA_DIRTY_BLOCKED:
      log_print("当前数据已修改，先保存再新建。");
      break;
    case DATA_IO_ERROR:
      log_print("新建文件失败: %s", filename);
      break;
    default:
      log_print("新建失败，请检查文件名。");
      break;
  }
}

static void submit_query_match(const DataNode* node) {
  if (ctx.action == ACTION_GET) {
    result_show_record("查询结果", node);
    log_print("已显示 %s (%s)", node->name, node->id);
    enter_menu();
    return;
  }

  if (ctx.action == ACTION_DELETE) {
    char name_copy[256];
    char id_copy[256];
    DataStatus status;

    snprintf(name_copy, sizeof(name_copy), "%s", node->name);
    snprintf(id_copy, sizeof(id_copy), "%s", node->id);
    status = data_delete(node->id, 1);
    log_status_for_delete(status, name_copy, id_copy);
    if (status == DATA_OK) {
      result_set_message("删除数据", "已删除 %s (%s)", name_copy, id_copy);
      enter_menu();
    }
    return;
  }

  snprintf(ctx.pending_id, sizeof(ctx.pending_id), "%s", node->id);
  ctx.step = 1;
  editor_reset();
  clear_search();
  result_show_record("待修改记录", node);
  result_add_line("输入新值后按 Enter。");
}

static void submit_query_step(void) {
  const DataNode* exact_match;

  if (ctx.input_len == 0) {
    log_print("请输入 ID 或姓名前缀。");
    return;
  }

  ctx.input_buf[ctx.input_len] = '\0';
  exact_match = data_find_by_id(ctx.input_buf);
  if (exact_match) {
    submit_query_match(exact_match);
    return;
  }

  refresh_query_results();
  if (ctx.search_total_count == 0) {
    log_print("没有找到匹配记录。");
    return;
  }

  submit_query_match(ctx.search_results[ctx.search_selected]);
}

static void submit_insert_step(void) {
  DataStatus status;
  const DataNode* node;

  if (ctx.input_len == 0) {
    log_print("请输入完整内容。");
    return;
  }

  ctx.input_buf[ctx.input_len] = '\0';
  if (ctx.step == 0) {
    snprintf(ctx.pending_name, sizeof(ctx.pending_name), "%s", ctx.input_buf);
    ctx.step = 1;
    editor_reset();
    update_input_summary();
    return;
  }
  if (ctx.step == 1) {
    snprintf(ctx.pending_id, sizeof(ctx.pending_id), "%s", ctx.input_buf);
    ctx.step = 2;
    editor_reset();
    update_input_summary();
    return;
  }

  status = data_insert(ctx.pending_name, ctx.pending_id, ctx.input_buf, 1);
  log_status_for_insert(status, ctx.pending_name, ctx.pending_id);
  if (status == DATA_OK) {
    node = data_find_by_id(ctx.pending_id);
    if (node) {
      result_show_record("添加完成", node);
    } else {
      result_set_message("添加完成", "已添加 %s (%s)", ctx.pending_name, ctx.pending_id);
    }
    enter_menu();
    return;
  }

  if (status == DATA_DUPLICATE_ID) {
    ctx.step = 1;
    editor_set_text(ctx.pending_id);
  } else {
    ctx.step = 0;
    editor_set_text(ctx.pending_name);
  }
  update_input_summary();
}

static void submit_modify_value(void) {
  DataStatus status;
  const DataNode* node;

  if (ctx.input_len == 0) {
    log_print("请输入新值。");
    return;
  }

  ctx.input_buf[ctx.input_len] = '\0';
  status = data_modify(ctx.pending_id, ctx.input_buf, 1);
  log_status_for_modify(status, ctx.pending_id);
  if (status == DATA_OK) {
    node = data_find_by_id(ctx.pending_id);
    if (node) {
      result_show_record("修改完成", node);
    } else {
      result_set_message("修改完成", "已更新 ID '%s'", ctx.pending_id);
    }
    enter_menu();
  }
}

static void submit_file_action(void) {
  DataStatus status;

  if (ctx.input_len == 0) {
    log_print("请输入文件名。");
    return;
  }

  ctx.input_buf[ctx.input_len] = '\0';
  if (ctx.action == ACTION_SAVE) {
    status = data_save(ctx.input_buf);
  } else if (ctx.action == ACTION_LOAD) {
    status = data_load(ctx.input_buf);
  } else {
    status = data_new(ctx.input_buf);
  }

  log_status_for_file(ctx.action, status, ctx.input_buf);
  if (status == DATA_OK) {
    set_file_loaded(ctx.input_buf);
    if (ctx.action == ACTION_SAVE) {
      result_set_message("保存文件", "已保存到 %s", ctx.input_buf);
    } else if (ctx.action == ACTION_LOAD) {
      result_set_message("加载文件", "已加载 %s", ctx.input_buf);
    } else {
      result_set_message("新建文件", "已创建并切换到 %s", ctx.input_buf);
    }
    enter_menu();
    return;
  }

  update_input_summary();
}

static void submit_input(void) {
  if (ctx.action == ACTION_INSERT) {
    submit_insert_step();
    return;
  }
  if (is_query_step()) {
    submit_query_step();
    return;
  }
  if (ctx.action == ACTION_MODIFY) {
    submit_modify_value();
    return;
  }
  submit_file_action();
}

static void run_selected_menu_item(void) {
  if (ctx.selected_menu == MENU_UNDO) {
    DataStatus status = data_undo();
    if (status == DATA_OK) {
      result_set_message("撤销操作", "已撤销上一步操作。");
      log_print("已撤销上一步操作。");
    } else {
      result_set_message("撤销操作", "当前没有可撤销的操作。");
      log_print("没有可撤销的操作。");
    }
    return;
  }
  if (ctx.selected_menu == MENU_EXIT) {
    ctx.state = UI_EXIT;
    return;
  }
  start_action(action_from_menu(ctx.selected_menu));
}

static void rotate_menu_selection(int delta) {
  ctx.selected_menu = (ctx.selected_menu + delta + MENU_COUNT) % MENU_COUNT;
}

static void handle_menu_key(InputKey key) {
  switch (key.type) {
    case KEY_UP:
    case KEY_LEFT:
      rotate_menu_selection(-1);
      break;
    case KEY_DOWN:
    case KEY_RIGHT:
      rotate_menu_selection(1);
      break;
    case KEY_ENTER:
      run_selected_menu_item();
      break;
    case KEY_CHAR:
      if (key.bytes[0] >= '1' && key.bytes[0] <= '9') {
        ctx.selected_menu = key.bytes[0] - '1';
        run_selected_menu_item();
      }
      break;
    default:
      break;
  }
}

static void handle_input_key(InputKey key) {
  switch (key.type) {
    case KEY_ESCAPE:
      enter_menu();
      log_print("操作已取消。");
      break;
    case KEY_ENTER:
      submit_input();
      break;
    case KEY_UP:
      if (is_query_step() && ctx.search_total_count > 1 && ctx.search_selected > 0) {
        ctx.search_selected--;
      }
      break;
    case KEY_DOWN:
      if (is_query_step() && ctx.search_total_count > 1 &&
          ctx.search_selected < ctx.search_count - 1) {
        ctx.search_selected++;
      }
      break;
    case KEY_LEFT:
      editor_move_left();
      break;
    case KEY_RIGHT:
      editor_move_right();
      break;
    case KEY_HOME:
      editor_move_home();
      break;
    case KEY_END:
      editor_move_end();
      break;
    case KEY_BACKSPACE:
      editor_backspace();
      break;
    case KEY_CHAR:
      editor_insert_bytes(key.bytes, key.byte_count);
      break;
    default:
      break;
  }

  if (is_query_step() &&
      (key.type == KEY_LEFT || key.type == KEY_RIGHT || key.type == KEY_HOME ||
       key.type == KEY_END || key.type == KEY_BACKSPACE || key.type == KEY_CHAR)) {
    refresh_query_results();
  }
}

static const char* current_prompt(void) {
  switch (ctx.action) {
    case ACTION_INSERT:
      if (ctx.step == 0) return "姓名";
      if (ctx.step == 1) return "ID";
      return "值";
    case ACTION_GET:
      return "ID/姓名前缀";
    case ACTION_DELETE:
      return "ID/姓名前缀";
    case ACTION_MODIFY:
      if (ctx.step == 0) return "ID/姓名前缀";
      return "新值";
    default:
      return "文件名";
  }
}

static void render_header(void) {
  const char* save_state = data_is_modified() ? "已修改" : "已保存";
  move_cursor(1, 2);
  printf(BOLD CYAN "学生信息管理系统" RESET);
  move_cursor(2, 2);
  printf("文件: %s  状态: %s", ctx.current_file,
         ctx.file_loaded ? save_state : "未加载");
}

static void render_menu_box(int x, int y, int width, int height) {
  draw_box(x, y, width, height, "菜单");
  for (int i = 0; i < MENU_COUNT && i < height - 2; ++i) {
    move_cursor(y + 1 + i, x + 2);
    if (i == ctx.selected_menu) {
      printf(INVERT);
      print_clamped_text(menu_names[i], width - 4);
      printf(RESET);
    } else {
      print_clamped_text(menu_names[i], width - 4);
    }
  }
}

static void render_search_list(int x, int y, int width, int height) {
  int line = 0;
  for (int i = 0; i < ctx.result_line_count && line < height; ++i, ++line) {
    move_cursor(y + line, x);
    print_clamped_text(ctx.result_lines[i], width);
  }
  for (int i = 0; i < ctx.search_count && line < height; ++i, ++line) {
    char row[RESULT_LINE_LENGTH];
    snprintf(row, sizeof(row), "%s %s (%s)", i == ctx.search_selected ? ">" : " ",
             ctx.search_results[i]->name, ctx.search_results[i]->id);
    move_cursor(y + line, x);
    if (i == ctx.search_selected) {
      printf(INVERT);
      print_clamped_text(row, width);
      printf(RESET);
    } else {
      print_clamped_text(row, width);
    }
  }
}

static void render_result_box(int x, int y, int width, int height) {
  draw_box(x, y, width, height, ctx.result_title[0] != '\0' ? ctx.result_title : "结果");
  if (ctx.search_total_count > 1 && is_query_step()) {
    render_search_list(x + 2, y + 1, width - 4, height - 2);
    return;
  }
  for (int i = 0; i < ctx.result_line_count && i < height - 2; ++i) {
    move_cursor(y + 1 + i, x + 2);
    print_clamped_text(ctx.result_lines[i], width - 4);
  }
}

static void render_input_value(int x, int y, int width) {
  int field_width = width;
  int start = ctx.input_cursor;
  int used = 0;
  int prefix = 0;
  int i;

  while (start > 0) {
    int prev = prev_char_start(ctx.input_buf, start);
    int char_width = utf8_char_width((const unsigned char*)&ctx.input_buf[prev]);
    if (used + char_width >= field_width) break;
    used += char_width;
    start = prev;
  }

  if (start > 0 && field_width > 1) {
    prefix = 1;
    move_cursor(y, x);
    putchar('<');
    field_width--;
    x++;
  }

  used = 0;
  i = start;
  while (used < field_width && i < ctx.input_len) {
    int next = next_char_start(ctx.input_buf, i, ctx.input_len);
    int char_width = utf8_char_width((const unsigned char*)&ctx.input_buf[i]);
    int char_len = next - i;
    if (used + char_width > field_width) break;

    move_cursor(y, x + used);
    if (i == ctx.input_cursor) printf(INVERT);
    fwrite(ctx.input_buf + i, 1, (size_t)char_len, stdout);
    if (i == ctx.input_cursor) printf(RESET);
    used += char_width;
    i = next;
  }

  if (ctx.input_cursor == ctx.input_len && used < field_width) {
    move_cursor(y, x + used);
    printf(INVERT " " RESET);
    used++;
  }

  move_cursor(y, x + used);
  while (used < field_width) {
    putchar(' ');
    used++;
  }

  if (prefix) {
    move_cursor(y, x - 1);
    putchar('<');
  }
}

static void render_input_box(int x, int y, int width, int height) {
  char title[64];
  char prompt_line[RESULT_LINE_LENGTH];

  if (ctx.state == UI_MENU) {
    draw_box(x, y, width, height, "输入");
    move_cursor(y + 1, x + 2);
    print_clamped_text("按 Enter 开始当前菜单项，数字键可直接执行。", width - 4);
    move_cursor(y + 2, x + 2);
    print_clamped_text("方向键移动，Esc 保留给输入态取消操作。", width - 4);
    return;
  }

  snprintf(title, sizeof(title), "%s", action_names[ctx.action]);
  draw_box(x, y, width, height, title);
  snprintf(prompt_line, sizeof(prompt_line), "提示: %s", current_prompt());
  move_cursor(y + 1, x + 2);
  print_clamped_text(prompt_line, width - 4);
  move_cursor(y + 2, x + 2);
  render_input_value(x + 2, y + 2, width - 4);
}

static void render_log_box(int x, int y, int width, int height) {
  int line_count = log_get_line_count();
  int visible_lines = height - 2;
  int start = line_count > visible_lines ? line_count - visible_lines : 0;

  draw_box(x, y, width, height, "日志");
  for (int i = 0; i < visible_lines; ++i) {
    const char* line = log_get_line(start + i);
    move_cursor(y + 1 + i, x + 2);
    print_clamped_text(line ? line : "", width - 4);
  }
}

static void render_footer(void) {
  move_cursor(ctx.term_height, 2);
  printf(DIM "Enter 确认  Esc 取消输入  上下选择  左右移动光标  Home/End 跳转" RESET);
}

static void render_small_terminal_message(void) {
  printf(CLEAR HOME YELLOW "终端至少需要 %dx%d，当前是 %dx%d。"
         RESET,
         MIN_TERM_WIDTH, MIN_TERM_HEIGHT, ctx.term_width, ctx.term_height);
}

static void render_screen(void) {
  int menu_x = 2;
  int menu_y = 3;
  int menu_height = 11;
  int menu_width = MENU_WIDTH;
  int result_x = menu_x + menu_width + 1;
  int result_y = menu_y;
  int result_width = ctx.term_width - result_x - 1;
  int result_height = menu_height;
  int input_x = 2;
  int input_y = menu_y + menu_height;
  int input_width = ctx.term_width - 2;
  int input_height = 4;
  int log_x = 2;
  int log_y = input_y + input_height;
  int log_width = ctx.term_width - 2;
  int log_height = ctx.term_height - log_y;

  update_term_size();
  printf(CLEAR HOME);
  if (ctx.term_width < MIN_TERM_WIDTH || ctx.term_height < MIN_TERM_HEIGHT) {
    render_small_terminal_message();
    fflush(stdout);
    return;
  }

  render_header();
  render_menu_box(menu_x, menu_y, menu_width, menu_height);
  render_result_box(result_x, result_y, result_width, result_height);
  render_input_box(input_x, input_y, input_width, input_height);
  render_log_box(log_x, log_y, log_width, log_height);
  render_footer();
  fflush(stdout);
}

static void wait_for_min_size(void) {
  while (1) {
    update_term_size();
    if (ctx.term_width >= MIN_TERM_WIDTH && ctx.term_height >= MIN_TERM_HEIGHT) {
      return;
    }
    render_small_terminal_message();
    fflush(stdout);
    Sleep(300);
  }
}

void display_init(void) {
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;

  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  setlocale(LC_ALL, ".UTF8");

  GetConsoleMode(handle, &mode);
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(handle, mode);

  printf(ALT_SCREEN HOME HIDE_CURSOR);

  log_init();
  data_init();
  result_set_message("欢迎", "使用上下键或数字键选择操作。");
  log_print("系统就绪。");
}

void display_cleanup(void) {
  printf(SHOW_CURSOR MAIN_SCREEN);
  fflush(stdout);
}

void display_run(void) {
  wait_for_min_size();
  while (ctx.state != UI_EXIT) {
    InputKey key;
    render_screen();
    key = read_input_key();
    if (ctx.state == UI_MENU) {
      handle_menu_key(key);
    } else {
      handle_input_key(key);
    }
  }
}
