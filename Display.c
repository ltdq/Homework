#include "Display.h"

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

// ============================================================================
// 配色方案 - 现代深色主题 (Tokyo Night)
// ============================================================================
#define COLOR_BG        0x1a1b26
#define COLOR_FG        0xc0caf5
#define COLOR_ACCENT    0x7aa2f7
#define COLOR_SUCCESS   0x9ece6a
#define COLOR_WARNING   0xe0af68
#define COLOR_ERROR     0xf7768e
#define COLOR_MUTED     0x565f89
#define COLOR_MENU_BG   0x24283b
#define COLOR_INPUT_BG  0x1f2335

// ============================================================================
// 布局常量
// ============================================================================
#define STATUS_HEIGHT   1
#define LOG_MIN_HEIGHT  5
#define LOG_MAX_LINES   1000
#define INPUT_MIN_HEIGHT 1

// ============================================================================
// 内部结构体
// ============================================================================

typedef struct {
  char** lines;
  int capacity;
  int count;
  int start;
  int scroll_offset;
} LogBuffer;

typedef struct {
  char* buffer;
  size_t capacity;
  size_t length;
  size_t cursor_pos;
  int active;
} InputBuffer;

struct DisplayContext {
  struct notcurses* nc;
  struct ncplane* stdplane;
  struct ncplane* status_plane;
  struct ncplane* menu_plane;
  struct ncplane* input_plane;
  struct ncplane* log_plane;

  char filename[256];
  FileStatus file_status;
  int selected_menu;
  int running;

  InputBuffer input;
  LogBuffer log;

  int stdout_pipe[2];
  int orig_stdout;
  pthread_t reader_thread;
  int pipe_active;

  MenuCallback on_menu_select;
};

static const char* MENU_LABELS[MENU_COUNT] = {
  "[1] 创建文件",
  "[2] 加载文件",
  "[3] 保存文件",
  "[4] 插入数据",
  "[5] 查询数据",
  "[6] 删除数据",
  "[7] 修改数据"
};

// ============================================================================
// 日志缓冲区
// ============================================================================

static void logbuffer_init(LogBuffer* lb, int capacity) {
  lb->lines = calloc(capacity, sizeof(char*));
  lb->capacity = capacity;
  lb->count = 0;
  lb->start = 0;
  lb->scroll_offset = 0;
}

static void logbuffer_add(LogBuffer* lb, const char* line) {
  int idx = (lb->start + lb->count) % lb->capacity;
  if (lb->count < lb->capacity) {
    lb->count++;
  } else {
    free(lb->lines[idx]);
    lb->start = (lb->start + 1) % lb->capacity;
  }
  lb->lines[idx] = strdup(line);
  lb->scroll_offset = 0;
}

static char* logbuffer_get(LogBuffer* lb, int offset_from_end) {
  if (offset_from_end >= lb->count || offset_from_end < 0) return NULL;
  int idx = (lb->start + lb->count - 1 - offset_from_end) % lb->capacity;
  return lb->lines[idx];
}

static void logbuffer_free(LogBuffer* lb) {
  for (int i = 0; i < lb->capacity; i++) {
    free(lb->lines[i]);
  }
  free(lb->lines);
}

// ============================================================================
// 输入缓冲区
// ============================================================================

static void inputbuffer_init(InputBuffer* ib) {
  ib->capacity = 1024;
  ib->buffer = malloc(ib->capacity);
  ib->buffer[0] = '\0';
  ib->length = 0;
  ib->cursor_pos = 0;
  ib->active = 0;
}

static void inputbuffer_free(InputBuffer* ib) {
  free(ib->buffer);
}

static void inputbuffer_insert(InputBuffer* ib, char ch) {
  if (ib->length + 1 >= ib->capacity) {
    ib->capacity *= 2;
    ib->buffer = realloc(ib->buffer, ib->capacity);
  }
  memmove(ib->buffer + ib->cursor_pos + 1,
          ib->buffer + ib->cursor_pos,
          ib->length - ib->cursor_pos + 1);
  ib->buffer[ib->cursor_pos] = ch;
  ib->cursor_pos++;
  ib->length++;
}

static void inputbuffer_delete(InputBuffer* ib) {
  if (ib->cursor_pos < ib->length) {
    memmove(ib->buffer + ib->cursor_pos,
            ib->buffer + ib->cursor_pos + 1,
            ib->length - ib->cursor_pos);
    ib->length--;
  }
}

static void inputbuffer_backspace(InputBuffer* ib) {
  if (ib->cursor_pos > 0) {
    ib->cursor_pos--;
    inputbuffer_delete(ib);
  }
}

static void inputbuffer_clear(InputBuffer* ib) {
  ib->buffer[0] = '\0';
  ib->length = 0;
  ib->cursor_pos = 0;
}

// ============================================================================
// 管道读取线程
// ============================================================================

static void* pipe_reader_thread(void* arg) {
  DisplayContext* ctx = (DisplayContext*)arg;
  char buf[4096];
  char line_buf[4096];
  int line_pos = 0;

  while (ctx->pipe_active) {
    ssize_t n = read(ctx->stdout_pipe[0], buf, sizeof(buf) - 1);
    if (n <= 0) {
      if (errno == EINTR) continue;
      break;
    }
    buf[n] = '\0';

    for (ssize_t i = 0; i < n; i++) {
      if (buf[i] == '\n' || line_pos >= (int)sizeof(line_buf) - 1) {
        line_buf[line_pos] = '\0';
        logbuffer_add(&ctx->log, line_buf);
        line_pos = 0;
      } else {
        line_buf[line_pos++] = buf[i];
      }
    }
  }

  if (line_pos > 0) {
    line_buf[line_pos] = '\0';
    logbuffer_add(&ctx->log, line_buf);
  }

  return NULL;
}

static int start_pipe_capture(DisplayContext* ctx) {
  if (pipe(ctx->stdout_pipe) < 0) return -1;

  int flags = fcntl(ctx->stdout_pipe[0], F_GETFL);
  fcntl(ctx->stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

  ctx->orig_stdout = dup(STDOUT_FILENO);
  dup2(ctx->stdout_pipe[1], STDOUT_FILENO);

  ctx->pipe_active = 1;
  pthread_create(&ctx->reader_thread, NULL, pipe_reader_thread, ctx);

  return 0;
}

static void stop_pipe_capture(DisplayContext* ctx) {
  ctx->pipe_active = 0;
  fflush(stdout);
  dup2(ctx->orig_stdout, STDOUT_FILENO);
  close(ctx->orig_stdout);
  close(ctx->stdout_pipe[1]);
  pthread_join(ctx->reader_thread, NULL);
  close(ctx->stdout_pipe[0]);
}

// ============================================================================
// 绘制函数
// ============================================================================

static const char* get_file_status_str(FileStatus status) {
  switch (status) {
    case FILE_STATUS_NONE:     return "未加载";
    case FILE_STATUS_CLEAN:    return "已保存";
    case FILE_STATUS_MODIFIED: return "已修改";
    default:                   return "未知";
  }
}

static uint32_t get_file_status_color(FileStatus status) {
  switch (status) {
    case FILE_STATUS_NONE:     return COLOR_MUTED;
    case FILE_STATUS_CLEAN:    return COLOR_SUCCESS;
    case FILE_STATUS_MODIFIED: return COLOR_WARNING;
    default:                   return COLOR_MUTED;
  }
}

static void draw_status_bar(DisplayContext* ctx) {
  struct ncplane* p = ctx->status_plane;
  ncplane_erase(p);

  int width = ncplane_dim_x(p);

  ncplane_set_fg_rgb(p, COLOR_ACCENT);
  ncplane_set_bg_rgb(p, COLOR_MENU_BG);
  ncplane_printf_yx(p, 0, 0, " 学生信息管理系统 ");

  ncplane_set_fg_rgb(p, COLOR_FG);
  ncplane_printf(p, "| 文件: ");
  ncplane_set_fg_rgb(p, COLOR_ACCENT);
  ncplane_printf(p, "%s", ctx->filename[0] ? ctx->filename : "(无)");

  const char* status_str = get_file_status_str(ctx->file_status);
  int status_len = (int)strlen(status_str) + 6;
  ncplane_cursor_move_yx(p, 0, width - status_len);
  ncplane_set_fg_rgb(p, get_file_status_color(ctx->file_status));
  ncplane_set_bg_rgb(p, COLOR_MENU_BG);
  ncplane_printf(p, "[ %s ]", status_str);
}

static void draw_menu(DisplayContext* ctx) {
  struct ncplane* p = ctx->menu_plane;
  ncplane_erase(p);

  int plane_height = ncplane_dim_y(p);
  int plane_width = ncplane_dim_x(p);
  int menu_cols = 2;
  int menu_rows = (MENU_COUNT + menu_cols - 1) / menu_cols;

  int menu_width = 18;
  int start_y = (plane_height - menu_rows) / 2;
  int start_x = (plane_width - menu_width * menu_cols) / 2;

  // 标题
  ncplane_set_fg_rgb(p, COLOR_MUTED);
  ncplane_set_bg_rgb(p, COLOR_BG);
  ncplane_cursor_move_yx(p, start_y - 2, start_x);
  ncplane_printf(p, "======= 操作菜单 =======");

  for (int i = 0; i < MENU_COUNT; i++) {
    int row = i / menu_cols;
    int col = i % menu_cols;
    int y = start_y + row;
    int x = start_x + col * menu_width;

    int is_selected = (i == ctx->selected_menu);

    if (is_selected) {
      ncplane_set_fg_rgb(p, COLOR_BG);
      ncplane_set_bg_rgb(p, COLOR_ACCENT);
      ncplane_set_styles(p, NCSTYLE_BOLD);
    } else {
      ncplane_set_fg_rgb(p, COLOR_FG);
      ncplane_set_bg_rgb(p, COLOR_MENU_BG);
      ncplane_set_styles(p, NCSTYLE_NONE);
    }

    ncplane_cursor_move_yx(p, y, x);
    ncplane_printf(p, " %s ", MENU_LABELS[i]);
  }

  ncplane_set_styles(p, NCSTYLE_NONE);

  ncplane_set_fg_rgb(p, COLOR_MUTED);
  ncplane_set_bg_rgb(p, COLOR_BG);
  ncplane_cursor_move_yx(p, start_y + menu_rows + 1, start_x);
  ncplane_printf(p, "方向键选择 Enter确认 Tab输入 Q退出");
}

static void draw_input(DisplayContext* ctx) {
  struct ncplane* p = ctx->input_plane;
  ncplane_erase(p);

  int width = ncplane_dim_x(p);

  ncplane_set_bg_rgb(p, COLOR_INPUT_BG);

  if (ctx->input.active) {
    ncplane_set_fg_rgb(p, COLOR_ACCENT);
    ncplane_cursor_move_yx(p, 0, 0);
    ncplane_printf(p, " > 输入: ");
  } else {
    ncplane_set_fg_rgb(p, COLOR_MUTED);
    ncplane_cursor_move_yx(p, 0, 0);
    ncplane_printf(p, "   输入: ");
  }

  int label_width = 10;
  int available_width = width - label_width;

  int display_start = 0;
  if ((int)ctx->input.cursor_pos > available_width - 1) {
    display_start = (int)ctx->input.cursor_pos - available_width + 1;
  }

  if (ctx->input.active) {
    ncplane_set_fg_rgb(p, COLOR_FG);
  } else {
    ncplane_set_fg_rgb(p, COLOR_MUTED);
  }

  for (int i = 0; i < available_width && display_start + i < (int)ctx->input.length; i++) {
    ncplane_putchar(p, ctx->input.buffer[display_start + i]);
  }

  // 光标
  if (ctx->input.active) {
    int cursor_x = label_width + (int)ctx->input.cursor_pos - display_start;
    ncplane_cursor_move_yx(p, 0, cursor_x);
    ncplane_set_fg_rgb(p, COLOR_BG);
    ncplane_set_bg_rgb(p, COLOR_ACCENT);
    if (ctx->input.cursor_pos < ctx->input.length) {
      ncplane_putchar(p, ctx->input.buffer[ctx->input.cursor_pos]);
    } else {
      ncplane_putchar(p, ' ');
    }
  }
}

static void draw_log(DisplayContext* ctx) {
  struct ncplane* p = ctx->log_plane;
  ncplane_erase(p);

  int height = ncplane_dim_y(p);
  int width = ncplane_dim_x(p);

  ncplane_set_fg_rgb(p, COLOR_MUTED);
  ncplane_set_bg_rgb(p, COLOR_BG);

  // 手动画边框
  ncplane_cursor_move_yx(p, 0, 0);
  ncplane_printf(p, "+");
  for (int i = 1; i < width - 1; i++) ncplane_putchar(p, '-');
  ncplane_putchar(p, '+');

  for (int i = 1; i < height - 1; i++) {
    ncplane_cursor_move_yx(p, i, 0);
    ncplane_putchar(p, '|');
    ncplane_cursor_move_yx(p, i, width - 1);
    ncplane_putchar(p, '|');
  }

  ncplane_cursor_move_yx(p, height - 1, 0);
  ncplane_printf(p, "+");
  for (int i = 1; i < width - 1; i++) ncplane_putchar(p, '-');
  ncplane_putchar(p, '+');

  // 标题
  ncplane_cursor_move_yx(p, 0, 2);
  ncplane_set_fg_rgb(p, COLOR_ACCENT);
  ncplane_printf(p, " 日志 ");

  // 日志内容
  ncplane_set_fg_rgb(p, COLOR_FG);
  ncplane_set_bg_rgb(p, COLOR_BG);

  int visible_lines = height - 2;
  int start_line = ctx->log.count - visible_lines - ctx->log.scroll_offset;
  if (start_line < 0) start_line = 0;

  for (int i = 0; i < visible_lines; i++) {
    int log_idx = start_line + i;
    if (log_idx < ctx->log.count) {
      char* line = logbuffer_get(&ctx->log, ctx->log.count - 1 - log_idx);
      if (line) {
        ncplane_cursor_move_yx(p, 1 + i, 1);
        int print_len = (int)strlen(line);
        if (print_len > width - 2) print_len = width - 2;
        ncplane_printf(p, "%.*s", print_len, line);
      }
    }
  }

  // 滚动指示
  if (ctx->log.scroll_offset > 0) {
    ncplane_cursor_move_yx(p, height - 1, width - 8);
    ncplane_set_fg_rgb(p, COLOR_WARNING);
    ncplane_printf(p, " +%d", ctx->log.scroll_offset);
  }
}

// ============================================================================
// 事件处理
// ============================================================================

static void handle_menu_action(DisplayContext* ctx) {
  if (ctx->on_menu_select) {
    stop_pipe_capture(ctx);
    ctx->on_menu_select(ctx, (MenuItem)ctx->selected_menu, ctx->input.buffer);
    fflush(stdout);
    start_pipe_capture(ctx);
  }
  inputbuffer_clear(&ctx->input);
}

static void handle_key(DisplayContext* ctx, struct ncinput* ni) {
  if (ctx->input.active) {
    switch (ni->id) {
      case NCKEY_TAB:
      case NCKEY_ESC:
        ctx->input.active = 0;
        break;
      case NCKEY_ENTER:
        if (ctx->input.length > 0) {
          handle_menu_action(ctx);
        }
        break;
      case NCKEY_BACKSPACE:
      case 0x7f:
        inputbuffer_backspace(&ctx->input);
        break;
      case NCKEY_LEFT:
        if (ctx->input.cursor_pos > 0) ctx->input.cursor_pos--;
        break;
      case NCKEY_RIGHT:
        if (ctx->input.cursor_pos < ctx->input.length) ctx->input.cursor_pos++;
        break;
      case NCKEY_HOME:
        ctx->input.cursor_pos = 0;
        break;
      case NCKEY_END:
        ctx->input.cursor_pos = ctx->input.length;
        break;
      default:
        if (ni->id >= 0x20 && ni->id < 0x7f) {
          inputbuffer_insert(&ctx->input, (char)ni->id);
        }
        break;
    }
    return;
  }

  switch (ni->id) {
    case NCKEY_UP:
      ctx->selected_menu = (ctx->selected_menu - 2 + MENU_COUNT) % MENU_COUNT;
      break;
    case NCKEY_DOWN:
      ctx->selected_menu = (ctx->selected_menu + 2) % MENU_COUNT;
      break;
    case NCKEY_LEFT:
      if (ctx->selected_menu > 0) ctx->selected_menu--;
      break;
    case NCKEY_RIGHT:
      if (ctx->selected_menu < MENU_COUNT - 1) ctx->selected_menu++;
      break;
    case NCKEY_ENTER:
      ctx->input.active = 1;
      break;
    case NCKEY_TAB:
      ctx->input.active = 1;
      break;
    case 'q':
    case 'Q':
      ctx->running = 0;
      break;
    case NCKEY_PGUP:
      if (ctx->log.scroll_offset < ctx->log.count - 1)
        ctx->log.scroll_offset++;
      break;
    case NCKEY_PGDOWN:
      if (ctx->log.scroll_offset > 0)
        ctx->log.scroll_offset--;
      break;
  }
}

// ============================================================================
// 公共接口
// ============================================================================

DisplayContext* display_init(void) {
  DisplayContext* ctx = calloc(1, sizeof(DisplayContext));
  if (!ctx) return NULL;

  struct notcurses_options nopts = {
    .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_ALTERNATE_SCREEN,
  };

  ctx->nc = notcurses_init(&nopts, NULL);
  if (!ctx->nc) {
    free(ctx);
    return NULL;
  }

  ctx->stdplane = notcurses_stdplane(ctx->nc);

  unsigned int total_rows, total_cols;
  notcurses_stddim_yx(ctx->nc, &total_rows, &total_cols);

  int log_height = LOG_MIN_HEIGHT + 3;
  int input_height = INPUT_MIN_HEIGHT + 1;
  int menu_height = (int)total_rows - STATUS_HEIGHT - log_height - input_height;

  struct ncplane_options opts = {0};
  opts.cols = total_cols;

  opts.rows = STATUS_HEIGHT;
  opts.y = 0;
  ctx->status_plane = ncplane_create(ctx->stdplane, &opts);

  opts.rows = menu_height;
  opts.y = STATUS_HEIGHT;
  ctx->menu_plane = ncplane_create(ctx->stdplane, &opts);

  opts.rows = input_height;
  opts.y = STATUS_HEIGHT + menu_height;
  ctx->input_plane = ncplane_create(ctx->stdplane, &opts);

  opts.rows = log_height;
  opts.y = STATUS_HEIGHT + menu_height + input_height;
  ctx->log_plane = ncplane_create(ctx->stdplane, &opts);

  logbuffer_init(&ctx->log, LOG_MAX_LINES);
  inputbuffer_init(&ctx->input);

  ctx->filename[0] = '\0';
  ctx->file_status = FILE_STATUS_NONE;
  ctx->selected_menu = 0;
  ctx->running = 1;

  start_pipe_capture(ctx);

  return ctx;
}

void display_cleanup(DisplayContext* ctx) {
  if (!ctx) return;

  stop_pipe_capture(ctx);
  logbuffer_free(&ctx->log);
  inputbuffer_free(&ctx->input);

  ncplane_destroy(ctx->status_plane);
  ncplane_destroy(ctx->menu_plane);
  ncplane_destroy(ctx->input_plane);
  ncplane_destroy(ctx->log_plane);

  notcurses_stop(ctx->nc);
  free(ctx);
}

int display_run(DisplayContext* ctx) {
  struct ncinput ni;

  while (ctx->running) {
    draw_status_bar(ctx);
    draw_menu(ctx);
    draw_input(ctx);
    draw_log(ctx);

    notcurses_render(ctx->nc);
    notcurses_get(ctx->nc, NULL, &ni);
    handle_key(ctx, &ni);
  }

  return 0;
}

void display_set_filename(DisplayContext* ctx, const char* filename) {
  strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
}

void display_set_file_status(DisplayContext* ctx, FileStatus status) {
  ctx->file_status = status;
}

const char* display_get_input(DisplayContext* ctx) {
  return ctx->input.buffer;
}

void display_refresh(DisplayContext* ctx) {
  draw_status_bar(ctx);
  draw_menu(ctx);
  draw_input(ctx);
  draw_log(ctx);
  notcurses_render(ctx->nc);
}

void display_set_callback(DisplayContext* ctx, MenuCallback callback) {
  ctx->on_menu_select = callback;
}

const char* display_get_filename(DisplayContext* ctx) {
  return ctx->filename;
}
