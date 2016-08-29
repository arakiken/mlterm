/*
 *	$Id$
 */

#include "../ui_xic.h"

#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* bl_str_alloca_dup */
#include <pobl/bl_mem.h>    /* malloc */
#include <pobl/bl_locale.h> /* bl_get_locale */

#ifdef UTF16_IME_CHAR
#include <mef/ef_utf16_parser.h>
#endif

#define HAS_XIM_LISTENER(win, function) ((win)->xim_listener && (win)->xim_listener->function)

/* --- static functions --- */

static int get_spot(ui_window_t* win, XPoint* spot) {
  int x;
  int y;

  if (!HAS_XIM_LISTENER(win, get_spot) ||
      win->xim_listener->get_spot(win->xim_listener->self, &x, &y) == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " xim_listener->get_spot() failed.\n");
#endif

    return 0;
  }

  spot->x = x + win->hmargin;
  spot->y = y + win->vmargin;

  if (win->parent && GetFocus() == ui_get_root_window(win)->my_window) {
    spot->x += win->x;
    spot->y += win->y;
  }

  return 1;
}

/* --- global functions --- */

int ui_xic_activate(ui_window_t* win, char* xim_name, char* xim_locale) {
  if (win->xic) {
    /* already activated */

    return 0;
  }

  if ((win->xic = malloc(sizeof(ui_xic_t))) == NULL) {
    return 0;
  }

#ifndef UTF16_IME_CHAR
  if ((win->xic->parser = vt_char_encoding_parser_new(vt_get_char_encoding(bl_get_codeset_win32()))) == NULL)
#else
  /* UTF16LE => UTF16BE in ui_xic_get_str. */
  if ((win->xic->parser = ef_utf16_parser_new()) == NULL)
#endif
  {
    free(win->xic);
    win->xic = NULL;

    return 0;
  }

  win->xic->ic = ImmGetContext(win->my_window);
  win->xic->prev_keydown_wparam = 0;

  ui_xic_font_set_changed(win);

  return 1;
}

int ui_xic_deactivate(ui_window_t* win) {
  if (win->xic == NULL) {
    /* already deactivated */

    return 0;
  }

  ImmReleaseContext(win->my_window, win->xic->ic);
  (*win->xic->parser->delete)(win->xic->parser);
  free(win->xic);
  win->xic = NULL;

  return 1;
}

char* ui_xic_get_xim_name(ui_window_t* win) { return ""; }

char* ui_xic_get_default_xim_name(void) { return ""; }

int ui_xic_fg_color_changed(ui_window_t* win) { return 0; }

int ui_xic_bg_color_changed(ui_window_t* win) { return 0; }

int ui_xic_font_set_changed(ui_window_t* win) {
  if (win->xic && HAS_XIM_LISTENER(win, get_fontset)) {
    if (ImmSetCompositionFont(win->xic->ic,
                              (*win->xim_listener->get_fontset)(win->xim_listener->self))) {
      return 1;
    }
  }

  return 0;
}

int ui_xic_resized(ui_window_t* win) { return 0; }

int ui_xic_set_spot(ui_window_t* win) {
  XPoint spot;
  COMPOSITIONFORM cf;

  if (win->xic == NULL ||
      /*
       * Multiple windows can share the same input context, so windows except
       * the focused one don't call ImmSetCompositionWindow().
       */
      !win->is_focused) {
    return 0;
  }

  if (get_spot(win, &spot) == 0) {
    return 0;
  }

  cf.ptCurrentPos = spot;
  cf.dwStyle = CFS_FORCE_POSITION;

  return ImmSetCompositionWindow(win->xic->ic, &cf);
}

size_t ui_xic_get_str(ui_window_t* win, u_char* seq, size_t seq_len, ef_parser_t** parser,
                      KeySym* keysym, XKeyEvent* event) {
  size_t len;

  if (win->xic == NULL) {
    goto zero_return;
  }

  *keysym = win->xic->prev_keydown_wparam;

  if ('a' <= *keysym && *keysym <= VK_F24) {
    /*
     * Avoid to conflict 'a' - 'z' with VK_NUMPAD1..9,
     * VK_MULTIPLY..VK_DIVIDE, VK_F1..VK_F11.
     * (0x61 - 0x7a)
     */
    *keysym += 0xff00;
  }

  win->xic->prev_keydown_wparam = 0;

  if (seq_len == 0 || event->ch == 0) {
    goto zero_return;
  } else if ((event->state & ShiftMask) && *keysym == XK_ISO_Left_Tab) {
    goto zero_return;
  } else if (event->state & ControlMask) {
    if (event->ch == '2' || event->ch == ' ' || event->ch == '@') {
      event->ch = 0;
    } else if ('3' <= event->ch && event->ch <= '7') {
      /* '3' => 0x1b  '4' => 0x1c  '5' => 0x1d  '6' => 0x1e  '7' => 0x1f */
      event->ch -= 0x18;
    } else if (event->ch == '8') {
      event->ch = 0x7f;
    } else if (event->ch == '0' || event->ch == '1' || event->ch == '9') {
      /* For modifyOtherKeys */
      goto zero_return;
    } else if (event->ch == '^') {
      event->ch = 0x1d;
    } else if (event->ch == '_' || event->ch == '/') {
      event->ch = 0x1f;
    }
  }

#ifndef UTF16_IME_CHAR
  len = 1;
  if (event->ch > 0xff) {
    *(seq++) = (char)((event->ch >> 8) & 0xff);

    if (seq_len == 1) {
      goto zero_return;
    }

    len++;
  }

  *seq = (char)(event->ch & 0xff);
#else
  if (seq_len == 1) {
    goto zero_return;
  }

  *(seq++) = (char)((event->ch >> 8) & 0xff);
  *seq = (char)(event->ch & 0xff);
  len = 2;
#endif

  /* wparam doesn't tell upper case from lower case. */
  if ('A' <= *keysym && *keysym <= 'Z') {
    if (event->ch < 'A' || 'Z' < event->ch) {
      /* Upper to Lower case */
      *keysym += 0x20;
    }
  }

  *parser = win->xic->parser;

  return len;

zero_return:
  *parser = NULL;

  return 0;
}

size_t ui_xic_get_utf8_str(ui_window_t* win, u_char* seq, size_t seq_len, ef_parser_t** parser,
                           KeySym* keysym, XKeyEvent* event) {
  return 0;
}

int ui_xic_filter_event(ui_window_t* win, /* Should be root window. */
                        XEvent* event) {
  u_int count;

  if (event->msg != WM_KEYDOWN) {
    return 0;
  }

  for (count = 0; count < win->num_of_children; count++) {
    ui_xic_filter_event(win->children[count], event);
  }

  if (!win->xic) {
    return 0;
  }

  win->xic->prev_keydown_wparam = event->wparam;

  return 1;
}

int ui_xic_set_focus(ui_window_t* win) {
  /* The composition font can be changed by the connection dialog box. */
  ui_xic_font_set_changed(win);

  return 1;
}

int ui_xic_unset_focus(ui_window_t* win) { return 1; }

int ui_xic_is_active(ui_window_t* win) {
  if (win->xic == NULL) {
    return 0;
  }

  return ImmGetOpenStatus(win->xic->ic);
}

int ui_xic_switch_mode(ui_window_t* win) {
  if (win->xic == NULL) {
    return 0;
  }

  return ImmSetOpenStatus(win->xic->ic, (ImmGetOpenStatus(win->xic->ic) == FALSE));
}

#if 0

/*
 * ui_xim.c <-> ui_xic.c communication functions
 * Not necessary in win32.
 */

int ui_xim_activated(ui_window_t* win) { return 1; }

int ui_xim_destroyed(ui_window_t* win) { return 1; }

#endif
