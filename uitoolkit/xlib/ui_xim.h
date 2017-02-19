/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_XIM_H__
#define __UI_XIM_H__

#include <mef/ef_parser.h>
#include <vt_char_encoding.h>

#include "../ui_window.h"

typedef struct ui_xim {
  XIM im;

  char *name;
  char *locale;

  ef_parser_t *parser;
  vt_char_encoding_t encoding;

  ui_window_t **xic_wins;
  u_int num_of_xic_wins;

} ui_xim_t;

int ui_xim_init(int use_xim);

int ui_xim_final(void);

int ui_xim_display_opened(Display *display);

int ui_xim_display_closed(Display *display);

int ui_add_xim_listener(ui_window_t *win, char *xim_name, char *xim_locale);

int ui_remove_xim_listener(ui_window_t *win);

XIMStyle ui_xim_get_style(ui_window_t *win);

XIC ui_xim_create_ic(ui_window_t *win, XIMStyle selected_style, XVaNestedList preedit_attr);

char *ui_get_xim_name(ui_window_t *win);

char *ui_get_xim_locale(ui_window_t *win);

char *ui_get_default_xim_name(void);

#endif
