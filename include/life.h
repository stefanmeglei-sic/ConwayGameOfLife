#ifndef LIFE_H
#define LIFE_H

#include <stddef.h>
#include <stdint.h>

#define LIFE_PATH_MAX 256

typedef enum {
    LIFE_PATTERN_RANDOM = 0,
    LIFE_PATTERN_GLIDER,
    LIFE_PATTERN_BLINKER,
    LIFE_PATTERN_BLOCK,
    LIFE_PATTERN_ACORN
} life_pattern_t;

typedef struct {
    int width;
    int height;
    int steps;
    unsigned int seed;
    double density;
    life_pattern_t pattern;
    int dump_final;
    int validate;
    int csv;
    int csv_header;
    int snapshot_every;
    char snapshot_prefix[LIFE_PATH_MAX];
    char pgm_final_path[LIFE_PATH_MAX];
    int decomposition_2d;
} life_options_t;

typedef struct {
    double total_seconds;
    double communication_seconds;
    double computation_seconds;
} life_timing_t;

typedef struct {
    int width;
    int height;
    int generation;
    uint8_t *current;
    uint8_t *next;
} life_engine_t;

typedef int (*life_generation_callback_t)(const life_engine_t *engine, void *user_data);

int life_parse_options(int argc, char **argv, life_options_t *options, const char **error_message);
void life_print_usage(const char *program_name);

size_t life_grid_size(int width, int height);
uint8_t *life_allocate_grid(int width, int height);
void life_initialize_grid(uint8_t *grid, int width, int height, const life_options_t *options);
void life_copy_grid(uint8_t *dest, const uint8_t *src, int width, int height);
void life_step_serial(const uint8_t *current, uint8_t *next, int width, int height);
void life_run_serial(const uint8_t *initial, uint8_t *final_grid, int width, int height, int steps, life_timing_t *timing);
void life_dump_grid_ascii(const uint8_t *grid, int width, int height);
int life_compare_grids(const uint8_t *left, const uint8_t *right, int width, int height);
int life_write_pgm(const char *path, const uint8_t *grid, int width, int height);

int life_engine_init(life_engine_t *engine, int width, int height, const life_options_t *options);
int life_engine_init_from_grid(life_engine_t *engine, int width, int height, const uint8_t *initial_grid);
void life_engine_destroy(life_engine_t *engine);
int life_engine_step(life_engine_t *engine);
int life_engine_run(life_engine_t *engine, int steps, life_generation_callback_t callback, void *user_data);
const uint8_t *life_engine_current_grid(const life_engine_t *engine);
int life_engine_generation(const life_engine_t *engine);
void life_engine_copy_current(const life_engine_t *engine, uint8_t *dest);
int life_run_with_options(const life_options_t *options,
                          uint8_t *final_grid,
                          life_timing_t *timing,
                          life_generation_callback_t callback,
                          void *user_data);

int life_local_row_count(int global_height, int size, int rank);
int life_row_offset(int global_height, int size, int rank);

#endif