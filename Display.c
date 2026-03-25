#include "Display.h"

#include <conio.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Data.h"
#include "Log.h"
#include "Trie.h"

// ANSI 颜色码 - 柔和清新配色 (60-30-10)
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"

// 60% 主色 - 柔和蓝/白色系
#define PRIMARY "\033[38;5;68m"    // 柔和蓝
#define WHITE "\033[38;5;255m"     // 纯白
#define GRAY "\033[38;5;252m"      // 亮灰
#define DARKGRAY "\033[38;5;248m"  // 中灰

// 30% 次要色 - 马卡龙色系
#define SECONDARY "\033[38;5;153m"  // 淡蓝
#define MINT "\033[38;5;121m"       // 薄荷绿
#define LILAC "\033[38;5;183m"      // 淡紫

// 10% 强调色
#define ACCENT "\033[38;5;174m"   // 珊瑚粉
#define SUCCESS "\033[38;5;150m"  // 柔绿
#define WARNING "\033[38;5;222m"  // 奶油黄

// 背景色
#define BG_PRIMARY "\033[48;5;68m"    // 柔和蓝背景
#define BG_SELECTED "\033[48;5;153m"  // 淡蓝选中

// 光标控制
#define CLEAR "\033[2J"
#define HOME "\033[H"
#define HIDE "\033[?25l"
#define SHOW "\033[?25h"
#define ALT_SCREEN "\033[?1049h"
#define MAIN_SCREEN "\033[?1049l"
#define INVERT "\033[7m"

// ========== 显示上下文结构体 ==========
typedef struct {
  // UI 状态
  UIState state;
  int selected_menu;
  int file_loaded;
  char filename[512];

  // 输入缓冲区
  char input_buf[512];
  int input_len;
  int input_cursor;

  // 临时存储（多步骤输入）
  char temp_name[512];
  char temp_id[512];
  char temp_value[512];
  int input_step;

  // 弹窗
  char popup_text[1024];
  int popup_text_len;
  int popup_active;

  // 搜索状态
  DataNode* search_results[5];
  int search_count;
  int search_selected;

  // 终端尺寸
  int term_width;
  int term_height;

  // 布局位置（动态计算）
  int menu_y;
  int input_y;
  int search_y;
  int log_y;
} DisplayContext;

// 全局显示上下文
static DisplayContext ctx = {.state = STATE_MENU,
                             .selected_menu = 0,
                             .file_loaded = 0,
                             .filename = "未加载",
                             .input_len = 0,
                             .input_cursor = 0,
                             .input_step = 0,
                             .popup_text_len = 0,
                             .popup_active = 0,
                             .search_count = 0,
                             .search_selected = -1,
                             .term_width = 80,
                             .term_height = 25};

// ========== 内容输出函数（供 Data.c 调用） ==========

// 收集内容到弹窗
void display_content_print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int written =
      vsnprintf(ctx.popup_text + ctx.popup_text_len,
                sizeof(ctx.popup_text) - ctx.popup_text_len, format, args);
  va_end(args);
  if (written > 0 &&
      ctx.popup_text_len + written < (int)sizeof(ctx.popup_text)) {
    ctx.popup_text_len += written;
  }
}

// 显示/隐藏弹窗
void display_show_popup(int show) { ctx.popup_active = show; }

// 清空弹窗内容
void display_clear_content(void) {
  ctx.popup_text[0] = '\0';
  ctx.popup_text_len = 0;
}

// ========== 辅助函数 ==========

// 设置文件名并切换到菜单状态
static void set_filename_and_state(const char* filename) {
  snprintf(ctx.filename, sizeof(ctx.filename), "%s", filename);
  ctx.file_loaded = 1;
  ctx.state = STATE_MENU;
}

// 菜单项
static const char* menu_icons[MENU_COUNT] = {"📝", "🔍", "🗑️", "✏️", "💾",
                                             "📂", "📄", "↩️",  "🚪"};
static const char* menu_names[MENU_COUNT] = {
    "添加数据", "查询数据", "删除数据", "修改数据", "保存文件",
    "加载文件", "新建文件", "撤销操作", "退出程序"};

// 更新终端大小
static void update_term_size(void) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    ctx.term_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    ctx.term_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  }
}

// 移动光标
static void move_cursor(int y, int x) { printf("\033[%d;%dH", y, x); }

// 解码一个 UTF-8 字符，返回码点和字节数
static uint32_t utf8_decode(const char* s, int* len) {
  unsigned char c = (unsigned char)*s;
  if (c < 0x80) {
    *len = 1;
    return c;
  }
  if ((c & 0xE0) == 0xC0) {
    *len = 2;
    return ((c & 0x1F) << 6) | (s[1] & 0x3F);
  }
  if ((c & 0xF0) == 0xE0) {
    *len = 3;
    return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
  }
  *len = 4;
  return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) |
         (s[3] & 0x3F);
}

// 根据码点判断显示宽度
static int utf8_char_width(uint32_t cp) {
  if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0;
  if (cp >= 0x1100 &&
      (cp <= 0x115F || cp == 0x2329 || cp == 0x232A ||
       (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0x33BF) ||
       (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0xA4CF) ||
       (cp >= 0xA960 && cp <= 0xA97C) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
       (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19) ||
       (cp >= 0xFE30 && cp <= 0xFE6B) || (cp >= 0xFF01 && cp <= 0xFF60) ||
       (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F000 && cp <= 0x1FBFF) ||
       (cp >= 0x20000 && cp <= 0x2FFFD) || (cp >= 0x30000 && cp <= 0x3FFFD)))
    return 2;
  return 1;
}

// 计算字符串显示宽度（跳过 ANSI 转义序列）
static int display_width(const char* text) {
  int width = 0;
  for (const char* p = text; *p;) {
    unsigned char c = (unsigned char)*p;
    if (c == '\033' && *(p + 1) == '[') {
      p += 2;
      while (*p && *p != 'm') ++p;
      if (*p) ++p;
      continue;
    }
    int len;
    uint32_t cp = utf8_decode(p, &len);
    width += utf8_char_width(cp);
    p += len;
  }
  return width;
}

// 打印居中文本
static void print_center(int y, const char* text) {
  int width = display_width(text);
  int x = (ctx.term_width - width) / 2;
  if (x < 1) x = 1;
  move_cursor(y, x);
  printf("%s", text);
}

// 打印重复字符串
static void print_repeat_str(int y, int x, const char* str, int count) {
  move_cursor(y, x);
  for (int i = 0; i < count; ++i) printf("%s", str);
}

// 绘制标题 - 3D 立体艺术字
static void draw_title(void) {
  // ASCII 艺术字 - FROSTTEA
  const char* art[] = {
      "███████╗ ██████╗   ██████╗  ███████╗ ████████╗ ████████╗ ███████╗  "
      "█████╗ ",
      "██╔════╝ ██╔══██╗ ██╔═══██╗ ██╔════╝ ╚══██╔══╝ ╚══██╔══╝ ██╔════╝ "
      "██╔══██╗",
      "█████╗   ██████╔╝ ██║   ██║ ███████╗    ██║       ██║    █████╗   "
      "███████║",
      "██╔══╝   ██╔══██╗ ██║   ██║ ╚════██║    ██║       ██║    ██╔══╝   "
      "██╔══██║",
      "██║      ██║  ██║ ╚██████╔╝ ███████║    ██║       ██║    ███████╗ ██║  "
      "██║",
      "╚═╝      ╚═╝  ╚═╝  ╚═════╝  ╚══════╝    ╚═╝       ╚═╝    ╚══════╝ ╚═╝  "
      "╚═╝"};

  int art_width = display_width(art[0]);
  int y = 2;

  // 阴影
  for (int i = 0; i < 6; ++i) {
    move_cursor(y + i + 1, (ctx.term_width - art_width) / 2 + 2);
    printf(DIM DARKGRAY "%s" RESET, art[i]);
  }

  // 主标题
  for (int i = 0; i < 6; ++i) {
    move_cursor(y + i, (ctx.term_width - art_width) / 2);
    printf(BOLD PRIMARY "%s" RESET, art[i]);
  }

  // 副标题
  print_center(9, LILAC "学生信息管理系统" RESET);
}

// 打印指定显示宽度的文本，不足部分用空格填充
static void print_padded(const char* text, int width) {
  printf("%s", text);
  int used = display_width(text);
  for (int i = used; i < width; ++i) putchar(' ');
}

// 绘制菜单
static void draw_menu(void) {
  int y = ctx.menu_y;
  // 每项格式: " {icon} {name} "，计算实际最大显示宽度
  int item_width = 0;
  for (int i = 0; i < MENU_COUNT; ++i) {
    char buf[64];
    snprintf(buf, sizeof(buf), " %s %s ", menu_icons[i], menu_names[i]);
    int w = display_width(buf);
    if (w > item_width) item_width = w;
  }
  int total_width = 3 * item_width;
  int start_x = (ctx.term_width - total_width) / 2;
  if (start_x < 2) start_x = 2;

  for (int i = 0; i < MENU_COUNT; ++i) {
    int row = i / 3;
    int col = i % 3;
    int x = start_x + col * item_width;
    int line_y = y + row * 3;
    move_cursor(line_y, x);
    char item[64];
    snprintf(item, sizeof(item), " %s %s ", menu_icons[i], menu_names[i]);
    if (i == ctx.selected_menu) {
      printf(BG_SELECTED BOLD WHITE);
      print_padded(item, item_width);
      printf(RESET);
    } else {
      printf(GRAY);
      print_padded(item, item_width);
      printf(RESET);
    }
  }
}

// ========== 多行输入辅助函数 ==========

// 获取当前输入的提示文字
static const char* get_prompt_text(void) {
  switch (ctx.selected_menu) {
    case MENU_INSERT:
      if (ctx.input_step == 0) return "姓名";
      if (ctx.input_step == 1) return "ID";
      return "值";
    case MENU_GET:    return "ID/NAME";
    case MENU_DELETE:  return "ID";
    case MENU_MODIFY:
      if (ctx.input_step == 0) return "ID";
      return "新值";
    default: return "文件名";
  }
}

// UTF-8 字符显示宽度（中文=2，ASCII=1）
static int utf8_char_display_width(const unsigned char* s) {
  if (*s < 0x80) return 1;
  if ((*s & 0xE0) == 0xC0) return 1;
  if ((*s & 0xF0) == 0xE0) return 2;
  if ((*s & 0xF8) == 0xF0) return 2;
  return 1;
}

// 跳过一个 UTF-8 字符，返回字节长度
static int utf8_char_len(const unsigned char* s) {
  if (*s < 0x80) return 1;
  if ((*s & 0xE0) == 0xC0) return 2;
  if ((*s & 0xF0) == 0xE0) return 3;
  if ((*s & 0xF8) == 0xF0) return 4;
  return 1;
}

// 构建输入布局：positions[i] = 第 i 个显示位置对应的字节偏移
// 返回显示位置总数
static int build_input_layout(int* positions, int max_positions) {
  int n = 0;
  int i = 0;
  while (i < ctx.input_len && n < max_positions) {
    positions[n++] = i;
    i += utf8_char_len((const unsigned char*)&ctx.input_buf[i]);
  }
  if (n < max_positions) positions[n] = ctx.input_len;  // 末尾位置
  return n;
}

// ========== 绘制输入区 ==========
static void draw_input_area(void) {
  int y = ctx.input_y;
  int left = 2;
  int right = ctx.term_width - 1;
  int box_width = right - left - 1;
  int content_rows = 4;  // 内容行数

  // 上边框
  move_cursor(y, left);
  printf(SECONDARY "┌");
  print_repeat_str(y, left + 1, "─", box_width);
  printf("┐" RESET);

  if (ctx.state == STATE_MENU) {
    // 菜单状态：提示文字
    move_cursor(y + 1, left);
    printf(SECONDARY "│" RESET " " WARNING "按 1-9 或方向键选择，Enter 确认" RESET);
    move_cursor(y + 1, right);
    printf(SECONDARY "│" RESET);
    // 空行填充
    for (int row = 1; row < content_rows; row++) {
      move_cursor(y + 1 + row, left);
      printf(SECONDARY "│" RESET " ");
      move_cursor(y + 1 + row, right);
      printf(SECONDARY "│" RESET);
    }
  } else {
    // 输入状态：自动换行显示
    const char* prompt = get_prompt_text();

    // 计算布局
    int prompt_display_len = display_width(prompt) + 2;  // "提示: " 的显示宽度
    int first_line_width = box_width - 2 - prompt_display_len;
    int other_line_width = box_width - 4;
    if (first_line_width < 1) first_line_width = 1;
    if (other_line_width < 1) other_line_width = 1;

    // 构建字符布局
    int pos[512];
    int total = build_input_layout(pos, 512);

    // 计算光标在第几个显示位置
    int cursor_disp_col = 0;
    for (int i = 0; i < total; i++) {
      if (pos[i] >= ctx.input_cursor) break;
      cursor_disp_col += utf8_char_display_width(
          (const unsigned char*)&ctx.input_buf[pos[i]]);
    }

    // 计算光标在哪一行（按显示列数）
    int cursor_row = 0;
    {
      int acc = 0;
      for (int r = 0; r < content_rows; r++) {
        int lw = (r == 0) ? first_line_width : other_line_width;
        if (cursor_disp_col < acc + lw) {
          cursor_row = r;
          break;
        }
        acc += lw;
        cursor_row = r + 1;
      }
    }
    if (cursor_row >= content_rows) cursor_row = content_rows - 1;

    // 填充每行
    int char_idx = 0;
    for (int row = 0; row < content_rows; row++) {
      move_cursor(y + 1 + row, left);
      printf(SECONDARY "│" RESET " ");

      int line_width = (row == 0) ? first_line_width : other_line_width;

      // 第一行打印提示前缀
      if (row == 0) {
        printf(BOLD ACCENT "%s:" RESET " ", prompt);
      } else {
        // 后续行缩进对齐（按显示宽度）
        for (int p = 0; p < prompt_display_len; p++) printf(" ");
      }

      // 打印这一行的字符
      int col = 0;
      while (char_idx < total && col < line_width) {
        int byte_pos = pos[char_idx];
        int next_byte = (char_idx + 1 < total) ? pos[char_idx + 1] : ctx.input_len;
        int char_len = next_byte - byte_pos;
        int char_w = utf8_char_display_width(
            (const unsigned char*)&ctx.input_buf[byte_pos]);

        if (col + char_w > line_width) break;

        // 检查光标是否在这个字符之前
        if (row == cursor_row && ctx.input_cursor == byte_pos && ctx.state != STATE_MENU) {
          printf(INVERT);
          for (int b = 0; b < char_len; b++)
            putchar(ctx.input_buf[byte_pos + b]);
          printf(RESET);
        } else {
          for (int b = 0; b < char_len; b++)
            putchar(ctx.input_buf[byte_pos + b]);
        }
        col += char_w;
        char_idx++;
      }

      // 光标在末尾（只在光标所在行显示）
      if (row == cursor_row && ctx.input_cursor == pos[char_idx] && char_idx >= total &&
          ctx.state != STATE_MENU) {
        printf(INVERT " " RESET);
      }

      move_cursor(y + 1 + row, right);
      printf(SECONDARY "│" RESET);
    }

    // 步骤指示器覆盖最后一行末尾
    if (ctx.selected_menu == MENU_INSERT || ctx.selected_menu == MENU_MODIFY) {
      int step_row = content_rows - 1;
      move_cursor(y + 1 + step_row, left);
      printf(SECONDARY "│" RESET " ");
      for (int p = 0; p < prompt_display_len; p++) printf(" ");
      if (ctx.selected_menu == MENU_INSERT)
        printf(MINT "步骤 %d/3" RESET, ctx.input_step + 1);
      else
        printf(MINT "步骤 %d/2" RESET, ctx.input_step + 1);
      move_cursor(y + 1 + step_row, right);
      printf(SECONDARY "│" RESET);
    }
  }

  // 下边框
  move_cursor(y + 1 + content_rows, left);
  printf(SECONDARY "└");
  print_repeat_str(y + 1 + content_rows, left + 1, "─", box_width);
  printf("┘" RESET);
}

// 判断当前是否处于搜索模式
static int is_search_mode(void) {
  if (ctx.state == STATE_MENU) return 0;
  if (ctx.selected_menu == MENU_GET) return 1;
  if (ctx.selected_menu == MENU_DELETE) return 1;
  if (ctx.selected_menu == MENU_MODIFY && ctx.input_step == 0) return 1;
  return 0;
}

// 执行搜索
static void perform_search(void) {
  ctx.search_count = 0;
  ctx.search_selected = -1;
  if (ctx.input_len == 0) return;
  ctx.input_buf[ctx.input_len] = '\0';
  DataNode** results = trie_search(ctx.input_buf, &ctx.search_count);
  if (ctx.search_count > 5) ctx.search_count = 5;
  for (int i = 0; i < ctx.search_count; i++) {
    ctx.search_results[i] = results[i];
  }
  free(results);
  if (ctx.search_count > 0) ctx.search_selected = 0;
}

// 绘制搜索结果框
static void draw_search_results(void) {
  if (!is_search_mode()) return;
  int y = ctx.search_y;
  int box_height = ctx.search_count > 0 ? ctx.search_count + 2 : 3;
  int left = 2;
  int right = ctx.term_width - 1;
  int box_width = right - left - 1;

  move_cursor(y, left);
  printf(SECONDARY "┌");
  print_repeat_str(y, left + 1, "─", box_width);
  printf("┐" RESET);

  if (ctx.search_count == 0) {
    move_cursor(y + 1, left);
    printf(SECONDARY "│" RESET GRAY "  无匹配结果" RESET);
    move_cursor(y + 1, right);
    printf(SECONDARY "│" RESET);
  } else {
    for (int i = 0; i < ctx.search_count; i++) {
      move_cursor(y + 1 + i, left);
      printf(SECONDARY "│" RESET " ");
      if (i == ctx.search_selected) {
        printf(BG_SELECTED BOLD WHITE " %s (%s) " RESET,
               ctx.search_results[i]->name, ctx.search_results[i]->id);
      } else {
        printf(GRAY " %s (%s) " RESET, ctx.search_results[i]->name,
               ctx.search_results[i]->id);
      }
      move_cursor(y + 1 + i, right);
      printf(SECONDARY "│" RESET);
    }
  }

  int by = y + 1 + box_height - 2;
  move_cursor(by, left);
  printf(SECONDARY "└");
  print_repeat_str(by, left + 1, "─", box_width);
  printf("┘" RESET);
}

// 绘制日志区
static void draw_log_area(void) {
  int y = ctx.log_y;

  move_cursor(y, 2);
  printf(SUCCESS "📋 日志" RESET);
  print_repeat_str(y, 10, "─", ctx.term_width - 12);

  int count = log_get_line_count();
  int start = count > 2 ? count - 2 : 0;

  for (int i = 0; i < 2; ++i) {
    int idx = start + i;
    move_cursor(y + 1 + i, 3);
    printf(GRAY "%s" RESET, idx < count ? log_get_line(idx) : "");
  }
}

// 绘制底部状态栏
static void draw_footer(void) {
  int y = ctx.term_height;

  // 整行背景色
  move_cursor(y, 1);
  printf(BG_PRIMARY);

  // 填满整行
  for (int i = 0; i < ctx.term_width; ++i) putchar(' ');

  // 重新定位写内容
  move_cursor(y, 1);
  printf(WHITE);
  const char* st = !ctx.file_loaded
                       ? "● 未加载"
                       : (data_is_modified() ? "● 已修改" : "● 已保存");
  printf(" 📁 %s %s │ ESC:取消 ↑↓←→:选择 Enter:确认 ", ctx.filename, st);
  printf(RESET);
}

// 绘制弹窗
static void draw_popup(void) {
  if (!ctx.popup_active) return;

  // 弹窗尺寸自适应终端大小
  int min_width = 40;
  int min_height = 10;
  int pw =
      (ctx.term_width * 3 / 5 < min_width) ? min_width : ctx.term_width * 3 / 5;
  int ph = (ctx.term_height * 2 / 5 < min_height) ? min_height
                                                  : ctx.term_height * 2 / 5;

  // 确保弹窗不超过终端大小
  if (pw > ctx.term_width - 4) pw = ctx.term_width - 4;
  if (ph > ctx.term_height - 4) ph = ctx.term_height - 4;

  int px = (ctx.term_width - pw) / 2;
  int py = (ctx.term_height - ph) / 2;

  // 背景
  for (int i = 0; i < ph; ++i) {
    move_cursor(py + i, px);
    printf(BG_PRIMARY);
    for (int j = 0; j < pw; ++j) putchar(' ');
  }

  // 边框
  move_cursor(py, px);
  printf(BG_PRIMARY BOLD WHITE "┌");
  print_repeat_str(py, px + 1, "─", pw - 2);
  printf("┐" RESET);

  move_cursor(py, px + (pw - 12) / 2);
  printf(BG_PRIMARY BOLD WHITE "─ 查询结果 ─" RESET);

  // 内容
  int line = 0;
  char* p = ctx.popup_text;
  int max_lines = ph - 4;
  while (*p && line < max_lines) {
    char* end = strchr(p, '\n');
    if (!end) end = p + strlen(p);
    int len = end - p;
    if (len > pw - 4) len = pw - 4;
    move_cursor(py + 2 + line, px + 2);
    printf(BG_PRIMARY WHITE "%.*s", len, p);
    ++line;
    if (*end == '\n')
      p = end + 1;
    else
      break;
  }

  move_cursor(py + ph - 2, px + (pw - 18) / 2);
  printf(BG_PRIMARY WARNING "按任意键继续..." RESET);

  move_cursor(py + ph - 1, px);
  printf(BG_PRIMARY BOLD WHITE "└");
  print_repeat_str(py + ph - 1, px + 1, "─", pw - 2);
  printf("┘" RESET);
}

// 等待终端达到最小尺寸
static void wait_for_min_size(void) {
  while (1) {
    update_term_size();
    if (ctx.term_width >= 60 && ctx.term_height >= 24) return;
    printf(CLEAR HOME WARNING "终端太小，请放大到至少 60×24（当前 %d×%d）" RESET,
           ctx.term_width, ctx.term_height);
    fflush(stdout);
    Sleep(500);
  }
}

// 重绘
static void redraw(void) {
  update_term_size();
  printf(CLEAR HOME);

  // 终端太小时显示提示
  if (ctx.term_width < 60 || ctx.term_height < 24) {
    printf(HOME WARNING "终端太小，请放大到至少 60×24（当前 %d×%d）" RESET,
           ctx.term_width, ctx.term_height);
    fflush(stdout);
    return;
  }

  // 计算布局位置（自适应终端大小）
  ctx.menu_y = 10;                                          // 标题占 y=2-9
  int menu_end = ctx.menu_y + 9;                            // 菜单 3 行 × 3 行高
  ctx.input_y = menu_end + 1;
  int input_end = ctx.input_y + 6;                          // 输入框 6 行（4 内容 + 2 边框）
  ctx.search_y = input_end + 1;
  int search_max = ctx.search_y + 6;                        // 搜索框最多 7 行
  ctx.log_y = search_max + 1;
  // 如果终端太低，压缩间距
  if (ctx.log_y + 3 > ctx.term_height) {
    ctx.log_y = ctx.term_height - 3;
  }

  draw_title();
  draw_menu();
  draw_input_area();
  draw_search_results();
  draw_log_area();
  draw_footer();
  draw_popup();

  fflush(stdout);
}

// 重置输入
static void reset_input(void) {
  ctx.input_len = 0;
  ctx.input_cursor = 0;
  ctx.input_buf[0] = '\0';
}

// 处理字符（支持 UTF-8 多字节输入）
static void handle_char(int ch) {
  // Windows 扫描码
  if (ch == 0 || ch == 224) {
    ch = _getch();
    switch (ch) {
      case 75:  // 左
        if (ctx.input_cursor > 0) {
          ctx.input_cursor--;
          while (ctx.input_cursor > 0 &&
                 (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80)
            ctx.input_cursor--;
        }
        return;
      case 77:  // 右
        if (ctx.input_cursor < ctx.input_len) {
          ++ctx.input_cursor;
          while (ctx.input_cursor < ctx.input_len &&
                 (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80)
            ++ctx.input_cursor;
        }
        return;
      case 71: ctx.input_cursor = 0; return;             // Home
      case 79: ctx.input_cursor = ctx.input_len; return;  // End
    }
    return;
  }

  switch (ch) {
    case 8:  // Backspace
    case 127:
      if (ctx.input_cursor > 0) {
        int start = ctx.input_cursor - 1;
        while (start > 0 && (ctx.input_buf[start] & 0xC0) == 0x80)
          start--;
        int char_len = ctx.input_cursor - start;
        memmove(ctx.input_buf + start, ctx.input_buf + ctx.input_cursor,
                ctx.input_len - ctx.input_cursor + 1);
        ctx.input_cursor = start;
        ctx.input_len -= char_len;
      }
      break;
    default:
      if (ch >= 32 && ctx.input_len < 511) {
        int utf8_len = 1;
        if ((ch & 0xE0) == 0xC0)
          utf8_len = 2;
        else if ((ch & 0xF0) == 0xE0)
          utf8_len = 3;
        else if ((ch & 0xF8) == 0xF0)
          utf8_len = 4;

        if (ctx.input_len + utf8_len >= 512) {
          utf8_len = 512 - ctx.input_len - 1;
          if (utf8_len <= 0) return;
        }

        memmove(ctx.input_buf + ctx.input_cursor + utf8_len,
                ctx.input_buf + ctx.input_cursor,
                ctx.input_len - ctx.input_cursor + 1);

        ctx.input_buf[ctx.input_cursor++] = ch;

        for (int i = 1; i < utf8_len; ++i) {
          int next = _getch();
          if (next == EOF || (next & 0xC0) != 0x80) {
            ctx.input_cursor--;
            memmove(ctx.input_buf + ctx.input_cursor,
                    ctx.input_buf + ctx.input_cursor + 1,
                    ctx.input_len - ctx.input_cursor + 1);
            return;
          }
          ctx.input_buf[ctx.input_cursor++] = next;
        }
        ctx.input_len += utf8_len;
      }
      break;
  }
}

// 开始操作
static void start_op(void) {
  ctx.input_step = 0;
  reset_input();
  ctx.state = STATE_INPUT_NAME;
}

// 处理输入
static void process_input(void) {
  ctx.input_buf[ctx.input_len] = '\0';

  // 空输入检查
  if (ctx.input_len == 0) {
    log_print("输入不能为空");
    return;
  }

  switch (ctx.selected_menu) {
    case MENU_INSERT:
      if (ctx.input_step == 0) {
        snprintf(ctx.temp_name, sizeof(ctx.temp_name), "%s", ctx.input_buf);
        ctx.input_step = 1;
        reset_input();
      } else if (ctx.input_step == 1) {
        snprintf(ctx.temp_id, sizeof(ctx.temp_id), "%s", ctx.input_buf);
        ctx.input_step = 2;
        reset_input();
      } else {
        snprintf(ctx.temp_value, sizeof(ctx.temp_value), "%s", ctx.input_buf);
        data_insert(ctx.temp_name, ctx.temp_id, ctx.temp_value, 1);
        ctx.state = STATE_MENU;
        ctx.input_step = 0;
      }
      break;
    case MENU_GET:
      display_clear_content();
      data_get(ctx.input_buf);
      display_show_popup(ctx.popup_text[0] != '\0');
      ctx.state = STATE_MENU;
      ctx.search_count = 0;
      ctx.search_selected = -1;
      break;
    case MENU_DELETE:
      data_delete(ctx.input_buf, 1);
      ctx.state = STATE_MENU;
      break;
    case MENU_MODIFY:
      if (ctx.input_step == 0) {
        snprintf(ctx.temp_name, sizeof(ctx.temp_name), "%s", ctx.input_buf);
        ctx.input_step = 1;
        reset_input();
      } else {
        data_modify(ctx.temp_name, ctx.input_buf, 1);
        ctx.state = STATE_MENU;
        ctx.input_step = 0;
      }
      break;
    case MENU_SAVE:
      data_save(ctx.input_buf);
      set_filename_and_state(ctx.input_buf);
      break;
    case MENU_LOAD:
      data_load(ctx.input_buf);
      set_filename_and_state(ctx.input_buf);
      break;
    case MENU_NEW:
      data_new(ctx.input_buf);
      set_filename_and_state(ctx.input_buf);
      break;
  }
  reset_input();
}

// 初始化
void display_init(void) {
  // Windows UTF-8
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  setlocale(LC_ALL, ".UTF8");

  // 启用 ANSI 转义码
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(hOut, &mode);
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, mode);

  // 隐藏光标
  printf(ALT_SCREEN HOME HIDE);

  update_term_size();
  log_init();
  data_init();
}

void display_cleanup(void) {
  printf(SHOW MAIN_SCREEN);
  fflush(stdout);
}

// 主循环
void display_run(void) {
  int ch;
  log_print("系统就绪");

  // 启动时等待终端足够大
  wait_for_min_size();

  while (ctx.state != STATE_EXIT) {
    redraw();
    ch = _getch();

    // 弹窗
    if (ctx.popup_active) {
      ctx.popup_active = 0;
      continue;
    }

    if (ctx.state == STATE_MENU) {
      if (ch >= '1' && ch <= '9') {
        ctx.selected_menu = ch - '1';
        if (ctx.selected_menu == MENU_UNDO) {
          data_undo();
        } else if (ctx.selected_menu == MENU_EXIT) {
          ctx.state = STATE_EXIT;
        } else {
          start_op();
        }
      } else if (ch == 27) {
        // ANSI 方向键（等待少量时间让序列到达）
        { int w = 0; while (!_kbhit() && w < 10) { Sleep(1); w++; } }
        if (_kbhit()) {
          int next = _getch();
          if (next == '[' || next == 'O') {
            int code = _getch();
            int row = ctx.selected_menu / 3;
            int col = ctx.selected_menu % 3;
            switch (code) {
              case 'A': row = (row - 1 + 3) % 3; break;
              case 'B': row = (row + 1) % 3; break;
              case 'D': col = (col - 1 + 3) % 3; break;
              case 'C': col = (col + 1) % 3; break;
            }
            ctx.selected_menu = row * 3 + col;
          }
        }
      } else if (ch == 0 || ch == 224) {
        ch = _getch();
        int row = ctx.selected_menu / 3;
        int col = ctx.selected_menu % 3;
        switch (ch) {
          case 72:  // 上
            row = (row - 1 + 3) % 3;
            ctx.selected_menu = row * 3 + col;
            break;
          case 80:  // 下
            row = (row + 1) % 3;
            ctx.selected_menu = row * 3 + col;
            break;
          case 75:  // 左
            col = (col - 1 + 3) % 3;
            ctx.selected_menu = row * 3 + col;
            break;
          case 77:  // 右
            col = (col + 1) % 3;
            ctx.selected_menu = row * 3 + col;
            break;
        }
      } else if (ch == '\r' || ch == '\n') {
        if (ctx.selected_menu == MENU_UNDO)
          data_undo();
        else if (ctx.selected_menu == MENU_EXIT)
          ctx.state = STATE_EXIT;
        else
          start_op();
      }
    } else {
      if (ch == 27) {  // ESC 或 ANSI 转义序列
        // 等待少量时间让序列到达（最多 10ms，本地终端通常 < 1ms）
        { int w = 0; while (!_kbhit() && w < 10) { Sleep(1); w++; } }
        if (_kbhit()) {
          int next = _getch();
          if (next == '[' || next == 'O') {
            int code = _getch();
            // 只处理方向键
            switch (code) {
              case 'A': case 'B': break;  // 上下忽略
              case 'C':  // 右
                if (ctx.input_cursor < ctx.input_len) {
                  ++ctx.input_cursor;
                  while (ctx.input_cursor < ctx.input_len &&
                         (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80)
                    ++ctx.input_cursor;
                }
                break;
              case 'D':  // 左
                if (ctx.input_cursor > 0) {
                  ctx.input_cursor--;
                  while (ctx.input_cursor > 0 &&
                         (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80)
                    ctx.input_cursor--;
                }
                break;
              case 'H': ctx.input_cursor = 0; break;  // Home
              case 'F': ctx.input_cursor = ctx.input_len; break;  // End
            }
          }
        } else {
          // 单独 ESC — 取消操作
          ctx.state = STATE_MENU;
          ctx.input_step = 0;
          reset_input();
          ctx.search_count = 0;
          ctx.search_selected = -1;
          log_print("操作已取消");
        }
      } else if (ch == '\r' || ch == '\n') {
        // 搜索模式下有选中结果
        if (is_search_mode() && ctx.search_selected >= 0 &&
            ctx.search_selected < ctx.search_count) {
          DataNode* sel = ctx.search_results[ctx.search_selected];
          if (ctx.selected_menu == MENU_GET) {
            ctx.state = STATE_MENU;
            display_clear_content();
            data_get(sel->id);
            display_show_popup(ctx.popup_text[0] != '\0');
            ctx.search_count = 0;
            ctx.search_selected = -1;
            reset_input();
          } else if (ctx.selected_menu == MENU_DELETE) {
            data_delete(sel->id, 1);
            ctx.state = STATE_MENU;
            ctx.search_count = 0;
            ctx.search_selected = -1;
            reset_input();
          } else if (ctx.selected_menu == MENU_MODIFY) {
            snprintf(ctx.temp_name, sizeof(ctx.temp_name), "%s", sel->id);
            ctx.input_step = 1;
            ctx.search_count = 0;
            ctx.search_selected = -1;
            reset_input();
          }
        } else {
          process_input();
        }
      } else if (ch == 0 || ch == 224) {
        ch = _getch();
        // 搜索模式下方向键选择搜索结果
        if (is_search_mode() && ctx.search_count > 0) {
          if (ch == 72 && ctx.search_selected > 0) {  // 上
            ctx.search_selected--;
          } else if (ch == 80 &&
                     ctx.search_selected < ctx.search_count - 1) {  // 下
            ctx.search_selected++;
          }
        } else {
          handle_char(ch);
        }
      } else {
        handle_char(ch);
        // 搜索模式下每次输入后触发搜索
        if (is_search_mode()) {
          perform_search();
        }
      }
    }
  }
}
