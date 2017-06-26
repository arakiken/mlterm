/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * !! Notice !!
 * Don't provide any methods modifying vt_model_t and vt_logs_t states
 * unless these are logicalized in advance.
 */

#ifndef __VT_TERM_H__
#define __VT_TERM_H__

#include <pobl/bl_def.h> /* USE_WIN32API */

#include "vt_pty.h"
#include "vt_parser.h"
#include "vt_screen.h"

typedef struct vt_term {
  /*
   * private
   */
  vt_pty_ptr_t pty;
  vt_pty_event_listener_t *pty_listener; /* pool until pty opened. */

  vt_parser_t *parser;
  vt_screen_t *screen;

  /*
   * private
   */
  char *icon_path;
  char *uri;
  char *bidi_separators;

  /* vt_bidi_mode_t */ int8_t bidi_mode;
  /* vt_vertical_mode_t */ int8_t vertical_mode;

  int8_t use_ctl;
  int8_t use_dynamic_comb;
  int8_t use_ot_layout;

  int8_t use_local_echo;
  int8_t is_attached;
#ifdef OPEN_PTY_ASYNC
  int8_t return_special_pid;
#endif

  void *user_data;

} vt_term_t;

/* XXX */
extern void (*vt_term_pty_closed_event)(vt_term_t *);

#if defined(__ANDROID__) && defined(USE_LIBSSH2)
/* XXX */
extern int start_with_local_pty;
#endif

#define vt_term_init() vt_parser_init()

void vt_term_final(void);

vt_term_t *vt_term_new(const char *term_type, u_int cols, u_int rows, u_int tab_size,
                       u_int log_size, vt_char_encoding_t encoding, int is_auto_encoding,
                       int use_auto_detect, int logging_vt_seq, vt_unicode_policy_t policy,
                       u_int col_size_a, int use_char_combining, int use_multi_col_char,
                       int use_ctl, vt_bidi_mode_t bidi_mode, const char *bidi_separators,
                       int use_dynamic_comb, vt_bs_mode_t bs_mode, vt_vertical_mode_t vertical_mode,
                       int use_local_echo, const char *win_name, const char *icon_name,
                       vt_alt_color_mode_t alt_color_mode, int use_ot_layout,
                       vt_cursor_style_t cursor_style);

int vt_term_delete(vt_term_t *term);

int vt_term_zombie(vt_term_t *term);

int vt_term_open_pty(vt_term_t *term, const char *cmd_path, char **argv, char **env,
                     const char *host, const char *work_dir, const char *pass, const char *pubkey,
                     const char *privkey, u_int width_pix, u_int height_pix);

int vt_term_plug_pty(vt_term_t *term, vt_pty_ptr_t pty);

#define vt_term_pty_is_opened(term) ((term)->pty != NULL)

int vt_term_attach(vt_term_t *term, vt_xterm_event_listener_t *xterm_listener,
                   vt_config_event_listener_t *config_listener,
                   vt_screen_event_listener_t *screen_listener,
                   vt_pty_event_listener_t *pty_listner);

int vt_term_detach(vt_term_t *term);

#define vt_term_is_attached(term) ((term)->is_attached)

#define vt_term_parse_vt100_sequence(term) vt_parse_vt100_sequence((term)->parser)

#define vt_term_reset_pending_vt100_sequence(term) vt_reset_pending_vt100_sequence((term)->parser)

#define vt_term_response_config(term, key, value, to_menu) \
  vt_response_config((term)->pty, key, value, to_menu)

#ifdef USE_LIBSSH2
#define vt_term_scp(term, dst_path, src_path, path_encoding) \
  vt_pty_ssh_scp((term)->pty, vt_term_get_encoding(term), path_encoding, dst_path, src_path, 1)
#else
#define vt_term_scp(term, dst_path, src_path, path_encoding) (0)
#endif

#define vt_term_change_encoding(term, encoding) \
  vt_parser_change_encoding((term)->parser, encoding)

#define vt_term_get_encoding(term) vt_parser_get_encoding((term)->parser)

#define vt_term_set_use_ctl(term, flag) ((term)->use_ctl = (flag))

#define vt_term_is_using_ctl(term) ((term)->use_ctl)

#define vt_term_set_bidi_mode(term, mode) ((term)->bidi_mode = (mode))

#define vt_term_get_bidi_mode(term) ((term)->bidi_mode)

void vt_term_set_use_ot_layout(vt_term_t *term, int flag);

#define vt_term_is_using_ot_layout(term) ((term)->use_ot_layout)

#define vt_term_set_vertical_mode(term, mode) ((term)->vertical_mode = (mode))

#define vt_term_get_vertical_mode(term) ((term)->vertical_mode)

#define vt_term_set_use_dynamic_comb(term, use) ((term)->use_dynamic_comb = (use))

#define vt_term_is_using_dynamic_comb(term) ((term)->use_dynamic_comb)

#define vt_term_convert_to(term, dst, len, _parser) \
  vt_parser_convert_to((term)->parser, dst, len, _parser)

#define vt_term_init_encoding_parser(term) vt_init_encoding_parser((term)->parser)

#define vt_term_init_encoding_conv(term) vt_init_encoding_conv((term)->parser)

int vt_term_get_master_fd(vt_term_t *term);

int vt_term_get_slave_fd(vt_term_t *term);

char *vt_term_get_slave_name(vt_term_t *term);

pid_t vt_term_get_child_pid(vt_term_t *term);

size_t vt_term_write(vt_term_t *term, u_char *buf, size_t len);

#define vt_term_write_modified_key(term, key, modcode) \
  (term)->pty ? vt_parser_write_modified_key((term)->parser, key, modcode) : 0

#define vt_term_write_special_key(term, key, modcode, is_numlock) \
  (term)->pty ? vt_parser_write_special_key((term)->parser, key, modcode, is_numlock) : 0

/* Must be called in visual context. */
#define vt_term_write_loopback(term, buf, len) \
  vt_parser_write_loopback((term)->parser, buf, len)

/* Must be called in visual context. */
#define vt_term_show_message(term, msg) vt_parser_show_message((term)->parser, msg)

#if defined(__ANDROID__) || defined(__APPLE__)
/* Must be called in visual context. */
#define vt_term_preedit(term, buf, len) vt_parser_preedit((term)->parser, buf, len)
#endif

int vt_term_resize(vt_term_t *term, u_int cols, u_int rows, u_int width_pix, u_int height_pix);

#define vt_term_cursor_col(term) vt_screen_cursor_col((term)->screen)

#define vt_term_cursor_char_index(term) vt_screen_cursor_char_index((term)->screen)

#define vt_term_cursor_row(term) vt_screen_cursor_row((term)->screen)

#define vt_term_cursor_row_in_screen(term) vt_screen_cursor_row_in_screen((term)->screen)

int vt_term_unhighlight_cursor(vt_term_t *term, int revert_visual);

#define vt_term_get_cols(term) vt_screen_get_cols((term)->screen)

#define vt_term_get_rows(term) vt_screen_get_rows((term)->screen)

#define vt_term_get_logical_cols(term) vt_screen_get_logical_cols((term)->screen)

#define vt_term_get_logical_rows(term) vt_screen_get_logical_rows((term)->screen)

#define vt_term_get_log_size(term) vt_screen_get_log_size((term)->screen)

#define vt_term_change_log_size(term, log_size) vt_screen_change_log_size((term)->screen, log_size)

#define vt_term_unlimit_log_size(term) vt_screen_unlimit_log_size((term)->screen)

#define vt_term_log_size_is_unlimited(term) vt_screen_log_size_is_unlimited((term)->screen)

#define vt_term_get_num_of_logged_lines(term) vt_screen_get_num_of_logged_lines((term)->screen)

#define vt_term_convert_scr_row_to_abs(term, row) \
  vt_screen_convert_scr_row_to_abs((term)->screen, row)

#define vt_term_get_line(term, row) vt_screen_get_line(term->screen, row)

#define vt_term_get_line_in_screen(term, row) vt_screen_get_line_in_screen((term)->screen, row)

#define vt_term_get_cursor_line(term) vt_screen_get_cursor_line((term)->screen)

#if 0
int vt_term_set_modified_region(vt_term_t *term, int beg_char_index, int beg_row, u_int nchars,
                                u_int nrows);

int vt_term_set_modified_region_in_screen(vt_term_t *term, int beg_char_index, int beg_row,
                                          u_int nchars, u_int nrows);
#endif

int vt_term_set_modified_lines(vt_term_t *term, int beg, int end);

int vt_term_set_modified_lines_in_screen(vt_term_t *term, int beg, int end);

int vt_term_set_modified_all_lines_in_screen(vt_term_t *term);

int vt_term_updated_all(vt_term_t *term);

int vt_term_update_special_visual(vt_term_t *term);

#define vt_term_logical_visual_is_reversible(term) \
  vt_screen_logical_visual_is_reversible((term)->screen)

#define vt_term_is_backscrolling(term) vt_screen_is_backscrolling((term)->screen)

int vt_term_enter_backscroll_mode(vt_term_t *term);

#define vt_term_exit_backscroll_mode(term) vt_exit_backscroll_mode((term)->screen)

#define vt_term_backscroll_to(term, row) vt_screen_backscroll_to((term)->screen, row)

#define vt_term_backscroll_upward(term, size) vt_screen_backscroll_upward((term)->screen, size)

#define vt_term_backscroll_downward(term, size) vt_screen_backscroll_downward((term)->screen, size)

#define vt_term_reverse_color(term, beg_char_index, beg_row, end_char_index, end_row, is_rect) \
  vt_screen_reverse_color((term)->screen, beg_char_index, beg_row, end_char_index, end_row, is_rect)

#define vt_term_restore_color(term, beg_char_index, beg_row, end_char_index, end_row, is_rect) \
  vt_screen_restore_color((term)->screen, beg_char_index, beg_row, end_char_index, end_row, is_rect)

#define vt_term_copy_region(term, chars, num_of_chars, beg_char_index, beg_row, end_char_index, \
                            end_row, is_rect)                                                   \
  vt_screen_copy_region((term)->screen, chars, num_of_chars, beg_char_index, beg_row,           \
                        end_char_index, end_row, is_rect)

#define vt_term_get_region_size(term, beg_char_index, beg_row, end_char_index, end_row, is_rect) \
  vt_screen_get_region_size((term)->screen, beg_char_index, beg_row, end_char_index, end_row,    \
                            is_rect)

#define vt_term_get_line_region(term, beg_row, end_char_index, end_row, base_row) \
  vt_screen_get_line_region((term)->screen, beg_row, end_char_index, end_row, base_row)

#define vt_term_get_word_region(term, beg_char_index, beg_row, end_char_index, end_row,       \
                                base_char_index, base_row)                                    \
  vt_screen_get_word_region((term)->screen, beg_char_index, beg_row, end_char_index, end_row, \
                            base_char_index, base_row)

#define vt_term_write_content(term, fd, conv, clear_at_end, area) \
  vt_screen_write_content(term->screen, fd, conv, clear_at_end, area)

#define vt_term_cursor_is_rtl(term) vt_screen_cursor_is_rtl((term)->screen)

#define vt_term_set_use_multi_col_char(term, flag) \
  vt_parser_set_use_multi_col_char((term)->parser, flag)

#define vt_term_is_using_multi_col_char(term) \
  vt_parser_is_using_multi_col_char((term)->parser)

#define vt_term_get_mouse_report_mode(term) vt_parser_get_mouse_report_mode((term)->parser)

#define vt_term_want_focus_event(term) vt_parser_want_focus_event((term)->parser)

#define vt_term_set_alt_color_mode(term, mode) \
  vt_parser_set_alt_color_mode((term)->parser, mode)

#define vt_term_get_alt_color_mode(term) vt_parser_get_alt_color_mode((term)->parser)

int vt_term_set_icon_path(vt_term_t *term, const char *path);

#define vt_term_window_name(term) vt_get_window_name((term)->parser)

#define vt_term_icon_name(term) vt_get_icon_name((term)->parser)

#define vt_term_icon_path(term) ((term)->icon_path)

#define vt_term_get_uri(term) ((term)->uri)

#define vt_term_is_bracketed_paste_mode(term) \
  vt_parser_is_bracketed_paste_mode((term)->parser)

#define vt_term_set_unicode_policy(term, policy) \
  vt_parser_set_unicode_policy((term)->parser, policy)

#define vt_term_get_unicode_policy(term) vt_parser_get_unicode_policy((term)->parser)

void vt_term_set_bidi_separators(vt_term_t *term, const char *bidi_separators);

#define vt_term_get_cmd_line(term) vt_pty_get_cmd_line((term)->pty)

#define vt_term_start_config_menu(term, cmd_path, x, y, display) \
  vt_reset_pending_vt100_sequence((term)->parser);               \
  vt_start_config_menu((term)->pty, cmd_path, x, y, display)

int vt_term_get_config(vt_term_t *term, vt_term_t *output, char *key, int to_menu, int *flag);

int vt_term_set_config(vt_term_t *term, char *key, char *value);

#define vt_term_exec_cmd(term, cmd) vt_parser_exec_cmd((term)->parser, cmd)

#define vt_term_report_mouse_tracking(term, col, row, button, is_released, key_state,             \
                                      button_state)                                               \
  vt_parser_report_mouse_tracking((term)->parser, col, row, button, is_released, key_state, \
                                        button_state)

#define vt_term_search_init(term, match) vt_screen_search_init((term)->screen, match)

#define vt_term_search_final(term) vt_screen_search_final((term)->screen)

#define vt_term_search_reset_position(term) vt_screen_search_reset_position((term)->screen)

#define vt_term_search_find(term, beg_char_index, beg_row, end_char_index, end_row, regex,       \
                            backward)                                                            \
  vt_screen_search_find((term)->screen, beg_char_index, beg_row, end_char_index, end_row, regex, \
                        backward)

#define vt_term_blink(term) vt_screen_blink((term)->screen)

#define vt_term_get_user_data(term, key) ((term)->user_data)

#define vt_term_set_user_data(term, key, val) ((term)->user_data = (val))

#define vt_term_select_drcs(term) vt_parser_select_drcs((term)->parser)

#define vt_term_is_visible_cursor(term) vt_parser_is_visible_cursor((term)->parser)

#define vt_term_get_cursor_style(term) vt_parser_get_cursor_style((term)->parser)

#endif
