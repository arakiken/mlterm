/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_SCREEN_H__
#define __VT_SCREEN_H__

#include <time.h>
#include <pobl/bl_types.h> /* int8_t */
#include <mef/ef_conv.h>

#include "vt_edit.h"
#include "vt_logs.h"
#include "vt_logical_visual.h"

#define MAX_PAGE_ID 8

typedef struct vt_screen_event_listener {
  void *self;

  int (*screen_is_static)(void *);
  void (*line_scrolled_out)(void *);
  int (*window_scroll_upward_region)(void *, int, int, u_int);
  int (*window_scroll_downward_region)(void *, int, int, u_int);

} vt_screen_event_listener_t;

typedef enum vt_bs_mode {
  BSM_NONE = 0x0,
  BSM_DEFAULT,
  BSM_STATIC,

  BSM_MAX

} vt_bs_mode_t;

typedef enum vt_write_content_area {
  WCA_ALL,
  WCA_CURSOR_LINE,
  WCA_SCREEN,
} vt_write_content_area_t;

typedef struct vt_stored_edit {
  vt_edit_t edit;
  u_int32_t time;

} vt_stored_edit_t;

typedef struct vt_screen {
  /* public(readonly) */
  vt_edit_t *edit;

  /*
   * private
   */

  vt_edit_t normal_edit;
  vt_edit_t alt_edit;
  vt_edit_t *page_edits;         /* stores 8 pages */
  vt_stored_edit_t *stored_edit; /* Store logical edits. */
  vt_edit_t *main_edit;
  vt_edit_t *status_edit;

  vt_edit_scroll_event_listener_t edit_scroll_listener;

  vt_logs_t logs;

  vt_logical_visual_t *logvis;
  vt_logical_visual_t *container_logvis;

  vt_screen_event_listener_t *screen_listener;

  struct {
    int (*match)(size_t *, size_t *, void *, u_char *, int);

    /* Logical order */
    int char_index;
    int row;

  } * search;

  u_int backscroll_rows;
  /* vt_bs_mode_t */ int8_t backscroll_mode;
  int8_t is_backscrolling;

  int8_t has_blinking_char;

  int8_t has_status_line;

} vt_screen_t;

void vt_set_word_separators(const char *seps);

char *vt_get_word_separators(void);

void vt_set_regard_uri_as_word(int flag);

int vt_get_regard_uri_as_word(void);

#define vt_free_word_separators() vt_set_word_separators(NULL)

vt_screen_t *vt_screen_new(u_int cols, u_int rows, u_int tab_size, u_int num_log_lines,
                           int use_bce, vt_bs_mode_t bs_mode);

int vt_screen_delete(vt_screen_t *screen);

void vt_screen_set_listener(vt_screen_t *screen, vt_screen_event_listener_t *screen_listener);

/* This considers status line */
int vt_screen_resize(vt_screen_t *screen, u_int cols, u_int rows);

#define vt_screen_set_use_bce(screen, use) \
  vt_edit_set_use_bce(&(screen)->alt_edit, vt_edit_set_use_bce(&(screen)->normal_edit, use))

#define vt_screen_is_using_bce(screen) vt_edit_is_using_bce((screen)->edit)

#define vt_screen_set_bce_fg_color(screen, fg_color) \
  vt_edit_set_bce_fg_color((screen)->edit, fg_color)

#define vt_screen_set_bce_bg_color(screen, bg_color) \
  vt_edit_set_bce_bg_color((screen)->edit, bg_color)

#define vt_screen_cursor_char_index(screen) vt_cursor_char_index((screen)->edit)

#define vt_screen_cursor_col(screen) vt_cursor_col((screen)->edit)

/* XXX Don't call this in visual context for now. */
#define vt_screen_cursor_logical_col(screen) vt_edit_cursor_logical_col((screen)->edit)

/* This considers status line */
int vt_screen_cursor_row(vt_screen_t *screen);

/* This considers status line */
int vt_screen_cursor_row_in_screen(vt_screen_t *screen);

/* XXX Don't call this in visual context for now. */
#define vt_screen_cursor_logical_row(screen) vt_edit_cursor_logical_row((screen)->edit)

#define vt_screen_get_cols(screen) vt_edit_get_cols((screen)->edit)

/* This considers status line */
u_int vt_screen_get_rows(vt_screen_t *screen);

u_int vt_screen_get_logical_cols(vt_screen_t *screen);

u_int vt_screen_get_logical_rows(vt_screen_t *screen);

#define vt_screen_get_log_size(screen) vt_get_log_size(&(screen)->logs)

#define vt_screen_change_log_size(screen, log_size) vt_change_log_size(&(screen)->logs, log_size)

#define vt_screen_unlimit_log_size(screen) vt_unlimit_log_size(&(screen)->logs)

#define vt_screen_log_size_is_unlimited(screen) vt_log_size_is_unlimited(&(screen)->logs)

#define vt_screen_get_num_logged_lines(screen) vt_get_num_logged_lines(&(screen)->logs)

int vt_screen_convert_scr_row_to_abs(vt_screen_t *screen, int row);

/* This considers status line */
vt_line_t *vt_screen_get_line(vt_screen_t *screen, int row);

/* This considers status line */
vt_line_t *vt_screen_get_line_in_screen(vt_screen_t *screen, int row);

#define vt_screen_get_cursor_line(screen) \
  vt_edit_get_line((screen)->edit, vt_cursor_row((screen)->edit))

void vt_screen_set_modified_all(vt_screen_t *screen);

int vt_screen_add_logical_visual(vt_screen_t *screen, vt_logical_visual_t *logvis);

int vt_screen_delete_logical_visual(vt_screen_t *screen);

int vt_screen_render(vt_screen_t *screen);

int vt_screen_visual(vt_screen_t *screen);

int vt_screen_logical(vt_screen_t *screen);

#define vt_screen_logical_visual_is_reversible(screen) \
  (!(screen)->logvis || (screen)->logvis->is_reversible)

vt_bs_mode_t vt_screen_is_backscrolling(vt_screen_t *screen);

void vt_set_backscroll_mode(vt_screen_t *screen, vt_bs_mode_t mode);

#define vt_get_backscroll_mode(screen) ((screen)->backscroll_mode)

void vt_enter_backscroll_mode(vt_screen_t *screen);

void vt_exit_backscroll_mode(vt_screen_t *screen);

int vt_screen_backscroll_to(vt_screen_t *screen, int row);

int vt_screen_backscroll_upward(vt_screen_t *screen, u_int size);

int vt_screen_backscroll_downward(vt_screen_t *screen, u_int size);

#define vt_screen_get_tab_size(screen) vt_edit_get_tab_size((screen)->edit)

#define vt_screen_set_tab_size(screen, tab_size) vt_edit_set_tab_size((screen)->edit, tab_size)

#define vt_screen_is_tab_stop(screen, col) vt_edit_is_tab_stop((screen)->edit, col)

int vt_screen_restore_color(vt_screen_t *screen, int beg_char_index, int beg_row,
                            int end_char_index, int end_row, int is_rect);

int vt_screen_reverse_color(vt_screen_t *screen, int beg_char_index, int beg_row,
                            int end_char_index, int end_row, int is_rect);

u_int vt_screen_copy_region(vt_screen_t *screen, vt_char_t *chars, u_int num_chars,
                            int beg_char_index, int beg_row, int end_char_index, int end_row,
                            int is_rect);

u_int vt_screen_get_region_size(vt_screen_t *screen, int beg_char_index, int beg_row,
                                int end_char_index, int end_row, int is_rect);

int vt_screen_get_line_region(vt_screen_t *screen, int *beg_row, int *end_char_index, int *end_row,
                              int base_row);

int vt_screen_get_word_region(vt_screen_t *screen, int *beg_char_index, int *beg_row,
                              int *end_char_index, int *end_row, int base_char_index, int base_row);

int vt_screen_search_init(vt_screen_t *screen,
                          int (*match)(size_t *, size_t *, void *, u_char *, int));

void vt_screen_search_final(vt_screen_t *screen);

int vt_screen_search_reset_position(vt_screen_t *screen);

int vt_screen_search_find(vt_screen_t *screen, int *beg_char_index, int *beg_row,
                          int *end_char_index, int *end_row, void *regex, int backward);

int vt_screen_blink(vt_screen_t *screen);

/*
 * VT100 commands (called in logical context)
 */

vt_char_t *vt_screen_get_n_prev_char(vt_screen_t *screen, int n);

int vt_screen_combine_with_prev_char(vt_screen_t *screen, u_int32_t code, ef_charset_t cs,
                                     int is_fullwidth, int is_comb, vt_color_t fg_color,
                                     vt_color_t bg_color, int is_bold, int is_italic,
                                     int line_style, int is_blinking, int is_protected);

int vt_screen_insert_chars(vt_screen_t *screen, vt_char_t *chars, u_int len);

#define vt_screen_insert_blank_chars(screen, len) vt_edit_insert_blank_chars((screen)->edit, len)

#define vt_screen_forward_tabs(screen, num) vt_edit_forward_tabs((screen)->edit, num)

#define vt_screen_backward_tabs(screen, num) vt_edit_backward_tabs((screen)->edit, num)

#define vt_screen_set_tab_stop(screen) vt_edit_set_tab_stop((screen)->edit)

#define vt_screen_clear_tab_stop(screen) vt_edit_clear_tab_stop((screen)->edit)

#define vt_screen_clear_all_tab_stops(screen) vt_edit_clear_all_tab_stops((screen)->edit)

int vt_screen_insert_new_lines(vt_screen_t *screen, u_int size);

#define vt_screen_line_feed(screen) vt_edit_go_downward((screen)->edit, SCROLL)

int vt_screen_overwrite_chars(vt_screen_t *screen, vt_char_t *chars, u_int len);

#define vt_screen_delete_cols(screen, len) vt_edit_delete_cols((screen)->edit, len)

int vt_screen_delete_lines(vt_screen_t *screen, u_int size);

#define vt_screen_clear_cols(screen, cols) vt_edit_clear_cols((screen)->edit, cols)

#define vt_screen_clear_line_to_right(screen) vt_edit_clear_line_to_right((screen)->edit)

#define vt_screen_clear_line_to_left(screen) vt_edit_clear_line_to_left((screen)->edit)

#define vt_screen_clear_below(screen) vt_edit_clear_below((screen)->edit)

#define vt_screen_clear_above(screen) vt_edit_clear_above((screen)->edit)

#define vt_screen_save_protected_chars(screen, beg_row, end_row, relative) \
  vt_edit_save_protected_chars((screen)->edit, beg_row, end_row, relative)

#define vt_screen_restore_protected_chars(screen, save) \
  vt_edit_restore_protected_chars((screen)->edit, save)

#define vt_screen_set_vmargin(screen, beg, end) vt_edit_set_vmargin((screen)->edit, beg, end)

#define vt_screen_set_use_hmargin(screen, use) vt_edit_set_use_hmargin((screen)->edit, use)

#define vt_screen_set_hmargin(screen, beg, end) vt_edit_set_hmargin((screen)->edit, beg, end)

#define vt_screen_index(screen) vt_edit_go_downward((screen)->edit, SCROLL)

#define vt_screen_reverse_index(screen) vt_edit_go_upward((screen)->edit, SCROLL)

#define vt_screen_scroll_upward(screen, size) vt_edit_scroll_upward((screen)->edit, size)

#define vt_screen_scroll_downward(screen, size) vt_edit_scroll_downward((screen)->edit, size)

#define vt_screen_scroll_leftward(screen, size) vt_edit_scroll_leftward((screen)->edit, size)

#define vt_screen_scroll_rightward(screen, size) vt_edit_scroll_rightward((screen)->edit, size)

#define vt_screen_scroll_leftward_from_cursor(screen, size) \
  vt_edit_scroll_leftward_from_cursor((screen)->edit, size)

#define vt_screen_scroll_rightward_from_cursor(screen, size) \
  vt_edit_scroll_rightward_from_cursor((screen)->edit, size)

int vt_screen_go_forward(vt_screen_t *screen, u_int size, int scroll);

int vt_screen_go_back(vt_screen_t *screen, u_int size, int scroll);

int vt_screen_go_upward(vt_screen_t *screen, u_int size);

int vt_screen_go_downward(vt_screen_t *screen, u_int size);

#define vt_screen_goto_beg_of_line(screen) vt_edit_goto_beg_of_line((screen)->edit)

#define vt_screen_go_horizontally(screen, col) \
  vt_edit_goto((screen)->edit, col, vt_edit_cursor_logical_row((screen)->edit))

#define vt_screen_go_vertically(screen, row) \
  vt_edit_goto((screen)->edit, vt_edit_cursor_logical_col((screen)->edit), row)

#define vt_screen_goto_home(screen) vt_edit_goto_home((screen)->edit)

#define vt_screen_goto(screen, col, row) vt_edit_goto((screen)->edit, col, row)

#define vt_screen_set_relative_origin(screen, flag) \
  vt_edit_set_relative_origin((screen)->edit, flag)

#define vt_screen_is_relative_origin(screen) vt_edit_is_relative_origin((screen)->edit)

#define vt_screen_set_auto_wrap(screen, flag) vt_edit_set_auto_wrap((screen)->edit, flag)

#define vt_screen_is_auto_wrap(screen) vt_edit_is_auto_wrap((screen)->edit)

#define vt_screen_set_last_column_flag(screen, flag) \
  vt_edit_set_last_column_flag((screen)->edit, flag)

#define vt_screen_get_last_column_flag(screen) vt_edit_get_last_column_flag((screen)->edit)

#define vt_screen_get_bce_ch(screen) vt_edit_get_bce_ch((screen)->edit)

#define vt_screen_save_cursor(screen) vt_edit_save_cursor((screen)->edit)

#define vt_screen_saved_cursor_to_home(screen) vt_edit_saved_cursor_to_home((screen)->edit)

#define vt_screen_restore_cursor(screen) vt_edit_restore_cursor((screen)->edit)

int vt_screen_cursor_visible(vt_screen_t *screen);

int vt_screen_cursor_invisible(vt_screen_t *screen);

#define vt_screen_is_cursor_visible(screen) ((screen)->is_cursor_visible)

/*
 * XXX
 * Note that alt_edit/normal_edit are directly switched by ui_picture.c without
 * using following 3 functions.
 */

#define vt_screen_is_alternative_edit(screen) ((screen)->edit == &(screen)->alt_edit)

int vt_screen_use_normal_edit(vt_screen_t *screen);

int vt_screen_use_alternative_edit(vt_screen_t *screen);

#define vt_screen_is_local_echo_mode(screen) ((screen)->stored_edit)

int vt_screen_enable_local_echo(vt_screen_t *screen);

int vt_screen_local_echo_wait(vt_screen_t *screen, int msec);

int vt_screen_disable_local_echo(vt_screen_t *screen);

#define vt_screen_fill_area(screen, code, is_protected, col, beg, num_cols, num_rows) \
  vt_edit_fill_area((screen)->edit, code, is_protected, col, beg, num_cols, num_rows)

void vt_screen_copy_area(vt_screen_t *screen, int src_col, int src_row, u_int num_copy_cols,
                         u_int num_copy_rows, u_int src_page,
                         int dst_col, int dst_row, u_int dst_page);

#define vt_screen_erase_area(screen, col, row, num_cols, num_rows) \
  vt_edit_erase_area((screen)->edit, col, row, num_cols, num_rows)

#define vt_screen_change_attr_area(screen, col, row, num_cols, num_rows, attr) \
  vt_edit_change_attr_area((screen)->edit, col, row, num_cols, num_rows,       \
                           vt_char_change_attr, attr)

#define vt_screen_reverse_attr_area(screen, col, row, num_cols, num_rows, attr) \
  vt_edit_change_attr_area((screen)->edit, col, row, num_cols, num_rows,        \
                           vt_char_reverse_attr, attr)

u_int16_t vt_screen_get_checksum(vt_screen_t *screen, int col, int row,
                             u_int num_cols, u_int num_rows, int page);

#define vt_screen_set_use_rect_attr_select(screen, use) \
  vt_edit_set_use_rect_attr_select((screen)->edit, use)

#define vt_screen_is_using_rect_attr_select(screen) \
  vt_edit_is_using_rect_attr_select((screen)->edit)

#define vt_screen_clear_size_attr(screen) vt_edit_clear_size_attr((screen)->edit)

void vt_screen_enable_blinking(vt_screen_t *screen);

int vt_screen_write_content(vt_screen_t *screen, int fd, ef_conv_t *conv, int clear_at_end,
                            vt_write_content_area_t area);

int vt_screen_get_page_id(vt_screen_t *screen);

int vt_screen_goto_page(vt_screen_t *screen, u_int page_id);

int vt_screen_goto_next_page(vt_screen_t *screen, u_int offset);

int vt_screen_goto_prev_page(vt_screen_t *prev, u_int offset);

#define vt_screen_cursor_is_rtl(screen) \
  ((screen)->logvis ? vt_logical_visual_cursor_is_rtl((screen)->logvis) : 0)

/* This considers status line */
#define vt_screen_has_status_line(screen) ((screen)->has_status_line)

/* This considers status line */
void vt_screen_set_use_status_line(vt_screen_t *screen, int use);

/* This considers status line */
#define vt_status_line_is_focused(screen) ((screen)->edit == (screen)->status_edit)

/* This considers status line */
void vt_focus_status_line(vt_screen_t *screen);

/* This considers status line */
void vt_focus_main_screen(vt_screen_t *screen);

#endif
