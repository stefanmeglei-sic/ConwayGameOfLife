#include "life.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static size_t cell_index(int row, int column, int width) {
    return (size_t) row * (size_t) width + (size_t) column;
}

size_t life_grid_size(int width, int height) {
    return (size_t) width * (size_t) height;
}

uint8_t *life_allocate_grid(int width, int height) {
    size_t total_cells = life_grid_size(width, height);
    return (uint8_t *) calloc(total_cells, sizeof(uint8_t));
}

static int positive_mod(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

static life_pattern_t parse_pattern(const char *value, int *ok) {
    if (strcmp(value, "random") == 0) {
        *ok = 1;
        return LIFE_PATTERN_RANDOM;
    }
    if (strcmp(value, "glider") == 0) {
        *ok = 1;
        return LIFE_PATTERN_GLIDER;
    }
    if (strcmp(value, "blinker") == 0) {
        *ok = 1;
        return LIFE_PATTERN_BLINKER;
    }
    if (strcmp(value, "block") == 0) {
        *ok = 1;
        return LIFE_PATTERN_BLOCK;
    }
    if (strcmp(value, "acorn") == 0) {
        *ok = 1;
        return LIFE_PATTERN_ACORN;
    }

    *ok = 0;
    return LIFE_PATTERN_RANDOM;
}

void life_print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --width N\n"
            "  --height N\n"
            "  --steps N\n"
            "  --seed N\n"
            "  --density X\n"
            "  --pattern random|glider|blinker|block|acorn\n"
            "  --dump-final\n"
            "  --validate\n"
            "  --csv\n"
            "  --csv-header\n"
            "  --decomposition 1d|2d\n"
            "  --snapshot-every N\n"
            "  --snapshot-prefix NAME\n"
            "  --pgm-final FILE\n",
            program_name);
}

int life_parse_options(int argc, char **argv, life_options_t *options, const char **error_message) {
    int index;

    options->width = 64;
    options->height = 64;
    options->steps = 100;
    options->seed = 1u;
    options->density = 0.30;
    options->pattern = LIFE_PATTERN_RANDOM;
    options->dump_final = 0;
    options->validate = 0;
    options->csv = 0;
    options->csv_header = 0;
    options->snapshot_every = 0;
    strncpy(options->snapshot_prefix, "snapshot", sizeof(options->snapshot_prefix) - 1);
    options->snapshot_prefix[sizeof(options->snapshot_prefix) - 1] = '\0';
    options->pgm_final_path[0] = '\0';
    options->decomposition_2d = 0;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--width") == 0 && index + 1 < argc) {
            options->width = atoi(argv[++index]);
        } else if (strcmp(argv[index], "--height") == 0 && index + 1 < argc) {
            options->height = atoi(argv[++index]);
        } else if (strcmp(argv[index], "--steps") == 0 && index + 1 < argc) {
            options->steps = atoi(argv[++index]);
        } else if (strcmp(argv[index], "--seed") == 0 && index + 1 < argc) {
            options->seed = (unsigned int) strtoul(argv[++index], NULL, 10);
        } else if (strcmp(argv[index], "--density") == 0 && index + 1 < argc) {
            options->density = strtod(argv[++index], NULL);
        } else if (strcmp(argv[index], "--pattern") == 0 && index + 1 < argc) {
            int ok = 0;
            options->pattern = parse_pattern(argv[++index], &ok);
            if (!ok) {
                *error_message = "unknown pattern";
                return 0;
            }
        } else if (strcmp(argv[index], "--dump-final") == 0) {
            options->dump_final = 1;
        } else if (strcmp(argv[index], "--validate") == 0) {
            options->validate = 1;
        } else if (strcmp(argv[index], "--csv") == 0) {
            options->csv = 1;
        } else if (strcmp(argv[index], "--csv-header") == 0) {
            options->csv_header = 1;
        } else if (strcmp(argv[index], "--decomposition") == 0 && index + 1 < argc) {
            const char *mode = argv[++index];
            if (strcmp(mode, "1d") == 0) {
                options->decomposition_2d = 0;
            } else if (strcmp(mode, "2d") == 0) {
                options->decomposition_2d = 1;
            } else {
                *error_message = "decomposition must be 1d or 2d";
                return 0;
            }
        } else if (strcmp(argv[index], "--snapshot-every") == 0 && index + 1 < argc) {
            options->snapshot_every = atoi(argv[++index]);
        } else if (strcmp(argv[index], "--snapshot-prefix") == 0 && index + 1 < argc) {
            strncpy(options->snapshot_prefix, argv[++index], sizeof(options->snapshot_prefix) - 1);
            options->snapshot_prefix[sizeof(options->snapshot_prefix) - 1] = '\0';
        } else if (strcmp(argv[index], "--pgm-final") == 0 && index + 1 < argc) {
            strncpy(options->pgm_final_path, argv[++index], sizeof(options->pgm_final_path) - 1);
            options->pgm_final_path[sizeof(options->pgm_final_path) - 1] = '\0';
        } else {
            *error_message = "unknown or incomplete argument";
            return 0;
        }
    }

    if (options->width <= 0 || options->height <= 0 || options->steps < 0) {
        *error_message = "width, height and steps must be positive, with steps >= 0";
        return 0;
    }

    if (options->density < 0.0 || options->density > 1.0) {
        *error_message = "density must be in [0, 1]";
        return 0;
    }

    if (options->snapshot_every < 0) {
        *error_message = "snapshot-every must be >= 0";
        return 0;
    }

    return 1;
}

void life_copy_grid(uint8_t *dest, const uint8_t *src, int width, int height) {
    memcpy(dest, src, life_grid_size(width, height) * sizeof(uint8_t));
}

static void place_pattern_cell(uint8_t *grid, int width, int height, int row, int column) {
    int wrapped_row = positive_mod(row, height);
    int wrapped_column = positive_mod(column, width);
    grid[cell_index(wrapped_row, wrapped_column, width)] = 1u;
}

static void initialize_random(uint8_t *grid, int width, int height, unsigned int seed, double density) {
    size_t total_cells = life_grid_size(width, height);
    size_t index;

    srand(seed);
    for (index = 0; index < total_cells; ++index) {
        double value = (double) rand() / (double) RAND_MAX;
        grid[index] = value < density ? 1u : 0u;
    }
}

static void initialize_pattern(uint8_t *grid, int width, int height, life_pattern_t pattern) {
    int center_row = height / 2;
    int center_column = width / 2;

    switch (pattern) {
        case LIFE_PATTERN_GLIDER:
            place_pattern_cell(grid, width, height, center_row - 1, center_column);
            place_pattern_cell(grid, width, height, center_row, center_column + 1);
            place_pattern_cell(grid, width, height, center_row + 1, center_column - 1);
            place_pattern_cell(grid, width, height, center_row + 1, center_column);
            place_pattern_cell(grid, width, height, center_row + 1, center_column + 1);
            break;
        case LIFE_PATTERN_BLINKER:
            place_pattern_cell(grid, width, height, center_row, center_column - 1);
            place_pattern_cell(grid, width, height, center_row, center_column);
            place_pattern_cell(grid, width, height, center_row, center_column + 1);
            break;
        case LIFE_PATTERN_BLOCK:
            place_pattern_cell(grid, width, height, center_row, center_column);
            place_pattern_cell(grid, width, height, center_row, center_column + 1);
            place_pattern_cell(grid, width, height, center_row + 1, center_column);
            place_pattern_cell(grid, width, height, center_row + 1, center_column + 1);
            break;
        case LIFE_PATTERN_ACORN:
            place_pattern_cell(grid, width, height, center_row, center_column + 1);
            place_pattern_cell(grid, width, height, center_row + 1, center_column + 3);
            place_pattern_cell(grid, width, height, center_row + 2, center_column);
            place_pattern_cell(grid, width, height, center_row + 2, center_column + 1);
            place_pattern_cell(grid, width, height, center_row + 2, center_column + 4);
            place_pattern_cell(grid, width, height, center_row + 2, center_column + 5);
            place_pattern_cell(grid, width, height, center_row + 2, center_column + 6);
            break;
        case LIFE_PATTERN_RANDOM:
        default:
            break;
    }
}

void life_initialize_grid(uint8_t *grid, int width, int height, const life_options_t *options) {
    memset(grid, 0, life_grid_size(width, height) * sizeof(uint8_t));

    if (options->pattern == LIFE_PATTERN_RANDOM) {
        initialize_random(grid, width, height, options->seed, options->density);
        return;
    }

    initialize_pattern(grid, width, height, options->pattern);
}

static int count_neighbors(const uint8_t *grid, int width, int height, int row, int column) {
    int drow;
    int dcolumn;
    int alive_neighbors = 0;

    for (drow = -1; drow <= 1; ++drow) {
        for (dcolumn = -1; dcolumn <= 1; ++dcolumn) {
            int neighbor_row;
            int neighbor_column;

            if (drow == 0 && dcolumn == 0) {
                continue;
            }

            neighbor_row = positive_mod(row + drow, height);
            neighbor_column = positive_mod(column + dcolumn, width);
            alive_neighbors += grid[cell_index(neighbor_row, neighbor_column, width)] != 0;
        }
    }

    return alive_neighbors;
}

void life_step_serial(const uint8_t *current, uint8_t *next, int width, int height) {
    int row;

    for (row = 0; row < height; ++row) {
        int column;
        for (column = 0; column < width; ++column) {
            int neighbors = count_neighbors(current, width, height, row, column);
            uint8_t current_value = current[cell_index(row, column, width)];

            next[cell_index(row, column, width)] =
                (neighbors == 3 || (current_value && neighbors == 2)) ? 1u : 0u;
        }
    }
}

void life_run_serial(const uint8_t *initial, uint8_t *final_grid, int width, int height, int steps, life_timing_t *timing) {
    uint8_t *current = life_allocate_grid(width, height);
    uint8_t *next = life_allocate_grid(width, height);
    int step;
    clock_t start_clock;
    clock_t end_clock;

    if (current == NULL || next == NULL) {
        free(current);
        free(next);
        return;
    }

    life_copy_grid(current, initial, width, height);

    start_clock = clock();
    for (step = 0; step < steps; ++step) {
        uint8_t *swap_grid;
        life_step_serial(current, next, width, height);
        swap_grid = current;
        current = next;
        next = swap_grid;
    }
    end_clock = clock();

    life_copy_grid(final_grid, current, width, height);
    if (timing != NULL) {
        timing->total_seconds = (double) (end_clock - start_clock) / (double) CLOCKS_PER_SEC;
        timing->communication_seconds = 0.0;
        timing->computation_seconds = timing->total_seconds;
    }

    free(current);
    free(next);
}

void life_dump_grid_ascii(const uint8_t *grid, int width, int height) {
    int row;
    for (row = 0; row < height; ++row) {
        int column;
        for (column = 0; column < width; ++column) {
            putchar(grid[cell_index(row, column, width)] ? '#' : '.');
        }
        putchar('\n');
    }
}

int life_compare_grids(const uint8_t *left, const uint8_t *right, int width, int height) {
    return memcmp(left, right, life_grid_size(width, height) * sizeof(uint8_t)) == 0;
}

int life_write_pgm(const char *path, const uint8_t *grid, int width, int height) {
    FILE *out = fopen(path, "wb");
    size_t total_cells;
    size_t index;
    uint8_t *pixels;

    if (out == NULL) {
        return 0;
    }

    if (fprintf(out, "P5\n%d %d\n255\n", width, height) < 0) {
        fclose(out);
        return 0;
    }

    total_cells = life_grid_size(width, height);
    pixels = (uint8_t *) malloc(total_cells);
    if (pixels == NULL) {
        fclose(out);
        return 0;
    }

    for (index = 0; index < total_cells; ++index) {
        pixels[index] = grid[index] ? 255u : 0u;
    }

    if (fwrite(pixels, 1, total_cells, out) != total_cells) {
        free(pixels);
        fclose(out);
        return 0;
    }

    free(pixels);
    fclose(out);
    return 1;
}

int life_local_row_count(int global_height, int size, int rank) {
    int base = global_height / size;
    int remainder = global_height % size;
    return base + (rank < remainder ? 1 : 0);
}

int life_row_offset(int global_height, int size, int rank) {
    int base = global_height / size;
    int remainder = global_height % size;
    return rank * base + (rank < remainder ? rank : remainder);
}