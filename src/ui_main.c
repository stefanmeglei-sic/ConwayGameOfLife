#include "life.h"
#include "life_ui.h"
#include "life_log.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    life_options_t options;
    const char *error_message = NULL;

    if (life_parse_options(argc, argv, &options, &error_message) < 0) {
        if (error_message) {
            fprintf(stderr, "Error: %s\n", error_message);
        }
        life_print_usage(argv[0]);
        return 1;
    }

    LIFE_LOG_INFO("Starting Conway's Game of Life UI (SDL2)");
    LIFE_LOG_INFO("Grid: %dx%d | Pattern: %d | Steps: %d | Seed: %u | Density: %.2f",
                   options.width, options.height, options.pattern, options.steps, 
                   options.seed, options.density);

    int ret = life_ui_sdl2_run(&options);
    if (ret < 0) {
        LIFE_LOG_ERROR("SDL2 UI failed");
        return 1;
    }

    LIFE_LOG_INFO("UI completed successfully");
    return 0;
}
