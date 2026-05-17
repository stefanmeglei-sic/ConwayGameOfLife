#include "life.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    life_options_t options;
    const char *error_message = NULL;
    uint8_t *initial = NULL;
    uint8_t *final_grid = NULL;
    life_timing_t timing = {0.0, 0.0, 0.0};

    if (!life_parse_options(argc, argv, &options, &error_message)) {
        if (error_message != NULL) {
            fprintf(stderr, "Argument error: %s\n", error_message);
        }
        life_print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    initial = life_allocate_grid(options.width, options.height);
    final_grid = life_allocate_grid(options.width, options.height);
    if (initial == NULL || final_grid == NULL) {
        fprintf(stderr, "Failed to allocate serial grids\n");
        free(initial);
        free(final_grid);
        return EXIT_FAILURE;
    }

    life_initialize_grid(initial, options.width, options.height, &options);
    life_run_serial(initial, final_grid, options.width, options.height, options.steps, &timing);

    printf("serial: width=%d height=%d steps=%d total_seconds=%.6f\n",
           options.width,
           options.height,
           options.steps,
           timing.total_seconds);

    if (options.pgm_final_path[0] != '\0') {
        if (!life_write_pgm(options.pgm_final_path, final_grid, options.width, options.height)) {
            fprintf(stderr, "Failed to write PGM file: %s\n", options.pgm_final_path);
            free(initial);
            free(final_grid);
            return EXIT_FAILURE;
        }
    }

    if (options.dump_final) {
        life_dump_grid_ascii(final_grid, options.width, options.height);
    }

    free(initial);
    free(final_grid);
    return EXIT_SUCCESS;
}