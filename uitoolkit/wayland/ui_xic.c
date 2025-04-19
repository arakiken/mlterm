/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_xic.h"

#include <string.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* malloc */

#include "vt_char_encoding.h"
#include "ui_display.h"

/* --- global functions --- */

int ui_xic_activate(ui_window_t *win, char *xim_name, char *xim_locale) {
  if (win->xic) {
    /* already activated */

    return 0;
  }

  if (strcmp(xim_name, "unused") == 0) {
    return 0;
  }

  if ((win->xic = malloc(sizeof(ui_xic_t))) == NULL) {
    return 0;
  }

  if ((win->xic->parser = vt_char_encoding_parser_new(VT_UTF8)) == NULL) {
    free(win->xic);
    win->xic = NULL;

    return 0;
  }

  ui_xic_font_set_changed(win);

  ui_display_set_use_text_input(win->disp, 1);

  return 1;
}

int ui_xic_deactivate(ui_window_t *win) {
  if (win->xic == NULL) {
    /* already deactivated */

    return 0;
  }

  ui_display_set_use_text_input(win->disp, 0);

  (*win->xic->parser->destroy)(win->xic->parser);
  free(win->xic);
  win->xic = NULL;

  return 1;
}

char *ui_xic_get_xim_name(ui_window_t *win) { return ""; }

char *ui_xic_get_default_xim_name(void) { return ""; }

int ui_xic_fg_color_changed(ui_window_t *win) { return 0; }

int ui_xic_bg_color_changed(ui_window_t *win) { return 0; }

int ui_xic_font_set_changed(ui_window_t *win) { return 0; }

int ui_xic_resized(ui_window_t *win) { return 0; }

int ui_xic_set_spot(ui_window_t *win) {
  int x;
  int y;

  if (win->xic == NULL || win->xim_listener == NULL || win->xim_listener->get_spot == NULL ||
      (*win->xim_listener->get_spot)(win->xim_listener->self, &x, &y) == 0) {
    return 0;
  }

  ui_display_set_text_input_spot(win->disp, x, y);

  return 1;
}

size_t ui_xic_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                      KeySym *keysym, XKeyEvent *event) {
  if (win->xic == NULL || event->utf8 == NULL) {
    *parser = NULL;

    return 0;
  } else {
    size_t len = strlen(event->utf8);

    if (len <= seq_len) {
      memcpy(seq, event->utf8, seq_len);
      *parser = win->xic->parser;
    }

    return len;
  }
}

size_t ui_xic_get_utf8_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                           KeySym *keysym, XKeyEvent *event) {
  return 0;
}

int ui_xic_filter_event(ui_window_t *win, /* Should be root window. */
                        XEvent *event) {
  return 0;
}

int ui_xic_set_focus(ui_window_t *win) { return 1; }

int ui_xic_unset_focus(ui_window_t *win) { return 1; }

int ui_xic_is_active(ui_window_t *win) { return 0; }

int ui_xic_switch_mode(ui_window_t *win) { return 0; }

#if 0

/*
 * ui_xim.c <-> ui_xic.c communication functions
 * Not necessary in fb.
 */

int ui_xim_activated(ui_window_t *win) { return 1; }

int ui_xim_destroyed(ui_window_t *win) { return 1; }

#endif
