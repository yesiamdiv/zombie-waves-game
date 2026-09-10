#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Logger g_logger = {0};

static const char *level_strings[] = {
    "[TRACE]", "[DEBUG]", "[INFO] ", "[WARN] ", "[ERROR]", "[FATAL]"
};

static const char *level_colors[] = {
    "\033[36m",  /* cyan */
    "\033[35m",  /* magenta */
    "\033[32m",  /* green */
    "\033[33m",  /* yellow */
    "\033[31m",  /* red */
    "\033[37m\033[41m" /* white on red */
};

#define RESET_COLOR "\033[0m"

void log_init(const char *filename, LogLevel console_level, LogLevel file_level) {
    g_logger.console_level = console_level;
    g_logger.file_level = file_level;
    g_logger.show_timestamps = 1;

    if (filename) {
        g_logger.file = fopen(filename, "w");
        if (!g_logger.file) {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
        }
    }

    LOG_INFO("Logger initialized (console=%d, file=%d)", console_level, file_level);
}

void log_shutdown(void) {
    if (g_logger.file) {
        LOG_INFO("Logger shutting down");
        fclose(g_logger.file);
        g_logger.file = NULL;
    }
}

void log_set_level(LogLevel console_level, LogLevel file_level) {
    g_logger.console_level = console_level;
    g_logger.file_level = file_level;
}

void log_msg(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level < g_logger.console_level && (!g_logger.file || level < g_logger.file_level)) {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    const char *basename = strrchr(file, '/');
    basename = basename ? basename + 1 : file;

    if (level >= g_logger.console_level) {
        fprintf(stderr, "%s %s %s:%d: %s%s\n",
                timestamp, level_strings[level], basename, line,
                msg, RESET_COLOR);
        fflush(stderr);
    }

    if (g_logger.file && level >= g_logger.file_level) {
        fprintf(g_logger.file, "%s %s %s:%d: %s\n",
                timestamp, level_strings[level], basename, line, msg);
        fflush(g_logger.file);
    }
}
