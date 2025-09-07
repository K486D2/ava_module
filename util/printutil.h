#ifndef PRINT_UTIL_H
#define PRINT_UTIL_H

#include <stdarg.h>
#include <stdio.h>

// ANSI颜色代码
#define COLOR_RESET   "\033[0m"
#define COLOR_BLACK   "\033[30m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

// 背景色代码
#define BG_BLACK   "\033[40m"
#define BG_RED     "\033[41m"
#define BG_GREEN   "\033[42m"
#define BG_YELLOW  "\033[43m"
#define BG_BLUE    "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN    "\033[46m"
#define BG_WHITE   "\033[47m"

// 样式代码
#define STYLE_BOLD      "\033[1m"
#define STYLE_DIM       "\033[2m"
#define STYLE_ITALIC    "\033[3m"
#define STYLE_UNDERLINE "\033[4m"
#define STYLE_BLINK     "\033[5m"
#define STYLE_REVERSE   "\033[7m"
#define STYLE_HIDDEN    "\033[8m"

// 检测是否支持颜色输出
static inline int supports_color(void) {
#ifdef _WIN32
  // Windows下默认不支持ANSI颜色，但可以通过设置支持
  return 0; // 暂时禁用Windows下的颜色
#else
  // Linux/Unix下通常支持
  return 1;
#endif
}

// 基础颜色打印函数
static inline void cprintf(const char *color, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  if (supports_color()) {
    printf("%s", color);
    vprintf(fmt, args);
    printf("%s", COLOR_RESET);
  } else {
    vprintf(fmt, args);
  }

  va_end(args);
}

// 便捷的颜色打印函数
static inline void print_red(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_RED, fmt, args);
  va_end(args);
}

static inline void print_green(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_GREEN, fmt, args);
  va_end(args);
}

static inline void print_yellow(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_YELLOW, fmt, args);
  va_end(args);
}

static inline void print_blue(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_BLUE, fmt, args);
  va_end(args);
}

static inline void print_magenta(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_MAGENTA, fmt, args);
  va_end(args);
}

static inline void print_cyan(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_CYAN, fmt, args);
  va_end(args);
}

static inline void print_bold(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(STYLE_BOLD, fmt, args);
  va_end(args);
}

// 日志级别的颜色打印
static inline void print_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_BLUE, "[INFO] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_success(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_GREEN, "[SUCCESS] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_warning(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_YELLOW, "[WARNING] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_RED, "[ERROR] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_CYAN, "[DEBUG] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

// 带时间戳的日志打印
static inline void print_info_ts(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_BLUE, "[INFO] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_error_ts(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(COLOR_RED, "[ERROR] ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

// 进度条打印
static inline void print_progress(int percent, const char *label) {
  int bar_width = 50;
  int pos       = bar_width * percent / 100;

  printf("\r%s[", label);
  for (int i = 0; i < bar_width; ++i) {
    if (i < pos) {
      cprintf(COLOR_GREEN, "=");
    } else if (i == pos) {
      cprintf(COLOR_YELLOW, ">");
    } else {
      printf(" ");
    }
  }
  printf("] %d%%", percent);
  fflush(stdout);
}

// 表格打印辅助函数
static inline void print_table_header(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cprintf(STYLE_BOLD COLOR_BLUE, fmt, args);
  printf("\n");
  va_end(args);
}

static inline void print_table_row(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf("  ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

#endif // !PRINT_UTIL_H