/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_xic.h"

#include <X11/Xutil.h> /* X{mb|utf8}LookupString */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* bl_str_alloca_dup */
#include <pobl/bl_mem.h>    /* malloc */
#include <pobl/bl_locale.h> /* bl_get_locale */
#include <mef/ef_utf8_parser.h>

#include "ui_xim.h" /* refering mutually */

#define HAS_XIM_LISTENER(win, function) ((win)->xim_listener && (win)->xim_listener->function)

/* for XIMPreeditArea, XIMStatusArea */
#if 0
#define SET_XNAREA_ATTR
#endif

/* --- static variables --- */

static ef_parser_t *utf8_parser;

/* --- static functions --- */

#ifdef SET_XNAREA_ATTR
static void get_rect(ui_window_t *win, XRectangle *rect) {
  rect->x = 0;
  rect->y = 0;
  rect->width = ACTUAL_WIDTH(win);
  rect->height = ACTUAL_HEIGHT(win);
}
#endif

static int get_spot(ui_window_t *win, XPoint *spot) {
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
  spot->y = y /* + win->vmargin */;

  return 1;
}

static XFontSet load_fontset(ui_window_t *win) {
  char *cur_locale;
  XFontSet fontset;

  cur_locale = bl_str_alloca_dup(bl_get_locale());
  if (bl_locale_init(ui_get_xim_locale(win))) {
    if (!HAS_XIM_LISTENER(win, get_fontset)) {
      return NULL;
    }

    fontset = (*win->xim_listener->get_fontset)(win->xim_listener->self);

    /* restoring */
    bl_locale_init(cur_locale);
  } else {
    fontset = NULL;
  }

  return fontset;
}

static int destroy_xic(ui_window_t *win) {
  if (!win->xic) {
    return 0;
  }

  XDestroyIC(win->xic->ic);

  if (win->xic->fontset) {
    XFreeFontSet(win->disp->display, win->xic->fontset);
  }

  free(win->xic);
  win->xic = NULL;

  return 1;
}

static int create_xic(ui_window_t *win) {
  XIMStyle selected_style;
  XVaNestedList preedit_attr;
  XRectangle rect;
  XPoint spot;
  XFontSet fontset;
  XIC xic;
  long xim_ev_mask;

  if (win->xic) {
    /* already created */

    return 0;
  }

  if ((selected_style = ui_xim_get_style(win)) == 0) {
    return 0;
  }

  if (selected_style & XIMPreeditPosition) {
    /*
     * over the spot style.
     */

#ifdef SET_XNAREA_ATTR
    get_rect(win, &rect);
#endif
    if (get_spot(win, &spot) == 0) {
      /* forcibly set ... */
      spot.x = 0;
      spot.y = 0;
    }

    if (!(fontset = load_fontset(win))) {
      return 0;
    }

    if ((preedit_attr =
         XVaCreateNestedList(0,
                             /*
                              * Some input methods use XNArea to show lookup table,
                              * setting XNArea as follows shows lookup table at unexpected position.
                              */
#ifdef SET_XNAREA_ATTR
                             XNArea, &rect,
#endif
                             XNSpotLocation, &spot,
                             XNForeground,
                             (*win->xim_listener->get_fg_color)(win->xim_listener->self)->pixel,
                             XNBackground,
                             (*win->xim_listener->get_bg_color)(win->xim_listener->self)->pixel,
                             XNFontSet, fontset, NULL)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " XVaCreateNestedList() failed.\n");
#endif

      XFreeFontSet(win->disp->display, fontset);

      return 0;
    }

    if ((xic = ui_xim_create_ic(win, selected_style, preedit_attr)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " XCreateIC() failed\n");
#endif

      XFree(preedit_attr);
      XFreeFontSet(win->disp->display, fontset);

      return 0;
    }

    XFree(preedit_attr);
  } else {
    /*
     * root style
     */

    if ((xic = ui_xim_create_ic(win, selected_style, NULL)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " XCreateIC() failed\n");
#endif

      return 0;
    }

    fontset = NULL;
  }

  if ((win->xic = malloc(sizeof(ui_xic_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    if (fontset) {
      XFreeFontSet(win->disp->display, fontset);
    }

    return 0;
  }

  win->xic->ic = xic;
  win->xic->fontset = fontset;
  win->xic->style = selected_style;

  xim_ev_mask = 0;

  XGetICValues(win->xic->ic, XNFilterEvents, &xim_ev_mask, NULL);
  ui_window_add_event_mask(win, xim_ev_mask);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " XIC activated.\n");
#endif

  return 1;
}

/* --- global functions --- */

int ui_xic_activate(ui_window_t *win, char *xim_name, char *xim_locale) {
  if (win->xic) {
    /* already activated */

    return 0;
  }

  return ui_add_xim_listener(win, xim_name, xim_locale);
}

int ui_xic_deactivate(ui_window_t *win) {
  if (win->xic == NULL) {
    /* already deactivated */

    return 0;
  }

#if 0
  {
    /*
     * this should not be done.
     */
    int xim_ev_mask;

    XGetICValues(win->xic->ic, XNFilterEvents, &xim_ev_mask, NULL);
    ui_window_remove_event_mask(win, xim_ev_mask);
  }
#endif

  destroy_xic(win);

  if (!ui_remove_xim_listener(win)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_remove_xim_listener() failed.\n");
#endif
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " XIC deactivated.\n");
#endif

  return 1;
}

char *ui_xic_get_xim_name(ui_window_t *win) { return ui_get_xim_name(win); }

char *ui_xic_get_default_xim_name(void) { return ui_get_default_xim_name(); }

int ui_xic_fg_color_changed(ui_window_t *win) {
  XVaNestedList preedit_attr;

  if (win->xic == NULL || !(win->xic->style & XIMPreeditPosition)) {
    return 0;
  }

  if ((preedit_attr = XVaCreateNestedList(
           0, XNForeground, (*win->xim_listener->get_fg_color)(win->xim_listener->self)->pixel,
           NULL)) == NULL) {
    return 0;
  }

  XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL);

  XFree(preedit_attr);

  return 1;
}

int ui_xic_bg_color_changed(ui_window_t *win) {
  XVaNestedList preedit_attr;

  if (win->xic == NULL || !(win->xic->style & XIMPreeditPosition)) {
    return 0;
  }

  if ((preedit_attr = XVaCreateNestedList(
           0, XNBackground, (*win->xim_listener->get_bg_color)(win->xim_listener->self)->pixel,
           NULL)) == NULL) {
    return 0;
  }

  XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL);

  XFree(preedit_attr);

  return 1;
}

int ui_xic_font_set_changed(ui_window_t *win) {
  XVaNestedList preedit_attr;
  XFontSet fontset;

  if (win->xic == NULL || !(win->xic->style & XIMPreeditPosition)) {
    return 0;
  }

  if (!(fontset = load_fontset(win))) {
    return 0;
  }

  if ((preedit_attr = XVaCreateNestedList(0, XNFontSet, fontset, NULL)) == NULL) {
    XFreeFontSet(win->disp->display, fontset);

    return 0;
  }

  XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL);

  XFree(preedit_attr);

  XFreeFontSet(win->disp->display, win->xic->fontset);
  win->xic->fontset = fontset;

  return 1;
}

int ui_xic_resized(ui_window_t *win) {
  XVaNestedList preedit_attr;
  XRectangle rect;
  XPoint spot;

  if (win->xic == NULL || !(win->xic->style & XIMPreeditPosition)) {
    return 0;
  }

#ifdef SET_XNAREA_ATTR
  get_rect(win, &rect);
#endif
  if (get_spot(win, &spot) == 0) {
    /* forcibly set ...*/
    spot.x = 0;
    spot.y = 0;
  }

  if ((preedit_attr = XVaCreateNestedList(0,
#ifdef SET_XNAREA_ATTR
                                          XNArea, &rect,
#endif
                                          XNSpotLocation, &spot, NULL)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " XvaCreateNestedList() failed.\n");
#endif

    return 0;
  }

  XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL);

  XFree(preedit_attr);

  return 1;
}

int ui_xic_set_spot(ui_window_t *win) {
  XVaNestedList preedit_attr;
  XPoint spot;

  if (win->xic == NULL || !(win->xic->style & XIMPreeditPosition)) {
    return 0;
  }

  if (get_spot(win, &spot) == 0) {
    /* XNSpotLocation not changed */

    return 0;
  }

  if ((preedit_attr = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " XvaCreateNestedList failed.\n");
#endif

    return 0;
  }

  XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL);

  XFree(preedit_attr);

  return 1;
}

size_t ui_xic_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                      KeySym *keysym, XKeyEvent *event) {
  Status stat;
  size_t len;

  if (win->xic == NULL) {
    return 0;
  }

  if ((len = XmbLookupString(win->xic->ic, event, seq, seq_len, keysym, &stat)) == 0) {
    return 0;
  } else if (stat == XBufferOverflow /* len > seq_len */) {
    /*
     * Input string is too large for seq. seq and keysym are not modified.
     * len is required size for input string.
     */
    return len;
  }

  if (IS_ENCODING_BASED_ON_ISO2022(win->xim->encoding) && *seq < 0x20) {
    /*
     * XXX hack
     * control char(except delete char[0x7f]) is received.
     * in afraid of it being parsed as part of iso2022 sequence ,
     * *parser is set NULL.
     */
    *parser = NULL;
  } else {
    *parser = win->xim->parser;
  }

  return len;
}

size_t ui_xic_get_utf8_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                           KeySym *keysym, XKeyEvent *event) {
#ifdef HAVE_XUTF8_LOOKUP_STRING
  Status stat;
  size_t len;

  if (win->xic == NULL) {
    return 0;
  }

  if ((len = Xutf8LookupString(win->xic->ic, event, seq, seq_len, keysym, &stat)) == 0) {
    return 0;
  } else if (stat == XBufferOverflow /* len > seq_len */) {
    /*
     * Input string is too large for seq. seq and keysym are not modified.
     * len is required size for input string.
     */
    return len;
  }

  if (!utf8_parser) {
    utf8_parser = ef_utf8_parser_new();
  }

  *parser = utf8_parser;

  return len;
#else
  return 0;
#endif
}

int ui_xic_set_focus(ui_window_t *win) {
  if (!win->xic) {
    return 0;
  }

  XSetICFocus(win->xic->ic);

  return 1;
}

int ui_xic_unset_focus(ui_window_t *win) {
  if (!win->xic) {
    return 0;
  }

  XUnsetICFocus(win->xic->ic);

  return 1;
}

int ui_xic_is_active(ui_window_t *win) {
#ifdef XNPreeditState
  XIMPreeditState preedit_state;
  XVaNestedList preedit_attr;
  int res;

  if (!win->xic) {
    return 0;
  }

  preedit_attr = XVaCreateNestedList(0, XNPreeditState, &preedit_state, NULL);

  if (XGetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL) != NULL) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " XIM doesn't support XNPreeditState.\n");
#endif

    res = 0;
  } else {
    res = (preedit_state == XIMPreeditEnable);

#ifdef DEBUG
    if (res) {
      bl_debug_printf(BL_DEBUG_TAG " XIM is enabled.\n");
    } else {
      bl_debug_printf(BL_DEBUG_TAG " XIM is disabled.\n");
    }
#endif
  }

  XFree(preedit_attr);

  return res;
#else
  return 0;
#endif
}

int ui_xic_switch_mode(ui_window_t *win) {
#ifdef XNPreeditState
  XVaNestedList preedit_attr;

  if (!win->xic) {
    return 0;
  }

  preedit_attr = XVaCreateNestedList(
      0, XNPreeditState, ui_xic_is_active(win) ? XIMPreeditDisable : XIMPreeditEnable, NULL);

  if (XSetICValues(win->xic->ic, XNPreeditAttributes, preedit_attr, NULL) != NULL) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " XIM doesn't support XNPreeditState.\n");
#endif
  }

  XFree(preedit_attr);

  return 1;
#else
  return 0;
#endif
}

/*
 * ui_xim.c <-> ui_xic.c communication functions
 */

int ui_xim_activated(ui_window_t *win) { return create_xic(win); }

int ui_xim_destroyed(ui_window_t *win) { return destroy_xic(win); }
