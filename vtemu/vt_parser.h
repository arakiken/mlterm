/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_VT100_PARSER_H__
#define __VT_VT100_PARSER_H__

#include <pobl/bl_types.h> /* u_xxx */
#include <mef/ef_parser.h>
#include <mef/ef_conv.h>

#include "vt_pty.h"
#include "vt_screen.h"
#include "vt_char_encoding.h"
#include "vt_drcs.h"
#include "vt_termcap.h"
#include "vt_config_proto.h"

#define PTY_WR_BUFFER_SIZE 100

#if 0
#define ENABLE_OSC5379PICTURE
#endif

/*
 * Possible patterns are:
 *  NOT_USE_UNICODE_FONT(0x1)
 *  USE_UNICODE_PROPERTY(0x2)
 *  NOT_USE_UNICODE_FONT|USE_UNICODE_PROPERTY(0x3)
 *  ONLY_USE_UNICODE_FONT(0x4)
 */
typedef enum vt_unicode_policy {
  NO_UNICODE_POLICY = 0x0,
  NOT_USE_UNICODE_FONT = 0x1,
  ONLY_USE_UNICODE_FONT = 0x2,
  NOT_USE_UNICODE_BOXDRAW_FONT = 0x4,
  ONLY_USE_UNICODE_BOXDRAW_FONT = 0x8,
  USE_UNICODE_DRCS = 0x10,
  CONVERT_UNICODE_TO_ISCII = 0x20,

  UNICODE_POLICY_MAX

} vt_unicode_policy_t;

typedef enum vt_mouse_report_mode {
  NO_MOUSE_REPORT = 0,

  MOUSE_REPORT = 0x1,
  BUTTON_EVENT_MOUSE_REPORT = 0x2,
  ANY_EVENT_MOUSE_REPORT = 0x3,
  LOCATOR_CHARCELL_REPORT = 0x4,
  LOCATOR_PIXEL_REPORT = 0x5,

} vt_mouse_report_mode_t;

typedef enum vt_extended_mouse_report_mode {
  NO_EXTENDED_MOUSE_REPORT = 0,

  EXTENDED_MOUSE_REPORT_UTF8 = 0x1,
  EXTENDED_MOUSE_REPORT_SGR = 0x2,
  EXTENDED_MOUSE_REPORT_URXVT = 0x3,

} vt_extended_mouse_report_mode_t;

typedef enum vt_locator_report_mode {
  LOCATOR_BUTTON_DOWN = 0x1,
  LOCATOR_BUTTON_UP = 0x2,
  LOCATOR_ONESHOT = 0x4,
  LOCATOR_REQUEST = 0x8,
  LOCATOR_FILTER_RECT = 0x10,

} vt_locator_report_mode_t;

typedef enum vt_alt_color_mode {
  ALT_COLOR_BOLD = 0x1,
  ALT_COLOR_ITALIC = 0x2,
  ALT_COLOR_UNDERLINE = 0x4,
  ALT_COLOR_BLINKING = 0x8,
  ALT_COLOR_CROSSED_OUT = 0x10,

} vt_alt_color_mode_t;

typedef enum vt_cursor_style {
  CS_BLOCK = 0x0,
  CS_UNDERLINE = 0x1,
  CS_BAR = 0x2,
  CS_BLINK = 0x4,
} vt_cursor_style_t;

typedef struct vt_write_buffer {
  vt_char_t chars[PTY_WR_BUFFER_SIZE];
  u_int filled_len;

  /* for "CSI b"(REP) sequence */
  vt_char_t *last_ch;

  int (*output_func)(vt_screen_t *, vt_char_t *chars, u_int);

} vt_write_buffer_t;

typedef struct vt_read_buffer {
  u_char *chars;
  size_t len;
  size_t filled_len;
  size_t left;
  size_t new_len;

} vt_read_buffer_t;

typedef struct vt_xterm_event_listener {
  void *self;

  void (*start)(void *);     /* called in *visual* context. (Note that not logical) */
  void (*stop)(void *);      /* called in visual context. */
  void (*interrupt)(void *); /* called in visual context. */

  void (*resize)(void *, u_int, u_int, int);                   /* called in visual context. */
  void (*reverse_video)(void *, int);                          /* called in visual context. */
  void (*set_mouse_report)(void *);                            /* called in visual context. */
  void (*request_locator)(void *);                             /* called in visual context. */
  void (*set_window_name)(void *, u_char *);                   /* called in logical context. */
  void (*set_icon_name)(void *, u_char *);                     /* called in logical context. */
  void (*bel)(void *);                                         /* called in visual context. */
  int (*im_is_active)(void *);                                 /* called in logical context. */
  void (*switch_im_mode)(void *);                              /* called in logical context. */
  void (*set_selection)(void *, vt_char_t *, u_int, u_char *); /* called in logical context. */
  int (*get_window_size)(void *, u_int *, u_int *);            /* called in logical context. */
  int (*get_rgb)(void *, u_int8_t *, u_int8_t *, u_int8_t *,
                 vt_color_t); /* called in logical context. */
  vt_char_t *(*get_picture_data)(void *, char *, int *, int *, int *, int *,
                                 u_int32_t **, int, int);       /* called in logical context. */
  int (*get_emoji_data)(void *, vt_char_t *, vt_char_t *);      /* called in logical context. */
  void (*show_tmp_picture)(void *, char *);                     /* called in logical context. */
#ifdef ENABLE_OSC5379PICTURE
  void (*add_frame_to_animation)(void *, char *, int *, int *); /* called in logical context. */
#endif
  void (*hide_cursor)(void *, int);                             /* called in logical context. */
  int (*check_iscii_font)(void *, ef_charset_t);

} vt_xterm_event_listener_t;

/*
 * !! Notice !!
 * Validation of Keys and vals is not checked before these event called by
 * vt_parser.
 */
typedef struct vt_config_event_listener {
  void *self;

  /* Assume that exec, set and get affect each window. */
  int (*exec)(void *, char *);
  int (*set)(void *, char *, char *, char *);
  void (*get)(void *, char *, char *, int);

  /* Assume that saved, set_font and set_color affect all window. */
  void (*saved)(void); /* Event that mlterm/main file was changed. */

  void (*set_font)(void *, char *, char *, char *, int);
  void (*get_font)(void *, char *, char *, int);
  void (*set_color)(void *, char *, char *, char *, int);
  void (*get_color)(void *, char *, int);

} vt_config_event_listener_t;

typedef struct vt_parser *vt_parser_ptr_t;

typedef struct vt_storable_states {
  int is_saved : 1;

  int is_bold : 1;
  int is_italic : 1;
  int is_reversed : 1;
  int is_blinking : 1;
  int is_invisible : 1;
  int is_protected : 1;
  int is_relative_origin : 1;
  int is_auto_wrap : 1;

  /* vt_line_style_t */ int line_style : 7;
  vt_color_t fg_color;
  vt_color_t bg_color;
  ef_charset_t cs;

} vt_storable_states_t;

typedef struct vt_saved_names {
  char **names;
  u_int num;

} vt_saved_names_t;

typedef struct vt_macro {
  u_char *str;
  int8_t is_sixel;
  u_int8_t sixel_num;

} vt_macro_t;

typedef struct vt_parser {
  vt_read_buffer_t r_buf;
  vt_write_buffer_t w_buf;

  vt_pty_ptr_t pty;
  vt_pty_hook_t pty_hook;

  vt_screen_t *screen;
  vt_termcap_ptr_t termcap;

  ef_parser_t *cc_parser; /* char code parser */
  ef_conv_t *cc_conv;     /* char code converter */
  vt_char_encoding_t encoding;

  vt_color_t fg_color;
  vt_color_t bg_color;

  ef_charset_t cs;

  vt_xterm_event_listener_t *xterm_listener;
  vt_config_event_listener_t *config_listener;

  int log_file;

  char *win_name;
  char *icon_name;

  struct {
    u_int16_t top;
    u_int16_t left;
    u_int16_t bottom;
    u_int16_t right;

  } loc_filter;

  struct {
    u_int16_t flags;
    u_int8_t fg[16];
    u_int8_t bg[16];
  } alt_colors;

  /* for save/restore cursor */
  vt_storable_states_t saved_normal;
  vt_storable_states_t saved_alternate;

  vt_saved_names_t saved_win_names;
  vt_saved_names_t saved_icon_names;

  vt_drcs_t *drcs;

  vt_macro_t *macros;
  u_int num_macros;

  u_int32_t *sixel_palette;
  u_int64_t vtmode_flags;
  u_int64_t saved_vtmode_flags;

  /* vt_unicode_policy_t */ int8_t unicode_policy;

  /* vt_mouse_report_mode_t */ int8_t mouse_mode;
  /* vt_extended_mouse_report_mode_t */ int8_t ext_mouse_mode;
  /* vt_locator_report_mode_t */ int8_t locator_mode;

  /* vt_alt_color_mode_t */ int8_t alt_color_mode;
  u_int8_t col_size_of_width_a; /* 1 or 2 */
  /* vt_cursor_style_t */ int8_t cursor_style;
  /* vt_line_style_t */ int8_t line_style;
  int8_t modify_cursor_keys;
  int8_t modify_function_keys;
  int8_t modify_other_keys;

  /* Used for non iso2022 encoding */
  ef_charset_t gl;
  ef_charset_t g0;
  ef_charset_t g1;

  /* XXX Use (vt_parser->xxxxx ? 1 : 0) in copying these flags to int, otherwise int is -1. */
  int is_so : 1;

  int is_bold : 1;
  int is_italic : 1;
  int is_reversed : 1;
  int is_blinking : 1;
  int is_invisible : 1;

  int use_char_combining : 1;
  int use_multi_col_char : 1;
  int logging_vt_seq : 1;
  int is_app_keypad : 1;
  int is_app_cursor_keys : 1;
  int is_app_escape : 1;
  int is_bracketed_paste_mode : 1;
  int allow_deccolm : 1;
  int keep_screen_on_deccolm : 1;
  int want_focus_event : 1;
  int im_is_active : 1;
  int sixel_scrolling : 1;
  int cursor_to_right_of_sixel : 1;
  int yield : 1;
  int is_auto_encoding : 1;
  int use_auto_detect : 1;
  int is_visible_cursor : 1;
  int is_protected : 1;
  int ignore_broadcasted_chars : 1;
  int set_title_using_hex : 1;
  int get_title_using_hex : 1;
  int set_title_using_utf8 : 1;
  int get_title_using_utf8 : 1;
  int auto_cr : 1;
  int bold_affects_bg : 1;
  int use_ansi_colors : 1;

#ifdef USE_VT52
  int is_vt52_mode : 1;
#endif

} vt_parser_t;

void vt_set_use_alt_buffer(int use);

void vt_set_unicode_noconv_areas(char *areas);

void vt_set_full_width_areas(char *areas);

void vt_set_half_width_areas(char *areas);

void vt_set_use_ttyrec_format(int use);

#ifdef USE_LIBSSH2
void vt_set_use_scp_full(int use);
#else
#define vt_set_use_scp_full(use) (0)
#endif

void vt_set_timeout_read_pty(u_long timeout);

void vt_set_primary_da(char *da);

void vt_set_secondary_da(char *da);

#define vt_parser_init vt_config_proto_init

void vt_parser_final(void);

vt_parser_t *vt_parser_new(vt_screen_t *screen, vt_termcap_ptr_t termcap,
                           vt_char_encoding_t encoding, int is_auto_encoding,
                           int use_auto_detect, int logging_vt_seq,
                           vt_unicode_policy_t policy, u_int col_size_a,
                           int use_char_combining, int use_multi_col_char,
                           const char *win_name, const char *icon_name,
                           int use_ansi_colors, vt_alt_color_mode_t alt_color_mode,
                           vt_cursor_style_t cursor_style, int ignore_broadcasted_chars);

int vt_parser_delete(vt_parser_t *vt_parser);

void vt_parser_set_pty(vt_parser_t *vt_parser, vt_pty_ptr_t pty);

void vt_parser_set_xterm_listener(vt_parser_t *vt_parser,
                                        vt_xterm_event_listener_t *xterm_listener);

void vt_parser_set_config_listener(vt_parser_t *vt_parser,
                                         vt_config_event_listener_t *config_listener);

int vt_parse_vt100_sequence(vt_parser_t *vt_parser);

void vt_reset_pending_vt100_sequence(vt_parser_t *vt_parser);

int vt_parser_write_modified_key(vt_parser_t *vt_parser, int key, int modcode);

int vt_parser_write_special_key(vt_parser_t *vt_parser, vt_special_key_t key,
                                int modcode, int is_numlock);

/* Must be called in visual context. */
int vt_parser_write_loopback(vt_parser_t *vt_parser, const u_char *buf, size_t len);

/* Must be called in visual context. */
int vt_parser_show_message(vt_parser_t *vt_parser, char *msg);

#if 1 /* defined(__ANDROID__) || defined(__APPLE__) || defined(USE_SDL2) */
/* Must be called in visual context. */
int vt_parser_preedit(vt_parser_t *vt_parser, const u_char *buf, size_t len);
#endif

/* Must be called in visual context. */
int vt_parser_local_echo(vt_parser_t *vt_parser, const u_char *buf, size_t len);

int vt_parser_change_encoding(vt_parser_t *vt_parser, vt_char_encoding_t encoding);

#define vt_parser_get_encoding(vt_parser) ((vt_parser)->encoding)

size_t vt_parser_convert_to(vt_parser_t *vt_parser, u_char *dst, size_t len,
                                  ef_parser_t *parser);

void vt_init_encoding_parser(vt_parser_t *vt_parser);

void vt_init_encoding_conv(vt_parser_t *vt_parser);

#define vt_get_window_name(vt_parser) ((vt_parser)->win_name)

#define vt_get_icon_name(vt_parser) ((vt_parser)->icon_name)

#define vt_parser_set_use_char_combining(vt_parser, use) \
  ((vt_parser)->use_char_combining = (use))

#define vt_parser_is_using_char_combining(vt_parser) ((vt_parser)->use_char_combining)

#define vt_parser_is_using_multi_col_char(vt_parser) ((vt_parser)->use_multi_col_char)

#define vt_parser_get_mouse_report_mode(vt_parser) ((vt_parser)->mouse_mode)

#define vt_parser_is_bracketed_paste_mode(vt_parser) \
  ((vt_parser)->is_bracketed_paste_mode)

#define vt_parser_want_focus_event(vt_parser) ((vt_parser)->want_focus_event)

#define vt_parser_set_unicode_policy(vt_parser, policy) \
  ((vt_parser)->unicode_policy = (policy))

#define vt_parser_get_unicode_policy(vt_parser) ((vt_parser)->unicode_policy)

int vt_set_auto_detect_encodings(char *encodings);

int vt_convert_to_internal_ch(vt_parser_t *vt_parser, ef_char_t *ch);

#define vt_parser_select_drcs(vt_parser) vt_drcs_select((vt_parser)->drcs)

void vt_parser_set_alt_color_mode(vt_parser_t *vt_parser, vt_alt_color_mode_t mode);

#define vt_parser_get_alt_color_mode(vt_parser) ((vt_parser)->alt_color_mode)

void vt_set_broadcasting(int flag);

int vt_parser_is_broadcasting(vt_parser_t *vt_parser);

int true_or_false(const char *str);

int vt_parser_get_config(vt_parser_t *vt_parser, vt_pty_ptr_t output, char *key,
                               int to_menu, int *flag);

int vt_parser_set_config(vt_parser_t *vt_parser, char *key, char *val);

int vt_parser_exec_cmd(vt_parser_t *vt_parser, char *cmd);

void vt_parser_report_mouse_tracking(vt_parser_t *vt_parser, int col, int row,
                                           int button, int is_released, int key_state,
                                           int button_state);

#define vt_parser_is_visible_cursor(vt_parser) ((vt_parser)->is_visible_cursor)

#define vt_parser_get_cursor_style(vt_parser) ((vt_parser)->cursor_style)

#endif
