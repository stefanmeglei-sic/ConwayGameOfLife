#include "life_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static life_log_level_t g_level = LIFE_LOG_INFO;
static int g_rank = 0;
static char g_component[32] = "app";
static FILE *g_file = NULL;

static const char *level_name(life_log_level_t level) {
    switch (level) {
        case LIFE_LOG_ERROR:
            return "ERROR";
        case LIFE_LOG_WARN:
            return "WARN";
        case LIFE_LOG_INFO:
            return "INFO";
        case LIFE_LOG_DEBUG:
            return "DEBUG";
        default:
            return "INFO";
    }
}

static life_log_level_t parse_level(const char *value) {
    if (value == NULL) {
        return LIFE_LOG_INFO;
    }
    if (strcmp(value, "error") == 0 || strcmp(value, "ERROR") == 0) {
        return LIFE_LOG_ERROR;
    }
    if (strcmp(value, "warn") == 0 || strcmp(value, "WARN") == 0) {
        return LIFE_LOG_WARN;
    }
    if (strcmp(value, "info") == 0 || strcmp(value, "INFO") == 0) {
        return LIFE_LOG_INFO;
    }
    if (strcmp(value, "debug") == 0 || strcmp(value, "DEBUG") == 0) {
        return LIFE_LOG_DEBUG;
    }
    return LIFE_LOG_INFO;
}

void life_log_init(const char *component, int rank) {
    const char *level_env = getenv("LIFE_LOG_LEVEL");
    const char *file_env = getenv("LIFE_LOG_FILE");

    g_level = parse_level(level_env);
    g_rank = rank;

    if (component != NULL && component[0] != '\0') {
        strncpy(g_component, component, sizeof(g_component) - 1);
        g_component[sizeof(g_component) - 1] = '\0';
    }

    /* Optional per-rank log sink: <prefix>.<component>.rankN.log */
    if (file_env != NULL && file_env[0] != '\0') {
        char path[512];
        int written = snprintf(path, sizeof(path), "%s.%s.rank%d.log", file_env, g_component, g_rank);
        if (written > 0 && written < (int) sizeof(path)) {
            g_file = fopen(path, "a");
        }
    }
}

void life_log_shutdown(void) {
    if (g_file != NULL) {
        fclose(g_file);
        g_file = NULL;
    }
}

void life_log_message(life_log_level_t level, const char *source_file, int source_line, const char *format, ...) {
    char timestamp[32];
    time_t now;
    struct tm tm_now;
    va_list args;

    if (level > g_level) {
        return;
    }

    now = time(NULL);
    {
        struct tm *tm_ptr = localtime(&now);
        if (tm_ptr != NULL) {
            tm_now = *tm_ptr;
        } else {
            memset(&tm_now, 0, sizeof(tm_now));
        }
    }
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(stderr,
            "%s [%s] [%s] [rank=%d] %s:%d: ",
            timestamp,
            level_name(level),
            g_component,
            g_rank,
            source_file,
            source_line);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);

    /* Mirror stderr output to file when LIFE_LOG_FILE is configured. */
    if (g_file != NULL) {
        fprintf(g_file,
                "%s [%s] [%s] [rank=%d] %s:%d: ",
                timestamp,
                level_name(level),
                g_component,
                g_rank,
                source_file,
                source_line);
        va_start(args, format);
        vfprintf(g_file, format, args);
        va_end(args);
        fputc('\n', g_file);
        fflush(g_file);
    }
}