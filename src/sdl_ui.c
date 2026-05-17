#include "life_ui.h"
#include "life_log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>

typedef enum {
    MODE_MENU,
    MODE_RUNNING,
    MODE_PAUSED,
    MODE_QUIT
} ui_mode_t;

typedef struct {
    life_engine_t *engine;
    life_ui_state_t *ui_state;
    SDL_Window *window;
    SDL_Renderer *renderer;
    int generation;
    int population;
    int total_steps;
    double last_frame_time;
    ui_mode_t mode;
    unsigned int seed;
    int width;
    int height;
    life_pattern_t pattern;
    int menu_selection;  /* 0=pattern, 1=width, 2=height, 3=seed, 4=start */
} sdl_context_t;

/* Count live cells in current grid */
static int count_population(const uint8_t *grid, int width, int height) {
    int count = 0;
    for (int i = 0; i < width * height; i++) {
        if (grid[i]) count++;
    }
    return count;
}

static const char *pattern_name(life_pattern_t pattern) {
    switch (pattern) {
    case LIFE_PATTERN_RANDOM:
        return "RANDOM";
    case LIFE_PATTERN_GLIDER:
        return "GLIDER";
    case LIFE_PATTERN_BLINKER:
        return "BLINKER";
    case LIFE_PATTERN_BLOCK:
        return "BLOCK";
    case LIFE_PATTERN_ACORN:
        return "ACORN";
    default:
        return "UNKNOWN";
    }
}

static const char *menu_field_name(int menu_selection) {
    switch (menu_selection) {
    case 0:
        return "WIDTH";
    case 1:
        return "HEIGHT";
    case 2:
        return "PATTERN";
    case 3:
        return "SEED";
    default:
        return "UNKNOWN";
    }
}

static void clamp_cell_size(life_ui_state_t *ui_state) {
    if (ui_state->cell_size < 2) {
        ui_state->cell_size = 2;
    } else if (ui_state->cell_size > 64) {
        ui_state->cell_size = 64;
    }
}

static int wrap_index(int value, int limit) {
    if (limit <= 0) {
        return 0;
    }

    value %= limit;
    if (value < 0) {
        value += limit;
    }

    return value;
}

static void zoom_at(sdl_context_t *ctx, int mouse_x, int mouse_y, int direction) {
    int old_cell_size = ctx->ui_state->cell_size;
    int new_cell_size = old_cell_size + (direction * 2);

    ctx->ui_state->cell_size = new_cell_size;
    clamp_cell_size(ctx->ui_state);
    new_cell_size = ctx->ui_state->cell_size;

    if (new_cell_size == old_cell_size) {
        return;
    }

    int world_x = ctx->ui_state->pan_x + mouse_x;
    int world_y = ctx->ui_state->pan_y + mouse_y;
    int cell_x = world_x / old_cell_size;
    int cell_y = world_y / old_cell_size;

    ctx->ui_state->pan_x = cell_x * new_cell_size - mouse_x;
    ctx->ui_state->pan_y = cell_y * new_cell_size - mouse_y;
}

enum {
    HUD_PANEL_HEIGHT = 142,
    HUD_TEXT_SCALE = 2,
    HUD_CHAR_W = 6,
    HUD_CHAR_H = 8,
    HUD_GAP = 1
};

#define GLYPH(a, b, c, d, e, f, g) { (uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d), (uint8_t)(e), (uint8_t)(f), (uint8_t)(g) }

static void glyph_rows(char ch, uint8_t rows[7]) {
    switch (ch) {
    case 'A': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11), 7); break;
    case 'B': memcpy(rows, (uint8_t[7])GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E), 7); break;
    case 'C': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E), 7); break;
    case 'D': memcpy(rows, (uint8_t[7])GLYPH(0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E), 7); break;
    case 'E': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F), 7); break;
    case 'F': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10), 7); break;
    case 'G': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E), 7); break;
    case 'H': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11), 7); break;
    case 'I': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F), 7); break;
    case 'J': memcpy(rows, (uint8_t[7])GLYPH(0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C), 7); break;
    case 'K': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11), 7); break;
    case 'L': memcpy(rows, (uint8_t[7])GLYPH(0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F), 7); break;
    case 'M': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11), 7); break;
    case 'N': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11), 7); break;
    case 'O': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E), 7); break;
    case 'P': memcpy(rows, (uint8_t[7])GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10), 7); break;
    case 'Q': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D), 7); break;
    case 'R': memcpy(rows, (uint8_t[7])GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11), 7); break;
    case 'S': memcpy(rows, (uint8_t[7])GLYPH(0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E), 7); break;
    case 'T': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04), 7); break;
    case 'U': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E), 7); break;
    case 'V': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04), 7); break;
    case 'W': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11), 7); break;
    case 'X': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11), 7); break;
    case 'Y': memcpy(rows, (uint8_t[7])GLYPH(0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04), 7); break;
    case 'Z': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F), 7); break;
    case '0': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E), 7); break;
    case '1': memcpy(rows, (uint8_t[7])GLYPH(0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E), 7); break;
    case '2': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F), 7); break;
    case '3': memcpy(rows, (uint8_t[7])GLYPH(0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E), 7); break;
    case '4': memcpy(rows, (uint8_t[7])GLYPH(0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02), 7); break;
    case '5': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E), 7); break;
    case '6': memcpy(rows, (uint8_t[7])GLYPH(0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E), 7); break;
    case '7': memcpy(rows, (uint8_t[7])GLYPH(0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08), 7); break;
    case '8': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E), 7); break;
    case '9': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C), 7); break;
    case ':': memcpy(rows, (uint8_t[7])GLYPH(0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00), 7); break;
    case '.': memcpy(rows, (uint8_t[7])GLYPH(0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C), 7); break;
    case ',': memcpy(rows, (uint8_t[7])GLYPH(0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08), 7); break;
    case '-': memcpy(rows, (uint8_t[7])GLYPH(0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00), 7); break;
    case '+': memcpy(rows, (uint8_t[7])GLYPH(0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00), 7); break;
    case '/': memcpy(rows, (uint8_t[7])GLYPH(0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00), 7); break;
    case '(': memcpy(rows, (uint8_t[7])GLYPH(0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02), 7); break;
    case ')': memcpy(rows, (uint8_t[7])GLYPH(0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08), 7); break;
    case '[': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E), 7); break;
    case ']': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E), 7); break;
    case '|': memcpy(rows, (uint8_t[7])GLYPH(0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04), 7); break;
    case '!': memcpy(rows, (uint8_t[7])GLYPH(0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04), 7); break;
    case '?': memcpy(rows, (uint8_t[7])GLYPH(0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04), 7); break;
    case ' ': memset(rows, 0, 7); break;
    default: memset(rows, 0, 7); break;
    }
}

static void draw_char(SDL_Renderer *renderer, int x, int y, int scale, char ch) {
    uint8_t rows[7];
    glyph_rows((char)toupper((unsigned char)ch), rows);
    SDL_Rect pixel = {0, 0, scale, scale};

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (rows[row] & (1u << (4 - col))) {
                pixel.x = x + col * scale;
                pixel.y = y + row * scale;
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, const char *text) {
    int cursor_x = x;
    int cursor_y = y;
    for (const char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '\n') {
            cursor_x = x;
            cursor_y += (HUD_CHAR_H + HUD_GAP) * scale;
            continue;
        }
        draw_char(renderer, cursor_x, cursor_y, scale, *cursor);
        cursor_x += HUD_CHAR_W * scale;
    }
}

static void sync_stats_from_engine(sdl_context_t *ctx) {
    if (!ctx->engine) {
        ctx->population = 0;
        return;
    }

    ctx->generation = life_engine_generation(ctx->engine);
    ctx->population = count_population(life_engine_current_grid(ctx->engine),
                                        ctx->engine->width, ctx->engine->height);
}

static void update_title(SDL_Window *window, sdl_context_t *ctx) {
    static char title[512];
    const char *mode_str = (ctx->mode == MODE_RUNNING) ? "RUNNING" :
                           (ctx->mode == MODE_PAUSED) ? "PAUSED" :
                           (ctx->mode == MODE_MENU) ? "MENU" : "QUIT";

    if (ctx->mode == MODE_MENU) {
        snprintf(title, sizeof(title),
                 "Conway's Life | MENU | %dx%d | %s | seed=%u",
                 ctx->width, ctx->height, pattern_name(ctx->pattern), ctx->seed);
    } else {
        snprintf(title, sizeof(title),
                 "Conway's Life | %s | gen=%d | pop=%d | %dx%d",
                 mode_str, ctx->generation, ctx->population, ctx->width, ctx->height);
    }

    SDL_SetWindowTitle(window, title);
}

static void draw_hud_panel(sdl_context_t *ctx) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx->renderer, 10, 10, 16, 210);

    SDL_Rect panel = {
        0,
        ctx->ui_state->window_height - HUD_PANEL_HEIGHT,
        ctx->ui_state->window_width,
        HUD_PANEL_HEIGHT
    };
    SDL_RenderFillRect(ctx->renderer, &panel);

    SDL_SetRenderDrawColor(ctx->renderer, 65, 65, 85, 255);
    SDL_RenderDrawLine(ctx->renderer, 0, panel.y, panel.w, panel.y);

    SDL_SetRenderDrawColor(ctx->renderer, 230, 230, 240, 255);

    if (ctx->mode == MODE_MENU) {
        draw_text(ctx->renderer, 16, panel.y + 12, HUD_TEXT_SCALE, "MENU - CONFIGURE AND START");

        char line[256];
        snprintf(line, sizeof(line), "EDITING: %s", menu_field_name(ctx->menu_selection));
        draw_text(ctx->renderer, 16, panel.y + 28, HUD_TEXT_SCALE, line);

        SDL_Rect highlight = {
            12,
            panel.y + 48 + (ctx->menu_selection * 18),
            ctx->ui_state->window_width - 24,
            16
        };
        SDL_SetRenderDrawColor(ctx->renderer, 40, 90, 120, 180);
        SDL_RenderFillRect(ctx->renderer, &highlight);

        SDL_SetRenderDrawColor(ctx->renderer, 230, 230, 240, 255);

        snprintf(line, sizeof(line), "%c WIDTH    %d   (LEFT/RIGHT)", ctx->menu_selection == 0 ? '>' : ' ', ctx->width);
        draw_text(ctx->renderer, 16, panel.y + 48, HUD_TEXT_SCALE, line);

        snprintf(line, sizeof(line), "%c HEIGHT   %d   (LEFT/RIGHT)", ctx->menu_selection == 1 ? '>' : ' ', ctx->height);
        draw_text(ctx->renderer, 16, panel.y + 66, HUD_TEXT_SCALE, line);

        snprintf(line, sizeof(line), "%c PATTERN  %s   (LEFT/RIGHT)", ctx->menu_selection == 2 ? '>' : ' ', pattern_name(ctx->pattern));
        draw_text(ctx->renderer, 16, panel.y + 84, HUD_TEXT_SCALE, line);

        snprintf(line, sizeof(line), "%c SEED     %u   (LEFT/RIGHT)", ctx->menu_selection == 3 ? '>' : ' ', ctx->seed);
        draw_text(ctx->renderer, 16, panel.y + 102, HUD_TEXT_SCALE, line);

        draw_text(ctx->renderer, 16, panel.y + 122, HUD_TEXT_SCALE, "UP/DOWN MOVE  SPACE START  R RANDOM  Q QUIT");
    } else {
        char line[256];
        snprintf(line, sizeof(line), "GEN %d   POP %d", ctx->generation, ctx->population);
        draw_text(ctx->renderer, 16, panel.y + 12, HUD_TEXT_SCALE, line);

        draw_text(ctx->renderer, 16, panel.y + 34, HUD_TEXT_SCALE, "SPACE RUN/PAUSE  N STEP  ENTER/P RUN  M MENU  Q QUIT");
        draw_text(ctx->renderer, 16, panel.y + 54, HUD_TEXT_SCALE, "ARROWS PAN  I/O ZOOM");

        if (ctx->mode == MODE_PAUSED) {
            draw_text(ctx->renderer, 16, panel.y + 78, HUD_TEXT_SCALE, "PAUSED - SPACE RESUMES, N STEPS ONCE");
        } else {
            draw_text(ctx->renderer, 16, panel.y + 78, HUD_TEXT_SCALE, "RUNNING - PRESS SPACE TO PAUSE");
        }
    }
}

/* Render the grid on screen */
static void render_grid(sdl_context_t *ctx) {
    SDL_SetRenderDrawColor(ctx->renderer, 10, 10, 10, 255);
    SDL_RenderClear(ctx->renderer);

    if (ctx->engine == NULL) {
        /* Not initialized yet */
        update_title(ctx->window, ctx);
        SDL_RenderPresent(ctx->renderer);
        return;
    }

    sync_stats_from_engine(ctx);

    const uint8_t *grid = life_engine_current_grid(ctx->engine);
    int width = ctx->engine->width;
    int height = ctx->engine->height;
    int cell_size = ctx->ui_state->cell_size;
    int pan_x = ctx->ui_state->pan_x;
    int pan_y = ctx->ui_state->pan_y;
    int win_w = ctx->ui_state->window_width;
    int win_h = ctx->ui_state->window_height;

    /* Determine visible cell range */
    int start_col = pan_x / cell_size;
    int start_row = pan_y / cell_size;
    int visible_cols = (win_w + cell_size - 1) / cell_size;
    int visible_rows = (win_h - HUD_PANEL_HEIGHT + cell_size - 1) / cell_size; /* leave room for HUD */

    SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, 255);
    for (int r = 0; r < visible_rows; r++) {
        int world_row = wrap_index(start_row + r, height);
        int cell_y = (r * cell_size) + (pan_y % cell_size);

        for (int c = 0; c < visible_cols; c++) {
            int world_col = wrap_index(start_col + c, width);
            int cell_x = (c * cell_size) + (pan_x % cell_size);

            if (grid[world_row * width + world_col]) {
                SDL_Rect rect = {
                    cell_x,
                    cell_y,
                    cell_size,
                    cell_size
                };
                SDL_RenderFillRect(ctx->renderer, &rect);
            }
        }
    }

    /* Draw grid lines (light) */
    SDL_SetRenderDrawColor(ctx->renderer, 50, 50, 50, 255);
    for (int r = 0; r <= visible_rows; r++) {
        int y = (r * cell_size) + (pan_y % cell_size);
        SDL_RenderDrawLine(ctx->renderer, 0, y, win_w, y);
    }
    for (int c = 0; c <= visible_cols; c++) {
        int x = (c * cell_size) + (pan_x % cell_size);
        SDL_RenderDrawLine(ctx->renderer, x, 0, x, win_h - HUD_PANEL_HEIGHT);
    }

    update_title(ctx->window, ctx);
    draw_hud_panel(ctx);
    SDL_RenderPresent(ctx->renderer);
}


/* Render menu */
static void render_menu(sdl_context_t *ctx) {
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 40, 255);
    SDL_RenderClear(ctx->renderer);

    update_title(ctx->window, ctx);
    draw_hud_panel(ctx);
    SDL_RenderPresent(ctx->renderer);
}

/* Handle menu input */
static void handle_menu_input(sdl_context_t *ctx, const SDL_Event *event) {
    if (event->type != SDL_KEYDOWN) return;

    int num_patterns = 5;

    switch (event->key.keysym.sym) {
    case SDLK_q:
    case SDLK_ESCAPE:
        ctx->mode = MODE_QUIT;
        break;

    case SDLK_SPACE:
        /* Start simulation */
        ctx->mode = MODE_RUNNING;
        break;

    case SDLK_r:
        /* Generate random seed */
        ctx->seed = (unsigned int)time(NULL);
        LIFE_LOG_INFO("New random seed: %u", ctx->seed);
        break;

    case SDLK_RIGHT:
        if (ctx->menu_selection == 0) {  /* width */
            ctx->width += 64;
        } else if (ctx->menu_selection == 1) {  /* height */
            ctx->height += 64;
        } else if (ctx->menu_selection == 2) {  /* pattern */
            ctx->pattern = (life_pattern_t)((ctx->pattern + 1) % num_patterns);
        } else if (ctx->menu_selection == 3) {  /* seed */
            ctx->seed += 1;
        }
        break;

    case SDLK_LEFT:
        if (ctx->menu_selection == 0) {  /* width */
            ctx->width = (ctx->width > 64) ? ctx->width - 64 : 64;
        } else if (ctx->menu_selection == 1) {  /* height */
            ctx->height = (ctx->height > 64) ? ctx->height - 64 : 64;
        } else if (ctx->menu_selection == 2) {  /* pattern */
            ctx->pattern = (ctx->pattern == 0) ? (life_pattern_t)(num_patterns - 1) : (life_pattern_t)(ctx->pattern - 1);
        } else if (ctx->menu_selection == 3) {  /* seed */
            ctx->seed = (ctx->seed > 0) ? ctx->seed - 1 : 0;
        }
        break;

    case SDLK_UP:
        ctx->menu_selection = (ctx->menu_selection == 0) ? 3 : ctx->menu_selection - 1;
        break;

    case SDLK_DOWN:
        ctx->menu_selection = (ctx->menu_selection >= 3) ? 0 : ctx->menu_selection + 1;
        break;

    default:
        break;
    }
}

static void handle_window_event(sdl_context_t *ctx, const SDL_Event *event) {
    if (event->type != SDL_WINDOWEVENT) {
        return;
    }

    if (event->window.event == SDL_WINDOWEVENT_RESIZED || event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        ctx->ui_state->window_width = event->window.data1;
        ctx->ui_state->window_height = event->window.data2;
    }
}

static void handle_pan_zoom_keys(sdl_context_t *ctx, const SDL_Event *event) {
    if (event->type != SDL_KEYDOWN) {
        return;
    }

    switch (event->key.keysym.sym) {
    case SDLK_i:
        zoom_at(ctx, ctx->ui_state->window_width / 2, ctx->ui_state->window_height / 2, 1);
        break;
    case SDLK_o:
        zoom_at(ctx, ctx->ui_state->window_width / 2, ctx->ui_state->window_height / 2, -1);
        break;
    case SDLK_UP:
        ctx->ui_state->pan_y -= 24;
        break;
    case SDLK_DOWN:
        ctx->ui_state->pan_y += 24;
        break;
    case SDLK_LEFT:
        ctx->ui_state->pan_x -= 24;
        break;
    case SDLK_RIGHT:
        ctx->ui_state->pan_x += 24;
        break;
    default:
        break;
    }
}

int life_ui_sdl2_run(const life_options_t *options) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LIFE_LOG_ERROR("SDL2 initialization failed: %s", SDL_GetError());
        return -1;
    }

    /* Create UI state */
    life_ui_state_t ui_state = {
        .window_width = 1024,
        .window_height = 768,
        .cell_size = 4,
        .target_fps = 60,
        .paused = 0,
        .running = 1,
        .speed_multiplier = 1.0,
        .pan_x = 0,
        .pan_y = 0
    };

    /* Create window */
    SDL_Window *window = SDL_CreateWindow(
        "Conway's Game of Life",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ui_state.window_width,
        ui_state.window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        LIFE_LOG_ERROR("SDL2 window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    /* Create renderer */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        LIFE_LOG_ERROR("SDL2 renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    LIFE_LOG_INFO("SDL2 UI initialized: %dx%d window, cell_size=%d", 
                   ui_state.window_width, ui_state.window_height, ui_state.cell_size);

    /* Create context */
    sdl_context_t ctx = {
        .window = window,
        .renderer = renderer,
        .ui_state = &ui_state,
        .generation = 0,
        .population = 0,
        .last_frame_time = 0,
        .mode = MODE_MENU,
        .engine = NULL,
        .width = options->width,
        .height = options->height,
        .pattern = options->pattern,
        .seed = options->seed ? options->seed : (unsigned int)time(NULL),
        .total_steps = options->steps,
        .menu_selection = 0
    };

    uint8_t *final_grid = life_allocate_grid(options->width, options->height);
    if (!final_grid) {
        LIFE_LOG_ERROR("Failed to allocate final grid");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    LIFE_LOG_INFO("UI started in MENU mode");

    /* Main event loop */
    int quit = 0;
    while (!quit && ctx.ui_state->running && ctx.mode != MODE_QUIT) {
        SDL_Event event;
        
        if (ctx.mode == MODE_MENU) {
            /* Menu mode: wait for user input */
            if (SDL_WaitEventTimeout(&event, 100)) {
                handle_window_event(&ctx, &event);
                handle_menu_input(&ctx, &event);
            }
            render_menu(&ctx);
        } else if (ctx.mode == MODE_RUNNING) {
            /* Running mode: initialize engine and run */
            if (ctx.engine == NULL) {
                LIFE_LOG_INFO("Starting simulation: %dx%d, pattern=%d, seed=%u",
                               ctx.width, ctx.height, ctx.pattern, ctx.seed);
                
                life_engine_t *engine = malloc(sizeof(life_engine_t));
                if (!engine) {
                    LIFE_LOG_ERROR("Failed to allocate engine");
                    quit = 1;
                    break;
                }

                life_options_t sim_options = *options;
                sim_options.width = ctx.width;
                sim_options.height = ctx.height;
                sim_options.pattern = ctx.pattern;
                sim_options.seed = ctx.seed;
                sim_options.steps = ctx.total_steps;

                if (life_engine_init(engine, ctx.width, ctx.height, &sim_options) < 0) {
                    LIFE_LOG_ERROR("Failed to initialize engine");
                    free(engine);
                    quit = 1;
                    break;
                }

                ctx.engine = engine;
                ctx.generation = 0;
            }

            /* Run one step at a time */
            while (SDL_PollEvent(&event)) {
                handle_window_event(&ctx, &event);
                handle_pan_zoom_keys(&ctx, &event);
                if (event.type == SDL_QUIT) {
                    ctx.mode = MODE_QUIT;
                    break;
                } else if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        ctx.mode = MODE_PAUSED;
                        break;
                    } else if (event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_ESCAPE) {
                        ctx.mode = MODE_QUIT;
                        break;
                    }
                }
            }

            if (ctx.mode != MODE_RUNNING) {
                continue;
            }

            if (life_engine_step(ctx.engine) == 0) {
                ctx.generation++;
                ctx.population = count_population(life_engine_current_grid(ctx.engine),
                                                  ctx.engine->width, ctx.engine->height);
                render_grid(&ctx);

                if (ctx.generation >= ctx.total_steps) {
                    ctx.mode = MODE_PAUSED;
                    LIFE_LOG_INFO("Simulation complete at generation %d", ctx.generation);
                }
            } else {
                ctx.mode = MODE_PAUSED;
            }
        } else if (ctx.mode == MODE_PAUSED) {
            /* Paused mode: wait for user action */
            if (SDL_WaitEventTimeout(&event, 100)) {
                handle_window_event(&ctx, &event);
                handle_pan_zoom_keys(&ctx, &event);
                if (event.type == SDL_QUIT) {
                    ctx.mode = MODE_QUIT;
                } else if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        ctx.mode = MODE_RUNNING;
                        break;
                    case SDLK_RETURN:
                    case SDLK_p:
                        ctx.mode = MODE_RUNNING;
                        break;
                    case SDLK_RIGHT:
                    case SDLK_n:
                        /* Single step */
                        if (ctx.engine && ctx.generation < ctx.total_steps) {
                            if (life_engine_step(ctx.engine) == 0) {
                                ctx.generation++;
                                ctx.population = count_population(life_engine_current_grid(ctx.engine),
                                                                  ctx.engine->width, ctx.engine->height);
                                LIFE_LOG_INFO("Stepped to generation %d, population: %d", 
                                               ctx.generation, ctx.population);
                                if (ctx.generation >= ctx.total_steps) {
                                    LIFE_LOG_INFO("Simulation complete");
                                }
                            }
                        }
                        break;
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        ctx.mode = MODE_QUIT;
                        break;
                    case SDLK_m:
                        /* Return to menu */
                        if (ctx.engine) {
                            life_engine_destroy(ctx.engine);
                            free(ctx.engine);
                            ctx.engine = NULL;
                        }
                        ctx.generation = 0;
                        ctx.mode = MODE_MENU;
                        break;
                    default:
                        break;
                    }
                }
            }
            if (ctx.engine) {
                render_grid(&ctx);
            }
        }
    }

    LIFE_LOG_INFO("UI mode ended: %d", ctx.mode);

    /* Cleanup */
    if (ctx.engine) {
        life_engine_copy_current(ctx.engine, final_grid);
        life_engine_destroy(ctx.engine);
        free(ctx.engine);
    }
    free(final_grid);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    LIFE_LOG_INFO("UI closed cleanly");
    return 0;
}

#else

/* Stub when SDL2 not available */
int life_ui_sdl2_run(const life_options_t *options __attribute__((unused))) {
    fprintf(stderr, "Error: SDL2 not available. Install libsdl2-dev and rebuild.\n");
    return -1;
}

#endif

/* Terminal UI stub for now */
int life_ui_terminal_run(const life_options_t *options __attribute__((unused))) {
    fprintf(stderr, "Terminal UI not yet implemented\n");
    return -1;
}
