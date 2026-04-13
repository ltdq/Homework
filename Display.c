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

/* 清屏。 */
#define CLEAR "\033[2J"
/* 把光标移动到左上角。 */
#define HOME "\033[H"
/* 切换到备用屏幕。 */
#define ALT_SCREEN "\033[?1049h"
/* 返回主屏幕。 */
#define MAIN_SCREEN "\033[?1049l"
/* 隐藏光标。 */
#define HIDE_CURSOR "\033[?25l"
/* 显示光标。 */
#define SHOW_CURSOR "\033[?25h"

/* 重置终端样式。 */
#define RESET "\033[0m"
/* 粗体。 */
#define BOLD "\033[1m"
/* 暗色。 */
#define DIM "\033[2m"
/* 反色，用于高亮。 */
#define INVERT "\033[7m"
/* 青色。 */
#define CYAN "\033[36m"
/* 绿色。 */
#define GREEN "\033[32m"
/* 黄色。 */
#define YELLOW "\033[33m"

/* 终端允许正常显示界面的最小宽度。 */
#define MIN_TERM_WIDTH 72
/* 终端允许正常显示界面的最小高度。 */
#define MIN_TERM_HEIGHT 24
/* 左侧菜单框固定宽度。 */
#define MENU_WIDTH 26
/* 结果区最多缓存的文本行数。 */
#define RESULT_LINE_CAP 8
/* 单条结果文本的最大长度。 */
#define RESULT_LINE_LENGTH 192
/* 查询候选区最多展示的记录条数。 */
#define SEARCH_LIMIT 8

/* 主菜单枚举，顺序与 menu_names 数组完全对应。 */
typedef enum MenuItem {
  /* 添加数据。 */
  MENU_INSERT = 0,
  /* 查询数据。 */
  MENU_GET,
  /* 删除数据。 */
  MENU_DELETE,
  /* 修改数据。 */
  MENU_MODIFY,
  /* 保存文件。 */
  MENU_SAVE,
  /* 加载文件。 */
  MENU_LOAD,
  /* 新建文件。 */
  MENU_NEW,
  /* 撤销。 */
  MENU_UNDO,
  /* 退出。 */
  MENU_EXIT,
  /* 菜单总数。 */
  MENU_COUNT
} MenuItem;

/* UIState 描述界面所在的大状态。 */
typedef enum UIState {
  /* 菜单态。 */
  UI_MENU = 0,
  /* 输入态。 */
  UI_INPUT,
  /* 退出态。 */
  UI_EXIT
} UIState;

/* CurrentAction 描述当前输入态对应的具体业务动作。 */
typedef enum CurrentAction {
  /* 没有正在执行的动作。 */
  ACTION_NONE = 0,
  /* 添加。 */
  ACTION_INSERT,
  /* 查询。 */
  ACTION_GET,
  /* 删除。 */
  ACTION_DELETE,
  /* 修改。 */
  ACTION_MODIFY,
  /* 保存。 */
  ACTION_SAVE,
  /* 加载。 */
  ACTION_LOAD,
  /* 新建。 */
  ACTION_NEW
} CurrentAction;

/* InputKeyType 把底层键码统一映射成内部语义。 */
typedef enum InputKeyType {
  /* 无有效按键。 */
  KEY_NONE = 0,
  /* Enter。 */
  KEY_ENTER,
  /* Esc。 */
  KEY_ESCAPE,
  /* 上方向。 */
  KEY_UP,
  /* 下方向。 */
  KEY_DOWN,
  /* 左方向。 */
  KEY_LEFT,
  /* 右方向。 */
  KEY_RIGHT,
  /* Home。 */
  KEY_HOME,
  /* End。 */
  KEY_END,
  /* Backspace。 */
  KEY_BACKSPACE,
  /* 普通字符。 */
  KEY_CHAR
} InputKeyType;

/* InputKey 保存一次读取得到的按键信息。 */
typedef struct InputKey {
  /* 按键的语义类型。 */
  InputKeyType type;
  /* UTF-8 字节内容，最多 4 字节。 */
  unsigned char bytes[4];
  /* bytes 中实际有效的字节数。 */
  int byte_count;
} InputKey;

/* DisplayContext 集中保存整个界面的运行时状态。 */
typedef struct DisplayContext {
  /* 当前 UI 所处的大状态。 */
  UIState state;
  /* 当前高亮的菜单项。 */
  int selected_menu;
  /* 当前动作。 */
  CurrentAction action;
  /* 当前动作所处的步骤号。 */
  int step;

  /* 当前工作文件名。 */
  char current_file[512];
  /* 是否已经有可用文件上下文。 */
  int file_loaded;

  /* 输入框缓冲区。 */
  char input_buf[512];
  /* 输入框当前总字节数。 */
  int input_len;
  /* 光标在输入缓冲区中的字节位置。 */
  int input_cursor;

  /* 添加流程里暂存的姓名。 */
  char pending_name[512];
  /* 添加流程或修改流程里暂存的 ID。 */
  char pending_id[512];

  /* 结果框标题。 */
  char result_title[64];
  /* 结果框正文缓存。 */
  char result_lines[RESULT_LINE_CAP][RESULT_LINE_LENGTH];
  /* 当前结果区有效行数。 */
  int result_line_count;

  /* 查询候选结果数组。 */
  const DataNode* search_results[SEARCH_LIMIT];
  /* 当前实际展示的候选数量。 */
  int search_count;
  /* 当前搜索命中的总数量。 */
  int search_total_count;
  /* 当前高亮的候选下标。 */
  int search_selected;

  /* 终端宽度。 */
  int term_width;
  /* 终端高度。 */
  int term_height;
} DisplayContext;

/* 全局显示上下文，程序从这里开始维护所有 UI 状态。 */
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

/* 菜单名称数组，顺序与 MenuItem 枚举一致。 */
static const char* menu_names[MENU_COUNT] = {
    "1. 添加数据", "2. 查询数据", "3. 删除数据", "4. 修改数据", "5. 保存文件",
    "6. 加载文件", "7. 新建文件", "8. 撤销操作", "9. 退出程序",
};

/* 动作名称数组，顺序与 CurrentAction 枚举一致。 */
static const char* action_names[] = {
    "空闲", "添加数据", "查询数据", "删除数据", "修改数据", "保存文件", "加载文件", "新建文件",
};

static void move_cursor(int y, int x) {
  /* 用 ANSI 转义序列把光标移动到指定坐标。 */
  printf("\033[%d;%dH", y, x);
}

static void print_repeat(const char* text, int count) {
  /* 连续输出同一段文本，主要用于绘制边框。 */
  for (int i = 0; i < count; ++i) {
    printf("%s", text);
  }
}

static void update_term_size(void) {
  /* info 用来接收控制台窗口的尺寸信息。 */
  CONSOLE_SCREEN_BUFFER_INFO info;

  /* 成功读取终端信息后，更新上下文中的宽高。 */
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    ctx.term_width = info.srWindow.Right - info.srWindow.Left + 1;
    ctx.term_height = info.srWindow.Bottom - info.srWindow.Top + 1;
  }
}

static int utf8_char_len(const unsigned char* s) {
  /* ASCII 字符只占 1 个字节。 */
  if (*s < 0x80) return 1;
  /* 2 字节 UTF-8。 */
  if ((*s & 0xE0) == 0xC0) return 2;
  /* 3 字节 UTF-8。 */
  if ((*s & 0xF0) == 0xE0) return 3;
  /* 4 字节 UTF-8。 */
  if ((*s & 0xF8) == 0xF0) return 4;
  /* 其余情况按单字节兜底。 */
  return 1;
}

static int utf8_char_width(const unsigned char* s) {
  /* ASCII 在终端里通常占 1 列。 */
  if (*s < 0x80) return 1;
  /* 中文和多数四字节字符按 2 列处理。 */
  if ((*s & 0xF0) == 0xE0 || (*s & 0xF8) == 0xF0) return 2;
  /* 两字节字符通常按 1 列处理。 */
  return 1;
}

static int prev_char_start(const char* text, int index) {
  /* cursor 从当前光标位置开始向前回退。 */
  int cursor = index;

  /* 已经在最左边时，前一个字符起点就是 0。 */
  if (cursor <= 0) return 0;

  /* 先退一个字节。 */
  cursor--;

  /* 如果退到了 UTF-8 延续字节，就继续向前找到字符起始字节。 */
  while (cursor > 0 && ((unsigned char)text[cursor] & 0xC0) == 0x80) {
    cursor--;
  }

  /* 返回上一个完整字符的起始位置。 */
  return cursor;
}

static int next_char_start(const char* text, int index, int length) {
  /* 光标已经在末尾时，直接保持在末尾。 */
  if (index >= length) return length;

  /* 当前字符起点加上字符字节长度，就是下一个字符起点。 */
  return index + utf8_char_len((const unsigned char*)&text[index]);
}

static void print_clamped_text(const char* text, int width) {
  /* used 统计当前已经占用的终端列宽。 */
  int used = 0;
  /* p 按字节扫描 UTF-8 字符串。 */
  const unsigned char* p = (const unsigned char*)text;

  /* 逐字符输出，直到文本结束或显示宽度耗尽。 */
  while (*p && used < width) {
    int char_len = utf8_char_len(p);
    int char_width = utf8_char_width(p);

    /* 当前字符如果放不下，就整体停止，避免半个宽字符被截断。 */
    if (used + char_width > width) break;

    /* 原样输出这个完整字符。 */
    fwrite(p, 1, (size_t)char_len, stdout);
    p += char_len;
    used += char_width;
  }

  /* 右侧剩余位置补空格，覆盖旧画面内容。 */
  while (used < width) {
    putchar(' ');
    used++;
  }
}

static void draw_box(int x, int y, int width, int height, const char* title) {
  /* 绘制上边框。 */
  move_cursor(y, x);
  printf(DIM "┌");
  print_repeat("─", width - 2);
  printf("┐" RESET);

  /* 绘制中间区域的左右边框。 */
  for (int row = 1; row < height - 1; ++row) {
    move_cursor(y + row, x);
    printf(DIM "│" RESET);
    print_repeat(" ", width - 2);
    printf(DIM "│" RESET);
  }

  /* 绘制下边框。 */
  move_cursor(y + height - 1, x);
  printf(DIM "└");
  print_repeat("─", width - 2);
  printf("┘" RESET);

  /* 标题非空时，把标题写到框顶部。 */
  if (title && title[0] != '\0') {
    move_cursor(y, x + 2);
    printf(BOLD);
    print_clamped_text(title, width - 4);
    printf(RESET);
  }
}

static void editor_reset(void) {
  /* 输入长度归零。 */
  ctx.input_len = 0;
  /* 光标回到开头。 */
  ctx.input_cursor = 0;
  /* 缓冲区写成空字符串。 */
  ctx.input_buf[0] = '\0';
}

static void editor_set_text(const char* text) {
  /* 安全复制文本到输入框。 */
  snprintf(ctx.input_buf, sizeof(ctx.input_buf), "%s", text);
  /* 刷新当前字节长度。 */
  ctx.input_len = (int)strlen(ctx.input_buf);
  /* 光标移动到文本末尾。 */
  ctx.input_cursor = ctx.input_len;
}

static void editor_move_left(void) {
  /* 按 UTF-8 字符边界左移一个字符。 */
  ctx.input_cursor = prev_char_start(ctx.input_buf, ctx.input_cursor);
}

static void editor_move_right(void) {
  /* 按 UTF-8 字符边界右移一个字符。 */
  ctx.input_cursor = next_char_start(ctx.input_buf, ctx.input_cursor, ctx.input_len);
}

static void editor_move_home(void) {
  /* Home 直接跳到输入框最左侧。 */
  ctx.input_cursor = 0;
}

static void editor_move_end(void) {
  /* End 直接跳到输入框最右侧。 */
  ctx.input_cursor = ctx.input_len;
}

static void editor_backspace(void) {
  /* start 保存待删除字符的起始位置。 */
  int start;

  /* 光标已经在最左边时，没有字符可删。 */
  if (ctx.input_cursor <= 0) return;

  /* 找到光标前那个完整字符的起始字节。 */
  start = prev_char_start(ctx.input_buf, ctx.input_cursor);

  /* 把右侧内容整体左移，覆盖被删除字符。 */
  memmove(ctx.input_buf + start, ctx.input_buf + ctx.input_cursor,
          (size_t)(ctx.input_len - ctx.input_cursor + 1));

  /* 更新输入总长度。 */
  ctx.input_len -= ctx.input_cursor - start;

  /* 光标回到删除位置。 */
  ctx.input_cursor = start;
}

static void editor_insert_bytes(const unsigned char* bytes, int byte_count) {
  /* 没有有效输入字节时直接返回。 */
  if (byte_count <= 0) return;
  /* 插入后会超过缓冲区上限时直接忽略。 */
  if (ctx.input_len + byte_count >= (int)sizeof(ctx.input_buf)) return;

  /* 先给新字符腾出空间。 */
  memmove(ctx.input_buf + ctx.input_cursor + byte_count,
          ctx.input_buf + ctx.input_cursor,
          (size_t)(ctx.input_len - ctx.input_cursor + 1));

  /* 把新输入的 UTF-8 字节写到光标处。 */
  memcpy(ctx.input_buf + ctx.input_cursor, bytes, (size_t)byte_count);

  /* 光标移动到新字符后方。 */
  ctx.input_cursor += byte_count;

  /* 输入总长度增加。 */
  ctx.input_len += byte_count;
}

static InputKey read_input_key(void) {
  /* key 默认初始化为空按键。 */
  InputKey key = {.type = KEY_NONE, .byte_count = 0};
  /* _getch 读取一个原始键码，不回显。 */
  int ch = _getch();

  /* 0 和 224 表示 Windows 扩展键前缀。 */
  if (ch == 0 || ch == 224) {
    /* 再读取真实扫描码。 */
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
    /* 扩展键解析完成后直接返回。 */
    return key;
  }

  /* 27 是 Esc。 */
  if (ch == 27) {
    /* wait_count 用来短暂等待后续字节，区分单独 Esc 和转义序列。 */
    int wait_count = 0;
    while (!_kbhit() && wait_count < 10) {
      Sleep(1);
      wait_count++;
    }

    /* 没有后续字节时，把它当作取消键。 */
    if (!_kbhit()) {
      key.type = KEY_ESCAPE;
      return key;
    }

    /* 继续读取转义序列。 */
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

    /* 其余无法识别的序列统一当作 Esc。 */
    key.type = KEY_ESCAPE;
    return key;
  }

  /* 回车在不同环境里可能是 '\r' 或 '\n'。 */
  if (ch == '\r' || ch == '\n') {
    key.type = KEY_ENTER;
    return key;
  }
  /* Backspace 兼容两种常见编码。 */
  if (ch == 8 || ch == 127) {
    key.type = KEY_BACKSPACE;
    return key;
  }
  /* 32 以上的值视为可打印字符或 UTF-8 首字节。 */
  if (ch >= 32) {
    /* 默认按单字节字符处理。 */
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

    /* 继续读取剩余 UTF-8 字节。 */
    for (int i = 1; i < byte_count; ++i) {
      int next = _getch();

      /* 后续字节不合法时，保留目前已经读到的部分。 */
      if (next == EOF || (next & 0xC0) != 0x80) {
        key.byte_count = i;
        return key;
      }
      key.bytes[i] = (unsigned char)next;
    }

    /* 记录最终字节数。 */
    key.byte_count = byte_count;
  }

  /* 返回解析好的按键信息。 */
  return key;
}

static void result_clear(void) {
  /* 清空结果标题。 */
  ctx.result_title[0] = '\0';
  /* 清空结果行数。 */
  ctx.result_line_count = 0;
}

static void result_set_title(const char* title) {
  /* 把标题安全复制到上下文中。 */
  snprintf(ctx.result_title, sizeof(ctx.result_title), "%s", title);
}

static void result_add_line(const char* format, ...) {
  /* args 用于处理格式化参数。 */
  va_list args;

  /* 超出缓存行数上限时，忽略额外内容。 */
  if (ctx.result_line_count >= RESULT_LINE_CAP) return;

  /* 初始化可变参数读取。 */
  va_start(args, format);

  /* 把格式化后的文本写入当前结果行。 */
  vsnprintf(ctx.result_lines[ctx.result_line_count],
            sizeof(ctx.result_lines[ctx.result_line_count]), format, args);

  /* 结束参数读取。 */
  va_end(args);

  /* 结果有效行数加一。 */
  ctx.result_line_count++;
}

static void result_set_message(const char* title, const char* format, ...) {
  /* args 用于格式化单行消息。 */
  va_list args;

  /* 先清空旧结果。 */
  result_clear();
  /* 设置新标题。 */
  result_set_title(title);
  /* 开始读取可变参数。 */
  va_start(args, format);
  /* 把单行消息写入第一行。 */
  vsnprintf(ctx.result_lines[0], sizeof(ctx.result_lines[0]), format, args);
  /* 结束读取参数。 */
  va_end(args);
  /* 当前结果区只保留一行正文。 */
  ctx.result_line_count = 1;
}

static void result_show_record(const char* title, const DataNode* node) {
  /* 先清空旧结果。 */
  result_clear();
  /* 设置标题。 */
  result_set_title(title);
  /* 逐行写入记录字段。 */
  result_add_line("姓名: %s", node->name);
  result_add_line("ID: %s", node->id);
  result_add_line("值: %s", node->value);
}

static void clear_pending(void) {
  /* 清空暂存姓名。 */
  ctx.pending_name[0] = '\0';
  /* 清空暂存 ID。 */
  ctx.pending_id[0] = '\0';
}

static void clear_search(void) {
  /* 当前展示的候选数量清零。 */
  ctx.search_count = 0;
  /* 总匹配数量清零。 */
  ctx.search_total_count = 0;
  /* 取消当前候选高亮。 */
  ctx.search_selected = -1;
}

static void enter_menu(void) {
  /* 回到菜单态。 */
  ctx.state = UI_MENU;
  /* 当前动作归零。 */
  ctx.action = ACTION_NONE;
  /* 步骤归零。 */
  ctx.step = 0;
  /* 清空输入框。 */
  editor_reset();
  /* 清空添加/修改流程中的暂存数据。 */
  clear_pending();
  /* 清空候选数据。 */
  clear_search();
}

static int is_query_step(void) {
  /* 查询动作始终属于查询阶段。 */
  if (ctx.action == ACTION_GET) return 1;
  /* 删除动作始终属于查询阶段。 */
  if (ctx.action == ACTION_DELETE) return 1;
  /* 修改动作只有第 0 步属于查询阶段。 */
  if (ctx.action == ACTION_MODIFY && ctx.step == 0) return 1;
  /* 其他情况都不属于查询阶段。 */
  return 0;
}

static CurrentAction action_from_menu(int menu) {
  /* 把菜单编号映射成内部动作枚举。 */
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
  /* 结果区内容需要跟当前动作和当前步骤保持一致。 */
  result_clear();
  result_set_title(action_names[ctx.action]);
  switch (ctx.action) {
    case ACTION_INSERT:
      /* 如果姓名已经录入过，就在结果区回显。 */
      if (ctx.pending_name[0] != '\0') result_add_line("姓名: %s", ctx.pending_name);
      /* 如果 ID 已经录入过，也回显出来。 */
      if (ctx.pending_id[0] != '\0') result_add_line("ID: %s", ctx.pending_id);
      /* 提示当前是添加流程的第几步。 */
      result_add_line("步骤 %d/3", ctx.step + 1);
      break;
    case ACTION_SAVE:
    case ACTION_LOAD:
    case ACTION_NEW:
      /* 文件类动作只需要输入文件名。 */
      result_add_line("请输入文件名后按 Enter。");
      break;
    case ACTION_MODIFY:
      /* 修改动作的第 1 步是输入新值。 */
      if (ctx.step == 1) {
        result_add_line("输入新值后按 Enter。");
      } else {
        /* 第 0 步先找目标记录。 */
        result_add_line("输入 ID 或姓名前缀。");
      }
      break;
    case ACTION_GET:
    case ACTION_DELETE:
      /* 查询和删除都先查记录。 */
      result_add_line("输入 ID 或姓名前缀。");
      break;
    default:
      /* 默认提示用户如何操作菜单。 */
      result_add_line("使用上下键选择菜单项。");
      break;
  }
}

static void set_file_loaded(const char* filename) {
  /* 更新当前文件名。 */
  snprintf(ctx.current_file, sizeof(ctx.current_file), "%s", filename);
  /* 标记当前已经有工作文件。 */
  ctx.file_loaded = 1;
}

static void refresh_query_results(void) {
  /* exact_match 保存按 ID 精确查找的结果。 */
  const DataNode* exact_match;
  /* matches 保存 Trie 返回的动态结果数组。 */
  DataNode** matches;

  /* 每次刷新都先清空旧候选和旧结果。 */
  clear_search();
  result_clear();
  result_set_title(action_names[ctx.action]);

  /* 输入为空时只显示基础提示。 */
  if (ctx.input_len == 0) {
    result_add_line("输入 ID 或姓名前缀。");
    return;
  }

  /* 先保证输入缓冲区是一个正常的 C 字符串。 */
  ctx.input_buf[ctx.input_len] = '\0';

  /* 优先尝试按 ID 精确查找。 */
  exact_match = data_find_by_id(ctx.input_buf);
  if (exact_match) {
    /* 精确命中时只保留这一条候选。 */
    ctx.search_results[0] = exact_match;
    ctx.search_count = 1;
    ctx.search_total_count = 1;
    ctx.search_selected = 0;
    result_show_record("精确匹配", exact_match);
    return;
  }

  /* 没有精确命中时，按姓名前缀搜索。 */
  matches = trie_search(ctx.input_buf, &ctx.search_total_count);
  if (ctx.search_total_count == 0) {
    result_add_line("没有匹配记录。");
    /* Trie 返回的数组需要由调用方释放。 */
    free(matches);
    return;
  }

  /* 展示数量受到 SEARCH_LIMIT 限制。 */
  ctx.search_count = ctx.search_total_count < SEARCH_LIMIT ? ctx.search_total_count : SEARCH_LIMIT;
  for (int i = 0; i < ctx.search_count; ++i) {
    /* 把动态数组中的指针复制到上下文缓存里。 */
    ctx.search_results[i] = matches[i];
  }
  /* 默认高亮第一项。 */
  ctx.search_selected = 0;
  /* 上下文缓存已经复制完毕，可以释放动态数组本体。 */
  free(matches);

  /* 唯一候选时，直接展示详情。 */
  if (ctx.search_total_count == 1) {
    result_show_record("唯一匹配", ctx.search_results[0]);
    return;
  }

  /* 多候选时改成候选列表模式。 */
  result_set_title("候选记录");
  result_add_line("共 %d 条匹配，使用上下键选择。", ctx.search_total_count);
  if (ctx.search_total_count > SEARCH_LIMIT) {
    /* 匹配过多时，提示用户当前只显示前几条。 */
    result_add_line("当前显示前 %d 条。", SEARCH_LIMIT);
  }
}

static void start_action(CurrentAction action) {
  /* 进入输入态。 */
  ctx.state = UI_INPUT;
  /* 设置当前动作。 */
  ctx.action = action;
  /* 每次新动作都从第 0 步开始。 */
  ctx.step = 0;
  /* 清空输入框。 */
  editor_reset();
  /* 清空暂存数据。 */
  clear_pending();
  /* 清空旧候选。 */
  clear_search();
  /* 刷新结果区提示。 */
  update_input_summary();
}

static void log_status_for_insert(DataStatus status, const char* name, const char* id) {
  /* 把插入结果翻译成用户可读日志。 */
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
  /* 把删除结果翻译成用户可读日志。 */
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
  /* 把修改结果翻译成用户可读日志。 */
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
  /* 保存动作单独处理。 */
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
  /* 加载动作单独处理。 */
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
  /* 剩余文件动作就是新建文件。 */
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
  /* 查询动作只需要展示记录。 */
  if (ctx.action == ACTION_GET) {
    result_show_record("查询结果", node);
    log_print("已显示 %s (%s)", node->name, node->id);
    enter_menu();
    return;
  }

  /* 删除动作要先缓存字段，再执行真正删除。 */
  if (ctx.action == ACTION_DELETE) {
    char name_copy[256];
    char id_copy[256];
    DataStatus status;

    /* 删除后原节点会释放，所以这里先拷贝一份内容。 */
    snprintf(name_copy, sizeof(name_copy), "%s", node->name);
    snprintf(id_copy, sizeof(id_copy), "%s", node->id);

    /* 执行带撤销记录的删除。 */
    status = data_delete(node->id, 1);
    log_status_for_delete(status, name_copy, id_copy);
    if (status == DATA_OK) {
      /* 删除成功后在结果区展示确认消息。 */
      result_set_message("删除数据", "已删除 %s (%s)", name_copy, id_copy);
      enter_menu();
    }
    return;
  }

  /* 走到这里说明当前动作是修改，先保存待修改记录的 ID。 */
  snprintf(ctx.pending_id, sizeof(ctx.pending_id), "%s", node->id);
  /* 切到修改流程的第 1 步。 */
  ctx.step = 1;
  /* 清空输入框，准备让用户输入新值。 */
  editor_reset();
  /* 修改新值时不再需要候选列表。 */
  clear_search();
  /* 先把旧记录展示给用户确认。 */
  result_show_record("待修改记录", node);
  /* 再补一行下一步提示。 */
  result_add_line("输入新值后按 Enter。");
}

static void submit_query_step(void) {
  /* exact_match 保存精确查找结果。 */
  const DataNode* exact_match;

  /* 空输入不允许提交。 */
  if (ctx.input_len == 0) {
    log_print("请输入 ID 或姓名前缀。");
    return;
  }

  /* 补齐字符串结束符。 */
  ctx.input_buf[ctx.input_len] = '\0';

  /* 优先按 ID 精确查找。 */
  exact_match = data_find_by_id(ctx.input_buf);
  if (exact_match) {
    /* 精确命中时直接提交这条记录。 */
    submit_query_match(exact_match);
    return;
  }

  /* 否则刷新前缀候选结果。 */
  refresh_query_results();
  if (ctx.search_total_count == 0) {
    log_print("没有找到匹配记录。");
    return;
  }

  /* 默认提交当前高亮候选。 */
  submit_query_match(ctx.search_results[ctx.search_selected]);
}

static void submit_insert_step(void) {
  /* status 保存插入最终结果。 */
  DataStatus status;
  /* node 用于插入成功后重新取回完整记录。 */
  const DataNode* node;

  /* 添加流程每一步都要求当前输入非空。 */
  if (ctx.input_len == 0) {
    log_print("请输入完整内容。");
    return;
  }

  /* 先把输入缓冲区补成合法字符串。 */
  ctx.input_buf[ctx.input_len] = '\0';
  if (ctx.step == 0) {
    /* 第 0 步采集姓名。 */
    snprintf(ctx.pending_name, sizeof(ctx.pending_name), "%s", ctx.input_buf);
    ctx.step = 1;
    editor_reset();
    update_input_summary();
    return;
  }
  if (ctx.step == 1) {
    /* 第 1 步采集 ID。 */
    snprintf(ctx.pending_id, sizeof(ctx.pending_id), "%s", ctx.input_buf);
    ctx.step = 2;
    editor_reset();
    update_input_summary();
    return;
  }

  /* 第 2 步才真正执行插入。 */
  status = data_insert(ctx.pending_name, ctx.pending_id, ctx.input_buf, 1);
  log_status_for_insert(status, ctx.pending_name, ctx.pending_id);
  if (status == DATA_OK) {
    /* 插入成功后优先展示刚插入的完整记录。 */
    node = data_find_by_id(ctx.pending_id);
    if (node) {
      result_show_record("添加完成", node);
    } else {
      result_set_message("添加完成", "已添加 %s (%s)", ctx.pending_name, ctx.pending_id);
    }
    enter_menu();
    return;
  }

  /* ID 冲突时，让用户停留在 ID 这一步重新输入。 */
  if (status == DATA_DUPLICATE_ID) {
    ctx.step = 1;
    editor_set_text(ctx.pending_id);
  } else {
    /* 其他错误时退回到姓名这一步。 */
    ctx.step = 0;
    editor_set_text(ctx.pending_name);
  }
  /* 刷新提示信息。 */
  update_input_summary();
}

static void submit_modify_value(void) {
  /* status 保存修改结果。 */
  DataStatus status;
  /* node 用于修改成功后重新展示新值。 */
  const DataNode* node;

  /* 新值不能为空。 */
  if (ctx.input_len == 0) {
    log_print("请输入新值。");
    return;
  }

  /* 补上字符串结束符。 */
  ctx.input_buf[ctx.input_len] = '\0';
  /* 调用数据层执行修改。 */
  status = data_modify(ctx.pending_id, ctx.input_buf, 1);
  log_status_for_modify(status, ctx.pending_id);
  if (status == DATA_OK) {
    /* 修改成功后优先展示最新记录。 */
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
  /* status 保存文件类动作的执行结果。 */
  DataStatus status;

  /* 文件名为空时不执行。 */
  if (ctx.input_len == 0) {
    log_print("请输入文件名。");
    return;
  }

  /* 补上字符串结束符。 */
  ctx.input_buf[ctx.input_len] = '\0';
  if (ctx.action == ACTION_SAVE) {
    /* 保存动作调用 data_save。 */
    status = data_save(ctx.input_buf);
  } else if (ctx.action == ACTION_LOAD) {
    /* 加载动作调用 data_load。 */
    status = data_load(ctx.input_buf);
  } else {
    /* 其余就是新建动作。 */
    status = data_new(ctx.input_buf);
  }

  /* 日志区显示执行结果。 */
  log_status_for_file(ctx.action, status, ctx.input_buf);
  if (status == DATA_OK) {
    /* 成功后更新当前文件名状态。 */
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

  /* 失败时保留在输入态，并刷新提示。 */
  update_input_summary();
}

static void submit_input(void) {
  /* 添加动作采用三步录入流程。 */
  if (ctx.action == ACTION_INSERT) {
    submit_insert_step();
    return;
  }
  /* 查询阶段统一先定位目标记录。 */
  if (is_query_step()) {
    submit_query_step();
    return;
  }
  /* 修改动作的第 1 步是写入新值。 */
  if (ctx.action == ACTION_MODIFY) {
    submit_modify_value();
    return;
  }
  /* 剩下的就是文件类动作。 */
  submit_file_action();
}

static void run_selected_menu_item(void) {
  /* 撤销是立即执行型动作，不进入输入态。 */
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
  /* 退出菜单项把主循环状态改成 UI_EXIT。 */
  if (ctx.selected_menu == MENU_EXIT) {
    ctx.state = UI_EXIT;
    return;
  }
  /* 其余菜单项进入对应的输入流程。 */
  start_action(action_from_menu(ctx.selected_menu));
}

static void rotate_menu_selection(int delta) {
  /* 使用取模运算实现循环菜单。 */
  ctx.selected_menu = (ctx.selected_menu + delta + MENU_COUNT) % MENU_COUNT;
}

static void handle_menu_key(InputKey key) {
  /* 菜单态下，方向键负责移动高亮，回车负责执行。 */
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
      /* 数字键 1 到 9 允许直接跳转并执行对应菜单项。 */
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
  /* 输入态下，同一个按键可能控制编辑器，也可能控制候选列表。 */
  switch (key.type) {
    case KEY_ESCAPE:
      /* Esc 直接取消当前动作并回菜单。 */
      enter_menu();
      log_print("操作已取消。");
      break;
    case KEY_ENTER:
      /* 回车提交当前输入。 */
      submit_input();
      break;
    case KEY_UP:
      /* 查询候选区向上移动高亮。 */
      if (is_query_step() && ctx.search_total_count > 1 && ctx.search_selected > 0) {
        ctx.search_selected--;
      }
      break;
    case KEY_DOWN:
      /* 查询候选区向下移动高亮。 */
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

  /* 查询阶段中，编辑输入内容后要实时刷新候选列表。 */
  if (is_query_step() &&
      (key.type == KEY_LEFT || key.type == KEY_RIGHT || key.type == KEY_HOME ||
       key.type == KEY_END || key.type == KEY_BACKSPACE || key.type == KEY_CHAR)) {
    refresh_query_results();
  }
}

static const char* current_prompt(void) {
  /* 根据动作和步骤决定当前输入提示文本。 */
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
  /* 根据脏标记决定当前状态文案。 */
  const char* save_state = data_is_modified() ? "已修改" : "已保存";

  /* 第一行显示系统标题。 */
  move_cursor(1, 2);
  printf(BOLD CYAN "学生信息管理系统" RESET);

  /* 第二行显示文件名和保存状态。 */
  move_cursor(2, 2);
  printf("文件: %s  状态: %s", ctx.current_file,
         ctx.file_loaded ? save_state : "未加载");
}

static void render_menu_box(int x, int y, int width, int height) {
  /* 先画菜单外框。 */
  draw_box(x, y, width, height, "菜单");
  for (int i = 0; i < MENU_COUNT && i < height - 2; ++i) {
    move_cursor(y + 1 + i, x + 2);
    /* 当前菜单项使用反色高亮。 */
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
  /* line 表示结果框内部当前输出到第几行。 */
  int line = 0;

  /* 先输出候选列表上方的说明文字。 */
  for (int i = 0; i < ctx.result_line_count && line < height; ++i, ++line) {
    move_cursor(y + line, x);
    print_clamped_text(ctx.result_lines[i], width);
  }

  /* 再输出候选记录列表。 */
  for (int i = 0; i < ctx.search_count && line < height; ++i, ++line) {
    char row[RESULT_LINE_LENGTH];

    /* 生成每一条候选显示文本。 */
    snprintf(row, sizeof(row), "%s %s (%s)", i == ctx.search_selected ? ">" : " ",
             ctx.search_results[i]->name, ctx.search_results[i]->id);
    move_cursor(y + line, x);
    /* 当前高亮候选同样使用反色。 */
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
  /* 标题为空时使用默认标题“结果”。 */
  draw_box(x, y, width, height, ctx.result_title[0] != '\0' ? ctx.result_title : "结果");

  /* 多候选查询时，结果框切换为候选列表视图。 */
  if (ctx.search_total_count > 1 && is_query_step()) {
    render_search_list(x + 2, y + 1, width - 4, height - 2);
    return;
  }

  /* 普通模式下逐行输出结果文本。 */
  for (int i = 0; i < ctx.result_line_count && i < height - 2; ++i) {
    move_cursor(y + 1 + i, x + 2);
    print_clamped_text(ctx.result_lines[i], width - 4);
  }
}

static void render_input_value(int x, int y, int width) {
  /* field_width 是可用于显示输入内容的宽度。 */
  int field_width = width;
  /* start 表示当前可视窗口左边界在缓冲区中的起始字节位置。 */
  int start = ctx.input_cursor;
  /* used 统计已经占用的显示宽度。 */
  int used = 0;
  /* prefix 为 1 时表示左侧有隐藏内容。 */
  int prefix = 0;
  /* i 用来遍历输入缓冲区。 */
  int i;

  /* 从光标向左回退，找到一段能完整显示的窗口起点。 */
  while (start > 0) {
    int prev = prev_char_start(ctx.input_buf, start);
    int char_width = utf8_char_width((const unsigned char*)&ctx.input_buf[prev]);
    if (used + char_width >= field_width) break;
    used += char_width;
    start = prev;
  }

  /* 左侧还有隐藏内容时，用 '<' 做提示。 */
  if (start > 0 && field_width > 1) {
    prefix = 1;
    move_cursor(y, x);
    putchar('<');
    field_width--;
    x++;
  }

  /* 从可视窗口起点开始重新绘制可见内容。 */
  used = 0;
  i = start;
  while (used < field_width && i < ctx.input_len) {
    int next = next_char_start(ctx.input_buf, i, ctx.input_len);
    int char_width = utf8_char_width((const unsigned char*)&ctx.input_buf[i]);
    int char_len = next - i;
    if (used + char_width > field_width) break;

    move_cursor(y, x + used);
    /* 光标当前落在这个字符前面时，用反色绘制该字符。 */
    if (i == ctx.input_cursor) printf(INVERT);
    fwrite(ctx.input_buf + i, 1, (size_t)char_len, stdout);
    if (i == ctx.input_cursor) printf(RESET);
    used += char_width;
    i = next;
  }

  /* 光标在输入末尾时，需要把末尾空格高亮出来。 */
  if (ctx.input_cursor == ctx.input_len && used < field_width) {
    move_cursor(y, x + used);
    printf(INVERT " " RESET);
    used++;
  }

  /* 把剩余区域补空格，覆盖旧画面。 */
  move_cursor(y, x + used);
  while (used < field_width) {
    putchar(' ');
    used++;
  }

  /* 左侧有隐藏内容时，保持 '<' 提示。 */
  if (prefix) {
    move_cursor(y, x - 1);
    putchar('<');
  }
}

static void render_input_box(int x, int y, int width, int height) {
  /* title 保存输入框标题。 */
  char title[64];
  /* prompt_line 保存提示文本。 */
  char prompt_line[RESULT_LINE_LENGTH];

  /* 菜单态时，这个区域显示操作帮助。 */
  if (ctx.state == UI_MENU) {
    draw_box(x, y, width, height, "输入");
    move_cursor(y + 1, x + 2);
    print_clamped_text("按 Enter 开始当前菜单项，数字键可直接执行。", width - 4);
    move_cursor(y + 2, x + 2);
    print_clamped_text("方向键移动，Esc 保留给输入态取消操作。", width - 4);
    return;
  }

  /* 输入态时，标题显示当前动作名。 */
  snprintf(title, sizeof(title), "%s", action_names[ctx.action]);
  draw_box(x, y, width, height, title);

  /* 生成“提示: xxx”这一行。 */
  snprintf(prompt_line, sizeof(prompt_line), "提示: %s", current_prompt());
  move_cursor(y + 1, x + 2);
  print_clamped_text(prompt_line, width - 4);

  /* 第三行绘制可编辑输入内容。 */
  move_cursor(y + 2, x + 2);
  render_input_value(x + 2, y + 2, width - 4);
}

static void render_log_box(int x, int y, int width, int height) {
  /* 日志总行数。 */
  int line_count = log_get_line_count();
  /* 当前日志框可显示的行数。 */
  int visible_lines = height - 2;
  /* 日志过长时，只显示最后几行。 */
  int start = line_count > visible_lines ? line_count - visible_lines : 0;

  /* 先画日志框。 */
  draw_box(x, y, width, height, "日志");
  for (int i = 0; i < visible_lines; ++i) {
    const char* line = log_get_line(start + i);
    /* 越界时 log_get_line 会返回 NULL，这里用空串兜底。 */
    move_cursor(y + 1 + i, x + 2);
    print_clamped_text(line ? line : "", width - 4);
  }
}

static void render_footer(void) {
  /* 在底部输出快捷键提示。 */
  move_cursor(ctx.term_height, 2);
  printf(DIM "Enter 确认  Esc 取消输入  上下选择  左右移动光标  Home/End 跳转" RESET);
}

static void render_small_terminal_message(void) {
  /* 终端尺寸不足时，只显示最关键的提示信息。 */
  printf(CLEAR HOME YELLOW "终端至少需要 %dx%d，当前是 %dx%d。"
         RESET,
         MIN_TERM_WIDTH, MIN_TERM_HEIGHT, ctx.term_width, ctx.term_height);
}

static void render_screen(void) {
  /* 左侧菜单框的位置和尺寸。 */
  int menu_x = 2;
  int menu_y = 3;
  int menu_height = 11;
  int menu_width = MENU_WIDTH;

  /* 结果框紧贴菜单框右侧。 */
  int result_x = menu_x + menu_width + 1;
  int result_y = menu_y;
  int result_width = ctx.term_width - result_x - 1;
  int result_height = menu_height;

  /* 输入框位于菜单和结果框下方。 */
  int input_x = 2;
  int input_y = menu_y + menu_height;
  int input_width = ctx.term_width - 2;
  int input_height = 4;

  /* 日志框占据最底部剩余区域。 */
  int log_x = 2;
  int log_y = input_y + input_height;
  int log_width = ctx.term_width - 2;
  int log_height = ctx.term_height - log_y;

  /* 每次渲染前重新读取终端尺寸。 */
  update_term_size();
  /* 先清屏并把光标移动到左上角。 */
  printf(CLEAR HOME);

  /* 终端过小时，只显示尺寸警告。 */
  if (ctx.term_width < MIN_TERM_WIDTH || ctx.term_height < MIN_TERM_HEIGHT) {
    render_small_terminal_message();
    fflush(stdout);
    return;
  }

  /* 按固定顺序绘制完整界面。 */
  render_header();
  render_menu_box(menu_x, menu_y, menu_width, menu_height);
  render_result_box(result_x, result_y, result_width, result_height);
  render_input_box(input_x, input_y, input_width, input_height);
  render_log_box(log_x, log_y, log_width, log_height);
  render_footer();

  /* 立即刷新输出，保证界面及时显示。 */
  fflush(stdout);
}

static void wait_for_min_size(void) {
  /* 程序进入主循环前，持续等待直到终端足够大。 */
  while (1) {
    update_term_size();
    if (ctx.term_width >= MIN_TERM_WIDTH && ctx.term_height >= MIN_TERM_HEIGHT) {
      return;
    }
    /* 不够大时反复显示提示并稍作等待。 */
    render_small_terminal_message();
    fflush(stdout);
    Sleep(300);
  }
}

void display_init(void) {
  /* 取得标准输出控制台句柄。 */
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  /* mode 用于读取和修改控制台模式。 */
  DWORD mode = 0;

  /* 输出编码设置为 UTF-8，保证中文界面正常显示。 */
  SetConsoleOutputCP(CP_UTF8);
  /* 输入编码设置为 UTF-8，保证中文输入读取正确。 */
  SetConsoleCP(CP_UTF8);
  /* 设置 C 运行时本地化环境。 */
  setlocale(LC_ALL, ".UTF8");

  /* 读取当前控制台模式。 */
  GetConsoleMode(handle, &mode);
  /* 打开 ANSI 虚拟终端支持。 */
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  /* 写回新的模式。 */
  SetConsoleMode(handle, mode);

  /* 进入备用屏幕并隐藏光标。 */
  printf(ALT_SCREEN HOME HIDE_CURSOR);

  /* 初始化日志模块。 */
  log_init();
  /* 初始化数据模块。 */
  data_init();
  /* 设置欢迎信息。 */
  result_set_message("欢迎", "使用上下键或数字键选择操作。");
  /* 写入启动日志。 */
  log_print("系统就绪。");
}

void display_cleanup(void) {
  /* 恢复光标显示并回到主屏。 */
  printf(SHOW_CURSOR MAIN_SCREEN);
  /* 强制刷新，保证终端状态完全恢复。 */
  fflush(stdout);
}

void display_run(void) {
  /* 先等待终端达到最小尺寸要求。 */
  wait_for_min_size();
  while (ctx.state != UI_EXIT) {
    /* key 保存本轮输入事件。 */
    InputKey key;
    /* 先绘制当前帧。 */
    render_screen();
    /* 再读取用户按键。 */
    key = read_input_key();
    /* 菜单态和输入态分别进入不同处理逻辑。 */
    if (ctx.state == UI_MENU) {
      handle_menu_key(key);
    } else {
      handle_input_key(key);
    }
  }
}
