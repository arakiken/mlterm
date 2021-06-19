/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_xic.h"

#include <pobl/bl_debug.h>

/* --- static variables --- */

static int is_ascii_input;

/* --- global functions --- */

int ui_xic_activate(ui_window_t *win, char *xim_name, char *xim_locale) { return 1; }

int ui_xic_deactivate(ui_window_t *win) { return 1; }

char *ui_xic_get_xim_name(ui_window_t *win) { return ""; }

char *ui_xic_get_default_xim_name(void) { return ""; }

int ui_xic_fg_color_changed(ui_window_t *win) { return 0; }

int ui_xic_bg_color_changed(ui_window_t *win) { return 0; }

int ui_xic_font_set_changed(ui_window_t *win) { return 0; }

int ui_xic_resized(ui_window_t *win) { return 0; }

int ui_xic_set_spot(ui_window_t *win) { return 0; }

size_t ui_xic_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                      KeySym *keysym, XKeyEvent *event) {
  return 0;
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

int ui_xic_is_active(ui_window_t *win) { return is_ascii_input == 0; }

int ui_xic_switch_mode(ui_window_t *win) {
  JNIEnv *env;
  jobject this;

  if ((*win->disp->display->app->activity->vm)->GetEnv(win->disp->display->app->activity->vm,
                                                       &env, JNI_VERSION_1_6) != JNI_OK) {
    return 0;
  }

  this = win->disp->display->app->activity->clazz;
  if (is_ascii_input) {
    (*env)->CallVoidMethod(env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                                          "normalInput", "()V"));
    is_ascii_input = 0;
  } else {
    (*env)->CallVoidMethod(env, this, (*env)->GetMethodID(env, (*env)->GetObjectClass(env, this),
                                                          "asciiInput", "()V"));
    is_ascii_input = 1;
  }
}

#if 0

/*
 * ui_xim.c <-> ui_xic.c communication functions
 * Not necessary in fb.
 */

int x_xim_activated(ui_window_t *win) { return 1; }

int x_xim_destroyed(ui_window_t *win) { return 1; }

#endif
