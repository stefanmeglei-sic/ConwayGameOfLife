#ifndef LIFE_LOG_H
#define LIFE_LOG_H

typedef enum {
    LIFE_LOG_ERROR = 0,
    LIFE_LOG_WARN = 1,
    LIFE_LOG_INFO = 2,
    LIFE_LOG_DEBUG = 3
} life_log_level_t;

void life_log_init(const char *component, int rank);
void life_log_shutdown(void);
void life_log_message(life_log_level_t level, const char *source_file, int source_line, const char *format, ...);

#define LIFE_LOG_ERROR(...) life_log_message(LIFE_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LIFE_LOG_WARN(...) life_log_message(LIFE_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LIFE_LOG_INFO(...) life_log_message(LIFE_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LIFE_LOG_DEBUG(...) life_log_message(LIFE_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#endif