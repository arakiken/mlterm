/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef _COMM_H_

#define _COMM_H_
typedef struct Window {
  int top;
  int bottom;
  int left;
  int right;
  int framed;
  struct Window *parent;
} window_t;

typedef enum decor { DC_NORMAL, DC_CHOOSED, DC_POINTED } decor_t;

enum {
  KEY_UP = 256, /* char max +1 */
  KEY_DOWN,
  KEY_RIGHT,
  KEY_LEFT,
  KEY_ESC,
  KEY_DEL,
  KEY_BS
};
/*
 *  control terminal
 */
void set_cursor_pos(window_t *window, int x, int y);

int read_one(void);
void flush_stdout(void);

/* alternate screen */
void set_altscr(void);
void unset_altscr(void);

void set_keypad(void);
void unset_keypad(void);

void set_fg_color(int colorid);
void set_fg_color_default(void);
void set_bg_color(int colorid);

void cursor_show(void);
void cursor_hide(void);

void dec_char(void);
void normal_char(void);

int term_size(int *w, int *h);

char *mlterm_get_color_param(const char *key);
char *mlterm_get_font_param(const char *file, const char *key);
char *mlterm_get_param(const char *key);
void mlterm_set_color_param(const char *key, char *value);
void mlterm_set_font_param(const char *file, const char *key, char *value);
void mlterm_set_param(const char *key, char *value);
void mlterm_set_value(const char *key, int value);
void mlterm_exec(const char *cmd);

/*
 *  text window management
 */
window_t *window_new(int left, int top, int right, int bottom, int framed, window_t *parent);
void window_addstr(window_t *window, int x, int y, const char *str);
void window_clear(window_t *window);
void window_free(window_t *window);
int window_width(window_t *window);

/*
 *  termios (for unbuffered I/O)
 */
int termios_init(void);
int termios_final(void);

/*
 *  convenience functions
 */
void display_colorcube(window_t *window, int x, int y, int colorid);
void display_str(window_t *window, int x, int y, const char *src, decor_t flag);
void display_numeric(window_t *window, int x, int y, int value, const char *unit, decor_t flag);
int colorid_from_name(char *name);
const char *colorname_from_id(int colorid);

int string_edit(window_t *window, char *src, char **result);
int color_select(window_t *edit, int initial);
#endif
