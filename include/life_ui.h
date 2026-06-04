#ifndef LIFE_UI_H
#define LIFE_UI_H

#include "life.h"

/* Front-end state container used by interactive UI loops. */
typedef struct {
    int window_width;
    int window_height;
    int cell_size;          /* pixels per cell */
    int target_fps;         /* frames per second */
    int paused;             /* 1 if paused, 0 if running */
    int running;            /* 1 if should continue, 0 if quit */
    double speed_multiplier; /* 1.0 = real-time, 0.5 = half speed, 2.0 = double */
    int pan_x;              /* camera offset in cells */
    int pan_y;
} life_ui_state_t;

/* SDL2 graphical UI main loop.
   Returns 0 on success, -1 on error (SDL2 unavailable, init failure, etc.).
*/
int life_ui_sdl2_run(const life_options_t *options);

/* Terminal/ncurses UI entry point (reserved for future implementation).
   Returns 0 on success, -1 on error.
*/
int life_ui_terminal_run(const life_options_t *options);

#endif
