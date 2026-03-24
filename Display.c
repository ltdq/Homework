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

extern int file_modified;

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

// 状态
static UIState current_state = STATE_MENU;
static int selected_menu = 0;
static char current_filename[256] = "未加载";
static int file_loaded = 0;

// 输入
static char input_buffer[512];
static int input_length = 0;
static int input_cursor = 0;

// 临时存储
static char temp_name[256];
static char temp_id[256];
static char temp_value[256];
static int input_step = 0;

// 弹窗
static char popup_text[1024];
static int popup_text_len = 0;
static int popup_active = 0;

// 内容输出（供 Data 调用，收集到弹窗）
void display_content_print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int written = vsnprintf(popup_text + popup_text_len,
                          sizeof(popup_text) - popup_text_len, format, args);
  va_end(args);
  if (written > 0 && popup_text_len + written < (int)sizeof(popup_text)) {
    popup_text_len += written;
  }
}

// 显示弹窗
void display_show_popup(int show) { popup_active = show; }

// 清空弹窗内容
void display_clear_content(void) {
  popup_text[0] = '\0';
  popup_text_len = 0;
}

// 终端大小
static int term_width = 80;
static int term_height = 25;

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
    term_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    term_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
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
  int x = (term_width - len) / 2;
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
    move_cursor(y + i + 1, (term_width - art_width) / 2 + 2);
    printf(DIM DARKGRAY "%s" RESET, art[i]);
  }

  // 主标题
  for (int i = 0; i < 6; i++) {
    move_cursor(y + i, (term_width - art_width) / 2);
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
  int start_x = (term_width - total_width) / 2;
  if (start_x < 2) start_x = 2;

  for (int i = 0; i < MENU_COUNT; i++) {
    int row = i / 3;
    int col = i % 3;
    int x = start_x + col * menu_width;
    int line_y = y + row * 3;

    move_cursor(line_y, x);

    if (i == selected_menu) {
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
  int right = term_width - 1;
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
      if (current_state == STATE_MENU) {
        printf(WARNING "按 1-9 或方向键选择，Enter 确认" RESET);
      } else {
        const char* prompt = "";
        switch (selected_menu) {
          case MENU_INSERT:
            if (input_step == 0)
              prompt = "姓名";
            else if (input_step == 1)
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
            if (input_step == 0)
              prompt = "姓名/ID";
            else
              prompt = "新值";
            break;
          default:
            prompt = "文件名";
            break;
        }
        printf(BOLD ACCENT "%s:" RESET " %s", prompt, input_buffer);
      }
    } else {
      if (selected_menu == MENU_INSERT) {
        printf(MINT "步骤 %d/3" RESET, input_step + 1);
      } else if (selected_menu == MENU_MODIFY) {
        printf(MINT "步骤 %d/2" RESET, input_step + 1);
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
  print_repeat_str(y, 10, "─", term_width - 12);

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
  int y = term_height;

  // 整行背景色
  move_cursor(y, 1);
  printf(BG_PRIMARY);

  // 填满整行
  for (int i = 0; i < term_width; i++) putchar(' ');

  // 重新定位写内容
  move_cursor(y, 1);
  printf(WHITE);

  const char* st =
      !file_loaded ? "● 未加载" : (file_modified ? "● 已修改" : "● 已保存");
  printf(" 📁 %s  %s  │  ESC:取消  ↑↓←→:选择  Enter:确认 ", current_filename,
         st);

  printf(RESET);
}

// 绘制弹窗
static void draw_popup(void) {
  if (!popup_active) return;

  int pw = 50;
  int ph = 12;
  int px = (term_width - pw) / 2;
  int py = (term_height - ph) / 2;

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
  char* p = popup_text;
  while (*p && line < ph - 4) {
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
  input_length = 0;
  input_cursor = 0;
  input_buffer[0] = '\0';
}

// 处理字符
static void handle_char(int ch) {
  if (ch == 0 || ch == 224) {  // 功能键前缀
    ch = _getch();
    switch (ch) {
      case 75:  // 左
        if (input_cursor > 0) input_cursor--;
        break;
      case 77:  // 右
        if (input_cursor < input_length) input_cursor++;
        break;
      case 71:  // Home
        input_cursor = 0;
        break;
      case 79:  // End
        input_cursor = input_length;
        break;
    }
    return;
  }

  switch (ch) {
    case 8:  // Backspace
    case 127:
      if (input_cursor > 0) {
        memmove(input_buffer + input_cursor - 1, input_buffer + input_cursor,
                input_length - input_cursor + 1);
        input_cursor--;
        input_length--;
      }
      break;
    default:
      // 支持 UTF-8 输入（包括中文）
      if (ch >= 32 && input_length < 511) {
        memmove(input_buffer + input_cursor + 1, input_buffer + input_cursor,
                input_length - input_cursor + 1);
        input_buffer[input_cursor++] = ch;
        input_length++;
      }
      break;
  }
}

// 开始操作
static void start_op(void) {
  input_step = 0;
  reset_input();
  current_state = STATE_INPUT_NAME;
}

// 处理输入
static void process_input(void) {
  input_buffer[input_length] = '\0';

  // 空输入检查
  if (input_length == 0) {
    log_print("输入不能为空");
    return;
  }

  switch (selected_menu) {
    case MENU_INSERT:
      if (input_step == 0) {
        strncpy(temp_name, input_buffer, 255);
        input_step = 1;
        reset_input();
      } else if (input_step == 1) {
        strncpy(temp_id, input_buffer, 255);
        input_step = 2;
        reset_input();
      } else {
        strncpy(temp_value, input_buffer, 255);
        data_insert(temp_name, temp_id, temp_value, 1);
        current_state = STATE_MENU;
        input_step = 0;
      }
      break;
    case MENU_GET:
      display_clear_content();
      data_get(input_buffer);
      display_show_popup(popup_text[0] != '\0');
      current_state = STATE_MENU;
      break;
    case MENU_DELETE:
      data_delete(input_buffer, 1);
      current_state = STATE_MENU;
      break;
    case MENU_MODIFY:
      if (input_step == 0) {
        strncpy(temp_name, input_buffer, 255);
        input_step = 1;
        reset_input();
      } else {
        data_modify(temp_name, input_buffer, 1);
        current_state = STATE_MENU;
        input_step = 0;
      }
      break;
    case MENU_SAVE:
      data_save(input_buffer);
      strncpy(current_filename, input_buffer, 255);
      file_loaded = 1;
      current_state = STATE_MENU;
      break;
    case MENU_LOAD:
      data_load(input_buffer);
      strncpy(current_filename, input_buffer, 255);
      file_loaded = 1;
      current_state = STATE_MENU;
      break;
    case MENU_NEW:
      data_new(input_buffer);
      strncpy(current_filename, input_buffer, 255);
      file_loaded = 1;
      current_state = STATE_MENU;
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

  while (current_state != STATE_EXIT) {
    redraw();
    ch = _getch();

    // 弹窗
    if (popup_active) {
      popup_active = 0;
      continue;
    }

    if (current_state == STATE_MENU) {
      if (ch >= '1' && ch <= '9') {
        selected_menu = ch - '1';
        if (selected_menu == MENU_UNDO) {
          data_undo();
        } else if (selected_menu == MENU_EXIT) {
          current_state = STATE_EXIT;
        } else {
          start_op();
        }
      } else if (ch == 0 || ch == 224) {
        ch = _getch();
        int row = selected_menu / 3;
        int col = selected_menu % 3;
        switch (ch) {
          case 72:  // 上
            row = (row - 1 + 3) % 3;
            selected_menu = row * 3 + col;
            break;
          case 80:  // 下
            row = (row + 1) % 3;
            selected_menu = row * 3 + col;
            break;
          case 75:  // 左
            col = (col - 1 + 3) % 3;
            selected_menu = row * 3 + col;
            break;
          case 77:  // 右
            col = (col + 1) % 3;
            selected_menu = row * 3 + col;
            break;
        }
      } else if (ch == '\r' || ch == '\n') {
        if (selected_menu == MENU_UNDO)
          data_undo();
        else if (selected_menu == MENU_EXIT)
          current_state = STATE_EXIT;
        else
          start_op();
      }
    } else {
      if (ch == 27) {  // ESC
        current_state = STATE_MENU;
        input_step = 0;
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
