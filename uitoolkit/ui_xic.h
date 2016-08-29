/*
 *	$Id$
 */

#ifndef __UI_XIC_H__
#define __UI_XIC_H__

#include <pobl/bl_types.h> /* size_t */

#if defined(USE_WIN32GUI) || defined(USE_QUARTZ)
#include <vt_char_encoding.h>
#endif

#include "ui.h"
#include "ui_window.h"

typedef struct ui_xic {
  XIC ic;

#if defined(USE_WIN32GUI)
  WORD prev_keydown_wparam;
  ef_parser_t* parser;
#elif defined(USE_QUARTZ)
  ef_parser_t* parser;
#else
  XFontSet fontset;
  XIMStyle style;
#endif

} ui_xic_t;

int ui_xic_activate(ui_window_t* win, char* name, char* locale);

int ui_xic_deactivate(ui_window_t* win);

char* ui_xic_get_xim_name(ui_window_t* win);

char* ui_xic_get_default_xim_name(void);

int ui_xic_fg_color_changed(ui_window_t* win);

int ui_xic_bg_color_changed(ui_window_t* win);

int ui_xic_font_set_changed(ui_window_t* win);

int ui_xic_resized(ui_window_t* win);

int ui_xic_set_spot(ui_window_t* win);

size_t ui_xic_get_str(ui_window_t* win, u_char* seq, size_t seq_len, ef_parser_t** parser,
                      KeySym* keysym, XKeyEvent* event);

size_t ui_xic_get_utf8_str(ui_window_t* win, u_char* seq, size_t seq_len, ef_parser_t** parser,
                           KeySym* keysym, XKeyEvent* event);

int ui_xic_filter_event(ui_window_t* win, XEvent* event);

int ui_xic_set_focus(ui_window_t* win);

int ui_xic_unset_focus(ui_window_t* win);

int ui_xic_is_active(ui_window_t* win);

int ui_xic_switch_mode(ui_window_t* win);

int ui_xim_activated(ui_window_t* win);

int ui_xim_destroyed(ui_window_t* win);

#endif
