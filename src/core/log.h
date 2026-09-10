#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_LEVEL_COUNT
} LogLevel;

typedef struct {
    FILE *file;
    LogLevel console_level;
    LogLevel file_level;
    int show_timestamps;
} Logger;

void log_init(const char *filename, LogLevel console_level, LogLevel file_level);
void log_shutdown(void);
void log_set_level(LogLevel console_level, LogLevel file_level);

void log_msg(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOG_TRACE(fmt, ...) log_msg(LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_msg(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_msg(LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_msg(LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_msg(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) log_msg(LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define ASSERT(expr)                                                    \
    do {                                                                \
        if (!(expr)) {                                                  \
            LOG_FATAL("ASSERTION FAILED: %s at %s:%d", #expr, __FILE__, __LINE__); \
            abort();                                                    \
        }                                                               \
    } while(0)

#define ASSERT_MSG(expr, fmt, ...)                                      \
    do {                                                                \
        if (!(expr)) {                                                  \
            LOG_FATAL("ASSERTION FAILED: %s at %s:%d - " fmt,          \
                      #expr, __FILE__, __LINE__, ##__VA_ARGS__);        \
            abort();                                                    \
        }                                                               \
    } while(0)

#endif
