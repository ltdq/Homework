#include "Display.h"

#include <conio.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Data.h"
#include "Log.h"

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

// ========== 显示上下文结构体 ==========
typedef struct {
  // UI 状态
  UIState state;
  int selected_menu;
  int file_loaded;
  char filename[256];

  // 输入缓冲区
  char input_buf[512];
  int input_len;
  int input_cursor;

  // 临时存储（多步骤输入）
  char temp_name[256];
  char temp_id[256];
  char temp_value[256];
  int input_step;

  // 弹窗
  char popup_text[1024];
  int popup_text_len;
  int popup_active;

  // 终端尺寸
  int term_width;
  int term_height;
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

// 安全复制字符串并添加终止符
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
  snprintf(dest, dest_size, "%s", src);
}

// 设置文件名并切换到菜单状态
static void set_filename_and_state(const char* filename) {
  safe_strcpy(ctx.filename, filename, sizeof(ctx.filename));
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

// 打印居中文本
static void print_center(int y, const char* text) {
  int len = 0;
  // 计算显示宽度（跳过ANSI转义序列，中文算2）
  for (const char* p = text; *p; p++) {
    unsigned char c = (unsigned char)*p;
    // 跳过ANSI转义序列: \033[...m
    if (c == '\033' && *(p + 1) == '[') {
      p += 2;
      while (*p && *p != 'm') p++;
      continue;
    }
    if (c < 0x80)
      len++;
    else if ((c & 0xE0) == 0xC0) {
      len += 1;
      p++;
    } else if ((c & 0xF0) == 0xE0) {
      len += 2;
      p += 2;
    } else if ((c & 0xF8) == 0xF0) {
      len += 2;
      p += 3;
    } else
      len++;
  }
  int x = (ctx.term_width - len) / 2;
  if (x < 1) x = 1;
  move_cursor(y, x);
  printf("%s", text);
}

// 打印重复字符串
static void print_repeat_str(int y, int x, const char* str, int count) {
  move_cursor(y, x);
  for (int i = 0; i < count; i++) printf("%s", str);
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

  int art_width = 66;
  int y = 2;

  // 阴影
  for (int i = 0; i < 6; i++) {
    move_cursor(y + i + 1, (ctx.term_width - art_width) / 2 + 2);
    printf(DIM DARKGRAY "%s" RESET, art[i]);
  }

  // 主标题
  for (int i = 0; i < 6; i++) {
    move_cursor(y + i, (ctx.term_width - art_width) / 2);
    printf(BOLD PRIMARY "%s" RESET, art[i]);
  }

  // 副标题
  print_center(9, LILAC "学生信息管理系统" RESET);
}

// 绘制菜单
static void draw_menu(void) {
  int y = 11;
  int menu_width = 22;
  int total_width = 3 * menu_width;
  int start_x = (ctx.term_width - total_width) / 2;
  if (start_x < 2) start_x = 2;

  for (int i = 0; i < MENU_COUNT; i++) {
    int row = i / 3;
    int col = i % 3;
    int x = start_x + col * menu_width;
    int line_y = y + row * 3;
    move_cursor(line_y, x);
    if (i == ctx.selected_menu) {
      printf(BG_SELECTED BOLD WHITE " %s %-8s " RESET, menu_icons[i],
             menu_names[i]);
    } else {
      printf(GRAY " %s %-8s " RESET, menu_icons[i], menu_names[i]);
    }
  }
}

// 绘制输入区
static void draw_input_area(void) {
  int y = 21;
  int left = 2;
  int right = ctx.term_width - 1;
  int box_width = right - left - 1;

  // 上边框
  move_cursor(y, left);
  printf(SECONDARY);
  printf("┌");
  print_repeat_str(y, left + 1, "─", box_width);
  printf("┐");
  printf(RESET);

  // 中间两行
  for (int row = 0; row < 2; row++) {
    move_cursor(y + 1 + row, left);
    printf(SECONDARY "│" RESET " ");

    if (row == 0) {
      if (ctx.state == STATE_MENU) {
        printf(WARNING "按 1-9 或方向键选择，Enter 确认" RESET);
      } else {
        const char* prompt = "";
        switch (ctx.selected_menu) {
          case MENU_INSERT:
            if (ctx.input_step == 0)
              prompt = "姓名";
            else if (ctx.input_step == 1)
              prompt = "ID";
            else
              prompt = "值";
            break;
          case MENU_GET:
            prompt = "姓名/ID";
            break;
          case MENU_DELETE:
            prompt = "ID";
            break;
          case MENU_MODIFY:
            if (ctx.input_step == 0)
              prompt = "姓名/ID";
            else
              prompt = "新值";
            break;
          default:
            prompt = "文件名";
            break;
        }
        printf(BOLD ACCENT "%s:" RESET " %s", prompt, ctx.input_buf);
      }
    } else {
      if (ctx.selected_menu == MENU_INSERT) {
        printf(MINT "步骤 %d/3" RESET, ctx.input_step + 1);
      } else if (ctx.selected_menu == MENU_MODIFY) {
        printf(MINT "步骤 %d/2" RESET, ctx.input_step + 1);
      }
    }

    move_cursor(y + 1 + row, right);
    printf(SECONDARY "│" RESET);
  }

  // 下边框
  move_cursor(y + 3, left);
  printf(SECONDARY);
  printf("└");
  print_repeat_str(y + 3, left + 1, "─", box_width);
  printf("┘");
  printf(RESET);
}

// 绘制日志区
static void draw_log_area(void) {
  int y = 26;

  move_cursor(y, 2);
  printf(SUCCESS "📋 日志" RESET);
  print_repeat_str(y, 10, "─", ctx.term_width - 12);

  int count = log_get_line_count();
  int start = count > 2 ? count - 2 : 0;

  for (int i = 0; i < 2; i++) {
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
  for (int i = 0; i < ctx.term_width; i++) putchar(' ');

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
  for (int i = 0; i < ph; i++) {
    move_cursor(py + i, px);
    printf(BG_PRIMARY);
    for (int j = 0; j < pw; j++) putchar(' ');
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
    line++;
    if (*end == '\n')
      p = end + 1;
    else
      break;
  }

  move_cursor(py + ph - 2, px + (pw - 18) / 2);
  printf(BG_PRIMARY WARNING "按任意键继续..." RESET);

  move_cursor(py + ph - 1, px);
  printf(BG_PRIMARY BOLD WHITE);
  print_repeat_str(py + ph - 1, px + 1, "─", pw - 2);
  printf("┘" RESET);
}

// 重绘
static void redraw(void) {
  update_term_size();
  printf(CLEAR HOME);

  draw_title();
  draw_menu();
  draw_input_area();
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
  // 功能键前缀
  if (ch == 0 || ch == 224) {
    ch = _getch();
    switch (ch) {
      case 75:  // 左
        if (ctx.input_cursor > 0) {
          // 移动到前一个 UTF-8 字符的开头
          ctx.input_cursor--;
          while (ctx.input_cursor > 0 &&
                 (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80) {
            ctx.input_cursor--;
          }
        }
        break;
      case 77:  // 右
        if (ctx.input_cursor < ctx.input_len) {
          // 移动到下一个 UTF-8 字符的开头
          ctx.input_cursor++;
          while (ctx.input_cursor < ctx.input_len &&
                 (ctx.input_buf[ctx.input_cursor] & 0xC0) == 0x80) {
            ctx.input_cursor++;
          }
        }
        break;
      case 71:  // Home
        ctx.input_cursor = 0;
        break;
      case 79:  // End
        ctx.input_cursor = ctx.input_len;
        break;
    }
    return;
  }

  switch (ch) {
    case 8:  // Backspace
    case 127:
      if (ctx.input_cursor > 0) {
        // 找到前一个 UTF-8 字符的开头
        int start = ctx.input_cursor - 1;
        while (start > 0 && (ctx.input_buf[start] & 0xC0) == 0x80) {
          start--;
        }
        int char_len = ctx.input_cursor - start;
        memmove(ctx.input_buf + start, ctx.input_buf + ctx.input_cursor,
                ctx.input_len - ctx.input_cursor + 1);
        ctx.input_cursor = start;
        ctx.input_len -= char_len;
      }
      break;
    default:
      // 支持 UTF-8 输入（包括中文）
      if (ch >= 32 && ctx.input_len < 511) {
        // 检测 UTF-8 首字节，计算需要的字节数
        int utf8_len = 1;
        if ((ch & 0xE0) == 0xC0)
          utf8_len = 2;  // 2字节字符
        else if ((ch & 0xF0) == 0xE0)
          utf8_len = 3;  // 3字节字符
        else if ((ch & 0xF8) == 0xF0)
          utf8_len = 4;  // 4字节字符

        // 确保缓冲区有足够空间
        if (ctx.input_len + utf8_len >= 512) {
          utf8_len = 512 - ctx.input_len - 1;
          if (utf8_len <= 0) return;
        }

        // 移动现有内容腾出空间
        memmove(ctx.input_buf + ctx.input_cursor + utf8_len,
                ctx.input_buf + ctx.input_cursor,
                ctx.input_len - ctx.input_cursor + 1);

        // 写入首字节
        ctx.input_buf[ctx.input_cursor++] = ch;

        // 如果是多字节字符，读取后续字节
        for (int i = 1; i < utf8_len; i++) {
          int next = _getch();
          if (next == EOF || (next & 0xC0) != 0x80) {
            // 无效的后续字节，回滚
            ctx.input_cursor--;
            ctx.input_len -= i - 1;
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
        safe_strcpy(ctx.temp_name, ctx.input_buf, sizeof(ctx.temp_name));
        ctx.input_step = 1;
        reset_input();
      } else if (ctx.input_step == 1) {
        safe_strcpy(ctx.temp_id, ctx.input_buf, sizeof(ctx.temp_id));
        ctx.input_step = 2;
        reset_input();
      } else {
        safe_strcpy(ctx.temp_value, ctx.input_buf, sizeof(ctx.temp_value));
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
      break;
    case MENU_DELETE:
      data_delete(ctx.input_buf, 1);
      ctx.state = STATE_MENU;
      break;
    case MENU_MODIFY:
      if (ctx.input_step == 0) {
        safe_strcpy(ctx.temp_name, ctx.input_buf, sizeof(ctx.temp_name));
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
  printf(HIDE);

  update_term_size();
  log_init();
  data_init();
}

void display_cleanup(void) {
  printf(SHOW CLEAR);
  fflush(stdout);
}

// 主循环
void display_run(void) {
  int ch;
  log_print("系统就绪");

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
      if (ch == 27) {  // ESC
        ctx.state = STATE_MENU;
        ctx.input_step = 0;
        reset_input();
        log_print("操作已取消");
      } else if (ch == '\r' || ch == '\n') {
        process_input();
      } else {
        handle_char(ch);
      }
    }
  }
}
