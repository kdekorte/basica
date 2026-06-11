#ifndef GRAPHICS_H
#define GRAPHICS_H

int init_graphics();
void update_graphics();
void close_graphics();
void set_screen_mode(int mode);
void set_pixel(int x, int y, int color);
void draw_line(int x1, int y1, int x2, int y2, int color, int fill);
void draw_circle(int cx, int cy, int radius, int color, int fill);
int get_pixel(int x, int y);
void draw_paint(int x, int y, int color, int border_color);
void set_text_cursor(int row, int col);
void set_text_color(int color);
void graphics_cls();
int graphics_is_active();
void handle_events();
void wait_for_keypress();
void set_window_title(const char *title);

void get_graphics_cursor(int *x, int *y);
void set_graphics_cursor(int x, int y);

void graphics_sleep(int ms);
void graphics_print(const char *text);
int get_graphics_key(void);
int get_graphics_char(void);
void graphics_readline(char *buffer, int size);
void graphics_present_now();
void set_present_interval(int ms);

#endif