#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "graphics.h"
#include "interpreter.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
void update_graphics(); // Forward declaration
static SDL_Texture *glyph_cache[128] = {NULL};
static SDL_Texture *canvas = NULL;
static SDL_Color current_text_color = {255, 255, 255, 255};
static int canvas_width = 1280;
static int canvas_height = 400;
static int cursor_x = 0;
static int cursor_y = 0;
static TTF_Font *font = NULL;
static const int FONT_SIZE = 16;         // Use a DOS-style pixel font size
static int current_row_height = 16;      // Use pixel-based rows for doubled text modes
static int current_col_width = 16;       // Defaults to 80 columns at doubled resolution
static int last_key_code = 0;
static int last_key_char = 0;

static int mode_res_w = 640;
static int mode_res_h = 200;
static int text_columns = 80;
static int text_rows = 25;

void set_screen_mode(int mode) {
    switch(mode) {
        case 1:  mode_res_w = 320; mode_res_h = 200; break;
        case 2:  mode_res_w = 640; mode_res_h = 200; break;
        case 7:  mode_res_w = 320; mode_res_h = 200; break;
        case 8:  mode_res_w = 640; mode_res_h = 200; break;
        case 9:  mode_res_w = 640; mode_res_h = 350; break;
        case 12: mode_res_w = 640; mode_res_h = 480; break;
        case 13: mode_res_w = 320; mode_res_h = 200; break;
        case 0:
        default: mode_res_w = 640; mode_res_h = 200; break;
    }

    canvas_width = mode_res_w * 2;
    canvas_height = mode_res_h * 2;

    // Determine text layout based on classic BASIC mode documentation
    if (mode_res_w <= 320) text_columns = 40; else text_columns = 80;
    if (mode_res_h == 200) text_rows = 25;
    else if (mode_res_h == 350) text_rows = 35;
    else if (mode_res_h == 480) text_rows = 25; // standard 80x25 layout for 640x480
    else text_rows = 25;

    current_col_width = canvas_width / text_columns;
    current_row_height = canvas_height / text_rows;
    if (font) {
        int glyph_w = 0, glyph_h = 0;
        if (TTF_GetStringSize(font, "W", 0, &glyph_w, &glyph_h)) {
            int line_skip = TTF_GetFontLineSkip(font);
            if (line_skip > 0 && line_skip < current_row_height) current_row_height = line_skip;
            if (glyph_h > 0 && glyph_h < current_row_height) current_row_height = glyph_h;
        }
    }
    if (current_row_height < 1) current_row_height = 1;

    // Recreate the canvas at the new doubled resolution
    if (renderer) {
        if (canvas) SDL_DestroyTexture(canvas);
        canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, canvas_width, canvas_height);
        SDL_SetTextureBlendMode(canvas, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST); // Pixel-perfect retro graphics
    }

    if (renderer && canvas) {
        SDL_SetRenderTarget(renderer, canvas);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, NULL);
        // Render to window using centered 4:3 destination rect
        if (window) {
            int win_w, win_h;
            SDL_GetWindowSize(window, &win_w, &win_h);
            double target_aspect = 4.0 / 3.0;
            int dest_w = win_w;
            int dest_h = (int)(dest_w / target_aspect + 0.5);
            if (dest_h > win_h) {
                dest_h = win_h;
                dest_w = (int)(dest_h * target_aspect + 0.5);
            }
            SDL_FRect dst = { (float)((win_w - dest_w) / 2), (float)((win_h - dest_h) / 2), (float)dest_w, (float)dest_h };
            SDL_SetRenderTarget(renderer, NULL);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, canvas, NULL, &dst);
            SDL_RenderPresent(renderer);
        }
    }
}

static void check_scroll() {
    if (!font) return;
    if (cursor_y + current_row_height > canvas_height) {
        SDL_Texture *temp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, canvas_width, canvas_height);
        SDL_SetRenderTarget(renderer, temp);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_FRect src = {0.0f, (float)current_row_height, (float)canvas_width, (float)(canvas_height - current_row_height)};
        SDL_FRect dst = {0.0f, 0.0f, (float)canvas_width, (float)(canvas_height - current_row_height)};
        SDL_RenderTexture(renderer, canvas, &src, &dst);

        SDL_SetRenderTarget(renderer, canvas);
        SDL_RenderTexture(renderer, temp, NULL, NULL);

        // Clear the new line at the bottom
        SDL_FRect bottom_rect = {0.0f, (float)(canvas_height - current_row_height), (float)canvas_width, (float)current_row_height};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &bottom_rect);

        SDL_DestroyTexture(temp);
        cursor_y -= current_row_height;
    }
}

void graphics_cls() {
    if (renderer && canvas) {
        SDL_SetRenderTarget(renderer, canvas);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, NULL);
        cursor_x = 0;
        cursor_y = 0;
        update_graphics();
    }
}

void graphics_sleep(int ms) {
    if (ms > 0) {
        graphics_present_now();
        handle_events();
        SDL_Delay(ms);
    }
}

static int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

void graphics_print(const char *text) {
    if (!font || !canvas || !text) return;
    
    while (*text) {
        if (*text == '\n') {
            cursor_x = 0;
            cursor_y += current_row_height;
            check_scroll();
            SDL_SetRenderTarget(renderer, canvas);
            text++;
            continue;
        }
        if (*text == '\r') {
            cursor_x = 0;
            text++;
            continue;
        }
        if (*text == ' ') {
            cursor_x += current_col_width;
            text++;
        } else {
            unsigned char c = (unsigned char)*text;
            if (c < 128 && glyph_cache[c]) {
                SDL_SetTextureColorMod(glyph_cache[c], current_text_color.r, current_text_color.g, current_text_color.b);
                SDL_FRect dest = {(float)cursor_x, (float)cursor_y, (float)current_col_width, (float)current_row_height};
                SDL_RenderTexture(renderer, glyph_cache[c], NULL, &dest);
                text++;
            } else {
                int len = utf8_char_len(c);
                char s[5] = {0};
                for (int i = 0; i < len && text[i]; i++) s[i] = text[i];
                SDL_Surface* surf = TTF_RenderText_Blended(font, s, 0, current_text_color);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_FRect dest = {(float)cursor_x, (float)cursor_y, (float)current_col_width, (float)current_row_height};
                    SDL_RenderTexture(renderer, tex, NULL, &dest);
                    SDL_DestroySurface(surf);
                    SDL_DestroyTexture(tex);
                }
                text += len;
            }
            cursor_x += current_col_width;
        }

        if (cursor_x >= canvas_width) {
            cursor_x = 0;
            cursor_y += current_row_height;
            check_scroll();
            SDL_SetRenderTarget(renderer, canvas);
        }
    }
    update_graphics();
}

void set_text_cursor(int row, int col) {
    if (!font) return;
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    cursor_x = (col - 1) * current_col_width;
    cursor_y = (row - 1) * current_row_height;
}

int graphics_is_active() {
    return window != NULL;
}

int init_graphics() {
    if (window) return 1;
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0;
    if (!TTF_Init()) return 0;
    
    // Create a 4:3 Window (1024x768)
    window = SDL_CreateWindow("BASICA Virtual Framebuffer", 1024, 768, 0);
        
    if (!window) return 0;
    renderer = SDL_CreateRenderer(window, NULL); // Use default renderer flags

    set_screen_mode(2); // Default graphics mode when no SCREEN command is issued
    
    // Search for Modern DOS fonts in common directories and use the first one found.
    const char *font_candidates[] = {
        "./fonts/ModernDOS8x16.ttf",
        "./ModernDOS8x16.ttf",
        "./ModernDOS9x18.ttf",
        "./ModernDOS10x20.ttf",
        "./ModernDOS11x22.ttf",
        "/Library/Fonts/ModernDOS8x16.ttf",
        "/Library/Fonts/ModernDOS9x18.ttf",
        "/Library/Fonts/ModernDOS10x20.ttf",
        "/Library/Fonts/ModernDOS11x22.ttf",
        "/System/Library/Fonts/ModernDOS8x16.ttf",
        "/System/Library/Fonts/ModernDOS9x18.ttf",
        "/usr/share/fonts/truetype/modern-dos/ModernDOS8x16.ttf",
        "/usr/share/fonts/truetype/modern-dos/ModernDOS9x18.ttf",
        NULL
    };
    for (int i = 0; font_candidates[i] != NULL; i++) {
        font = TTF_OpenFont(font_candidates[i], FONT_SIZE);
        if (font) break;
    }
    if (!font) font = TTF_OpenFont("/System/Library/Fonts/Monaco.ttf", FONT_SIZE);
    if (!font) font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Courier New.ttf", FONT_SIZE);
    if (!font) font = TTF_OpenFont("/Library/Fonts/Andale Mono.ttf", FONT_SIZE);
    if (!font) font = TTF_OpenFont("/Library/Fonts/Arial.ttf", FONT_SIZE);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", SDL_GetError());
        return 0; // Indicate graphics initialization failure
    }

    // Set text cell dimensions for the current mode so SCREEN-based text stays aligned.
    int columns = text_columns;
    int glyph_w = 0, glyph_h = 0;
    if (!TTF_GetStringSize(font, "W", 0, &glyph_w, &glyph_h)) {
        glyph_w = 0;
        glyph_h = 0;
    }
    current_col_width = canvas_width / columns;

    int line_skip = TTF_GetFontLineSkip(font);
    int proposed_row_height = canvas_height / text_rows;
    current_row_height = proposed_row_height;
    if (line_skip > 0 && line_skip < current_row_height) current_row_height = line_skip;
    if (glyph_h > 0 && glyph_h < current_row_height) current_row_height = glyph_h;
    if (current_row_height < 1) current_row_height = 1;

    // Initialize Glyph Cache for ASCII
    SDL_Color white = {255, 255, 255, 255};
    for (int i = 32; i < 127; i++) {
        char s[2] = {(char)i, 0};
        SDL_Surface* surf = TTF_RenderText_Blended(font, s, 0, white);
        if (surf) {
            glyph_cache[i] = SDL_CreateTextureFromSurface(renderer, surf);
            if (glyph_cache[i]) SDL_SetTextureBlendMode(glyph_cache[i], SDL_BLENDMODE_BLEND);
            SDL_DestroySurface(surf);
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_SetRenderTarget(renderer, canvas);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    // Present initial cleared canvas using centered 4:3 rendering
    if (window) {
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        double target_aspect = 4.0 / 3.0;
        int dest_w = win_w;
        int dest_h = (int)(dest_w / target_aspect + 0.5);
        if (dest_h > win_h) {
            dest_h = win_h;
            dest_w = (int)(dest_h * target_aspect + 0.5);
        }
        SDL_FRect dst = { (float)((win_w - dest_w) / 2), (float)((win_h - dest_h) / 2), (float)dest_w, (float)dest_h };
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, canvas, NULL, &dst);
        SDL_RenderPresent(renderer);
    }
    current_text_color = (SDL_Color){255, 255, 255, 255};
    return 1;
}

// Presentation throttling: present to the window at most once per interval (ms)
static Uint64 last_present = 0;
static int present_interval_ms = 33; // default ~30 FPS

void set_present_interval(int ms) {
    if (ms < 0) ms = 0;
    present_interval_ms = ms;
}

void graphics_present_now() {
    if (!renderer || !canvas) return;
    if (window) {
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        double target_aspect = 4.0 / 3.0;
        int dest_w = win_w;
        int dest_h = (int)(dest_w / target_aspect + 0.5);
        if (dest_h > win_h) {
            dest_h = win_h;
            dest_w = (int)(dest_h * target_aspect + 0.5);
        }
        SDL_FRect dst = { (float)((win_w - dest_w) / 2), (float)((win_h - dest_h) / 2), (float)dest_w, (float)dest_h };
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, canvas, NULL, &dst);
        SDL_RenderPresent(renderer);
        last_present = SDL_GetTicks();
        SDL_SetRenderTarget(renderer, canvas);
    }
}

void set_text_color(int color_value) {
    if (color_value < 0) color_value = 0;
    if (color_value > 15) color_value = 15;
    switch (color_value) {
        case 0: current_text_color = (SDL_Color){0, 0, 0, 255}; break;
        case 1: current_text_color = (SDL_Color){0, 0, 170, 255}; break;
        case 2: current_text_color = (SDL_Color){0, 170, 0, 255}; break;
        case 3: current_text_color = (SDL_Color){0, 170, 170, 255}; break;
        case 4: current_text_color = (SDL_Color){170, 0, 0, 255}; break;
        case 5: current_text_color = (SDL_Color){170, 0, 170, 255}; break;
        case 6: current_text_color = (SDL_Color){170, 85, 0, 255}; break;
        case 7: current_text_color = (SDL_Color){170, 170, 170, 255}; break;
        case 8: current_text_color = (SDL_Color){85, 85, 85, 255}; break;
        case 9: current_text_color = (SDL_Color){85, 85, 255, 255}; break;
        case 10: current_text_color = (SDL_Color){85, 255, 85, 255}; break;
        case 11: current_text_color = (SDL_Color){85, 255, 255, 255}; break;
        case 12: current_text_color = (SDL_Color){255, 85, 85, 255}; break;
        case 13: current_text_color = (SDL_Color){255, 85, 255, 255}; break;
        case 14: current_text_color = (SDL_Color){255, 255, 85, 255}; break;
        case 15: current_text_color = (SDL_Color){255, 255, 255, 255}; break;
        default: current_text_color = (SDL_Color){255, 255, 255, 255}; break;
    }
}

static SDL_Color get_graphics_color(int color_value) {
    if (color_value < 0) color_value = 0;
    if (color_value > 15) color_value = 15;
    switch (color_value) {
        case 0: return (SDL_Color){0, 0, 0, 255};
        case 1: return (SDL_Color){0, 0, 170, 255};
        case 2: return (SDL_Color){0, 170, 0, 255};
        case 3: return (SDL_Color){0, 170, 170, 255};
        case 4: return (SDL_Color){170, 0, 0, 255};
        case 5: return (SDL_Color){170, 0, 170, 255};
        case 6: return (SDL_Color){170, 85, 0, 255};
        case 7: return (SDL_Color){170, 170, 170, 255};
        case 8: return (SDL_Color){85, 85, 85, 255};
        case 9: return (SDL_Color){85, 85, 255, 255};
        case 10: return (SDL_Color){85, 255, 85, 255};
        case 11: return (SDL_Color){85, 255, 255, 255};
        case 12: return (SDL_Color){255, 85, 85, 255};
        case 13: return (SDL_Color){255, 85, 255, 255};
        case 14: return (SDL_Color){255, 255, 85, 255};
        case 15: return (SDL_Color){255, 255, 255, 255};
        default: return (SDL_Color){255, 255, 255, 255};
    }
}

void set_pixel(int x, int y, int color) {
    if (!renderer || !canvas) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // Ensure opaque drawing
    SDL_SetRenderDrawColor(renderer, get_graphics_color(color).r, get_graphics_color(color).g, get_graphics_color(color).b, get_graphics_color(color).a);
    
    double xs = (double)canvas_width / mode_res_w;
    double ys = (double)canvas_height / mode_res_h;
    
    SDL_FRect r = { (float)(x * xs), (float)(y * ys), (float)(xs + 0.9f), (float)(ys + 0.9f) };
    SDL_RenderFillRect(renderer, &r);
    update_graphics();
}

void draw_line(int x1, int y1, int x2, int y2, int color, int fill) {
    if (!renderer || !canvas) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // Ensure opaque drawing
    SDL_Color draw_color = get_graphics_color(color);
    SDL_SetRenderDrawColor(renderer, draw_color.r, draw_color.g, draw_color.b, draw_color.a);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    double xs = (double)canvas_width / mode_res_w;
    double ys = (double)canvas_height / mode_res_h;

    int sx1 = (int)(x1 * xs + 0.5), sy1 = (int)(y1 * ys + 0.5);
    int sx2 = (int)(x2 * xs + 0.5), sy2 = (int)(y2 * ys + 0.5);
    
    if (fill) {
        int left = (sx1 < sx2 ? sx1 : sx2);
        int top = (sy1 < sy2 ? sy1 : sy2);
        int width = abs(sx1 - sx2) + (int)(xs + 0.5);
        int height = abs(sy1 - sy2) + (int)(ys + 0.5);
        SDL_FRect r = {
            (float)left, (float)top, (float)width, (float)height
        };
        if (fill == 2) SDL_RenderFillRect(renderer, &r);
        else SDL_RenderRect(renderer, &r);
    } else {
        SDL_RenderLine(renderer, (float)sx1, (float)sy1, (float)sx2, (float)sy2);
    }
    update_graphics();
}

void draw_circle(int cx, int cy, int radius, int color, int fill) {
    if (!renderer || !canvas || radius <= 0) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // Ensure opaque drawing
    SDL_Color draw_color = get_graphics_color(color);
    SDL_SetRenderDrawColor(renderer, draw_color.r, draw_color.g, draw_color.b, draw_color.a);

    double xs = (double)canvas_width / mode_res_w;
    double ys = (double)canvas_height / mode_res_h;

    int scx = (int)(cx * xs);
    int scy = (int)(cy * ys);
    int sr = (int)(radius * xs); // Use xs scaling for radius
    
    if (fill == 2) {
        // Filled circle using Midpoint Circle Algorithm
        int x = 0;
        int y = sr;
        int d = 3 - 2 * sr;
        
        while (x <= y) {
            // Draw horizontal lines at each y offset
            SDL_RenderLine(renderer, (float)(scx - x), (float)(scy + y), (float)(scx + x), (float)(scy + y));
            SDL_RenderLine(renderer, (float)(scx - x), (float)(scy - y), (float)(scx + x), (float)(scy - y));
            SDL_RenderLine(renderer, (float)(scx - y), (float)(scy + x), (float)(scx + y), (float)(scy + x));
            SDL_RenderLine(renderer, (float)(scx - y), (float)(scy - x), (float)(scx + y), (float)(scy - x));
            
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    } else {
        // Circle outline using Midpoint Circle Algorithm
        int x = 0;
        int y = sr;
        int d = 3 - 2 * sr;
        
        while (x <= y) {
            // Draw 8 octants
            SDL_RenderPoint(renderer, (float)(scx + x), (float)(scy + y));
            SDL_RenderPoint(renderer, (float)(scx - x), (float)(scy + y));
            SDL_RenderPoint(renderer, (float)(scx + x), (float)(scy - y));
            SDL_RenderPoint(renderer, (float)(scx - x), (float)(scy - y));
            SDL_RenderPoint(renderer, (float)(scx + y), (float)(scy + x));
            SDL_RenderPoint(renderer, (float)(scx - y), (float)(scy + x));
            SDL_RenderPoint(renderer, (float)(scx + y), (float)(scy - x));
            SDL_RenderPoint(renderer, (float)(scx - y), (float)(scy - x));
            
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
    update_graphics();
}

void set_window_title(const char *title) {
    if (!window || !title) return;
    SDL_SetWindowTitle(window, title);
}

void update_graphics() {
    if (!renderer || !canvas) return;

    // Throttle presents to avoid flickering and heavy CPU usage in tight loops.
    Uint64 now = SDL_GetTicks();
    if (present_interval_ms > 0 && (now - last_present < (Uint64)present_interval_ms)) { // Throttle check
        return;
    }
    handle_events(); // Process events only when a frame is presented

    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (window) {
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        double target_aspect = 4.0 / 3.0;
        int dest_w = win_w;
        int dest_h = (int)(dest_w / target_aspect + 0.5);
        if (dest_h > win_h) {
            dest_h = win_h;
            dest_w = (int)(dest_h * target_aspect + 0.5);
        }
        SDL_FRect dst = { (float)((win_w - dest_w) / 2), (float)((win_h - dest_h) / 2), (float)dest_w, (float)dest_h };
        SDL_RenderTexture(renderer, canvas, NULL, &dst);
    } else {
        SDL_RenderTexture(renderer, canvas, NULL, NULL);
    }

    SDL_RenderPresent(renderer);
    last_present = SDL_GetTicks();
    SDL_SetRenderTarget(renderer, canvas);
}

void graphics_readline(char *buffer, int size) {
    int pos = 0;
    buffer[0] = '\0';
    SDL_StartTextInput(window);
    
    // Store the starting position for the current input line
    int line_start_x = cursor_x;
    int line_start_y = cursor_y;

    while (!stop_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) exit(0);
            if (e.type == SDL_EVENT_TEXT_INPUT) {
                if (pos < size - 2) {
                    // Append the new character
                    strcat(buffer, e.text.text);
                    pos = strlen(buffer); 
                    buffer[pos] = '\0';

                    // Clear the current input line area and re-render
                    SDL_SetRenderTarget(renderer, canvas);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_FRect clear_rect = {(float)line_start_x, (float)line_start_y, (float)(canvas_width - line_start_x), (float)current_row_height};
                    SDL_RenderFillRect(renderer, &clear_rect);

                    // Reset cursor for rendering the buffer
                    cursor_x = line_start_x;
                    cursor_y = line_start_y;
                    graphics_print(buffer); 
                }
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_RETURN) {
                    graphics_print("\n");
                    SDL_StopTextInput(window);
                    return;
                } else if (e.key.key == SDLK_BACKSPACE && pos > 0) {
                    buffer[--pos] = '\0'; 

                    // Clear the current input line area and re-render
                    SDL_SetRenderTarget(renderer, canvas);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_FRect clear_rect = {(float)line_start_x, (float)line_start_y, (float)(canvas_width - line_start_x), (float)current_row_height};
                    SDL_RenderFillRect(renderer, &clear_rect);

                    // Reset cursor for rendering the buffer
                    cursor_x = line_start_x;
                    cursor_y = line_start_y;
                    graphics_print(buffer); 
                } else if (e.key.key == SDLK_C && (e.key.mod & SDL_KMOD_CTRL)) {
                    stop_running = 1;
                    SDL_StopTextInput(window);
                    return;
                }
            }
        }
        SDL_Delay(10);
    }
    SDL_StopTextInput(window);
}

void handle_events() {
    if (!window) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) exit(0);
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            if (e.text.text[0]) last_key_char = (unsigned char)e.text.text[0];
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_UP:    last_key_code = 1; break;
                case SDLK_RIGHT: last_key_code = 2; break;
                case SDLK_DOWN:  last_key_code = 3; break;
                case SDLK_LEFT:  last_key_code = 4; break;
                case SDLK_ESCAPE: stop_running = 1; break;
                case SDLK_C:
                    if (e.key.mod & SDL_KMOD_CTRL) stop_running = 1;
                    break;
                default:
                    if (e.key.key >= 32 && e.key.key < 127) {
                        last_key_char = (int)e.key.key;
                    }
                    break;
            }
        }
    }
}

int get_graphics_key(void) {
    handle_events();
    int key = last_key_code;
    last_key_code = 0;
    return key;
}

int get_graphics_char(void) {
    handle_events();
    int c = last_key_char;
    last_key_char = 0;
    return c;
}

void wait_for_keypress() {
    if (!window) return;
    SDL_Event e;
    while (SDL_WaitEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) exit(0);
        if (e.type == SDL_EVENT_KEY_DOWN) break;
    }
}

void close_graphics() {
    for (int i = 0; i < 128; i++) {
        if (glyph_cache[i]) SDL_DestroyTexture(glyph_cache[i]);
    }
    if (font) TTF_CloseFont(font);
    if (canvas) SDL_DestroyTexture(canvas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}