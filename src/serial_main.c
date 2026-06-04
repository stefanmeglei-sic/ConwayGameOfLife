#include "life.h"
#include "life_log.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    life_options_t options;
    const char *error_message = NULL;
    uint8_t *final_grid = NULL;
    life_timing_t timing = {0.0, 0.0, 0.0};

    life_log_init("serial", 0);
    LIFE_LOG_INFO("Serial runner started");

    if (!life_parse_options(argc, argv, &options, &error_message)) {
        if (error_message != NULL) {
            LIFE_LOG_ERROR("Argument error: %s", error_message);
        }
        life_print_usage(argv[0]);
        life_log_shutdown();
        return EXIT_FAILURE;
    }

    final_grid = life_allocate_grid(options.width, options.height);
    if (final_grid == NULL) {
        LIFE_LOG_ERROR("Failed to allocate serial grids (width=%d, height=%d)", options.width, options.height);
        free(final_grid);
        life_log_shutdown();
        return EXIT_FAILURE;
    }

    /* Serial CLI delegates simulation logic to the shared backend engine. */
    if (!life_run_with_options(&options, final_grid, &timing, NULL, NULL)) {
        LIFE_LOG_ERROR("Backend run failed (width=%d, height=%d, steps=%d)",
                       options.width,
                       options.height,
                       options.steps);
        free(final_grid);
        life_log_shutdown();
        return EXIT_FAILURE;
    }

    printf("serial: width=%d height=%d steps=%d total_seconds=%.6f\n",
           options.width,
           options.height,
           options.steps,
           timing.total_seconds);

    if (options.pgm_final_path[0] != '\0') {
        if (!life_write_pgm(options.pgm_final_path, final_grid, options.width, options.height)) {
            LIFE_LOG_ERROR("Failed to write PGM file: %s", options.pgm_final_path);
            free(final_grid);
            life_log_shutdown();
            return EXIT_FAILURE;
        }
        LIFE_LOG_INFO("Wrote final PGM snapshot to %s", options.pgm_final_path);
    }

    if (options.dump_final) {
        life_dump_grid_ascii(final_grid, options.width, options.height);
    }

    free(final_grid);
    LIFE_LOG_INFO("Serial runner finished successfully");
    life_log_shutdown();
    return EXIT_SUCCESS;
}