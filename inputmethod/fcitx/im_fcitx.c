/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>

#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE) || defined(USE_SDL2)
#define NO_XKB
#endif

#ifdef USE_FCITX5
#include <fcitx-gclient/fcitxgclient.h>
#ifdef NO_XKB
/* Don't include <fcitx-utils/macros.h> which includes cpp headers. */
#define _FCITX_UTILS_MACROS_H_
#define FCITX_C_DECL_BEGIN
#define FCITX_C_DECL_END
#include <fcitx-utils/keysymgen.h>
#endif
#else
#include <fcitx/frontend.h>
#include <fcitx-gclient/fcitxclient.h>
#endif

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>

#include <ui_im.h>

#include "../im_common.h"
#include "../im_info.h"

#define FCITX_ID -3

#ifdef USE_FCITX5
#define FcitxClient FcitxGClient
#define FcitxPreeditItem FcitxGPreeditItem

#define fcitx_client_focus_in(client) fcitx_g_client_focus_in(client)
#define fcitx_client_focus_out(client) fcitx_g_client_focus_out(client)
#define fcitx_client_process_key_sync fcitx_g_client_process_key_sync
#define fcitx_client_set_capacity(client, flag) fcitx_g_client_set_capability(client, flag)
#define fcitx_client_set_cursor_rect fcitx_g_client_set_cursor_rect
#define fcitx_client_close_ic(client)
#define fcitx_client_enable_ic(client)

/* See fcitx-utils/keysym.h */
#define FcitxKeyState_IgnoredMask (1 << 25)

/* The 5th argument of fcitx_g_client_process_key_sync() is 'isRelease' */
#define FCITX_PRESS_KEY (0)
#define FCITX_RELEASE_KEY (1)

/* See fcitx-utils/capabilityflags.h */
#define CAPACITY_CLIENT_SIDE_UI (1ull << 39) //(1 << 0)
#define CAPACITY_PREEDIT (1 << 1)
#define CAPACITY_CLIENT_SIDE_CONTROL_STATE (1 << 2)
#define CAPACITY_FORMATTED_PREEDIT (1 << 4)
#endif

#if 0
#define IM_FCITX_DEBUG 1
#endif

/*
 * fcitx doesn't support wayland, so the positioning of gtk-based candidate window of fcitx
 * doesn't work correctly on wayland.
 */
#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE) || defined(USE_WAYLAND) || defined(USE_SDL2)
#define KeyPress 2 /* see uitoolkit/fb/ui_display.h */
#define USE_IM_CANDIDATE_SCREEN
#endif

/* When fcitx encoding is the same as terminal, conv is NULL. */
#define NEED_TO_CONV(fcitx) ((fcitx)->conv)

typedef struct im_fcitx {
  /* input method common object */
  ui_im_t im;

  FcitxClient *client;

  vt_char_encoding_t term_encoding;

#ifdef USE_IM_CANDIDATE_SCREEN
  ef_parser_t *parser_term; /* for term encoding */
#endif
  ef_conv_t *conv; /* for term encoding */

  /*
   * Cache a result of fcitx_input_context_is_enabled() which uses
   * DBus connection internally.
   */
  gboolean is_enabled;

  XKeyEvent prev_key;

} im_fcitx_t;

/* --- static variables --- */

static int ref_count = 0;
static ef_parser_t *parser_utf8 = NULL;
static ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
#ifdef DEBUG_MODKEY
static int mod_key_debug = 0;
#endif

/* --- static functions --- */

static void connection_handler(void) { g_main_context_iteration(g_main_context_default(), FALSE); }

/*
 * methods of ui_im_t
 */

static void destroy(ui_im_t *im) {
  im_fcitx_t *fcitx;

  fcitx = (im_fcitx_t*)im;

  g_signal_handlers_disconnect_by_data(fcitx->client, fcitx);
  g_object_unref(fcitx->client);

  if (fcitx->conv) {
    (*fcitx->conv->destroy)(fcitx->conv);
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (fcitx->parser_term) {
    (*fcitx->parser_term->destroy)(fcitx->parser_term);
  }
#endif

  free(fcitx);

  if (--ref_count == 0) {
    (*syms->ui_event_source_remove_fd)(FCITX_ID);

    if (parser_utf8) {
      (*parser_utf8->destroy)(parser_utf8);
      parser_utf8 = NULL;
    }
  }

#ifdef IM_FCITX_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was destroyed. ref_count: %d\n", ref_count);
#endif
}

#ifdef NO_XKB
static KeySym native_to_fcitx_ksym(KeySym ksym) {
  switch (ksym) {
    case XK_BackSpace:
      return FcitxKey_BackSpace;

    case XK_Tab:
      return FcitxKey_Tab;

    case XK_Return:
      return FcitxKey_Return;

    case XK_Escape:
      return FcitxKey_Escape;

    case XK_Zenkaku_Hankaku:
      return FcitxKey_Zenkaku_Hankaku;

    case XK_Hiragana_Katakana:
      return FcitxKey_Hiragana_Katakana;

    case XK_Muhenkan:
      return FcitxKey_Muhenkan;

    case XK_Henkan_Mode:
      return FcitxKey_Henkan_Mode;

    case XK_Home:
      return FcitxKey_Home;

    case XK_Left:
      return FcitxKey_Left;

    case XK_Up:
      return FcitxKey_Up;

    case XK_Right:
      return FcitxKey_Right;

    case XK_Down:
      return FcitxKey_Down;

    case XK_Prior:
      return FcitxKey_Prior;

    case XK_Next:
      return FcitxKey_Next;

    case XK_Insert:
      return FcitxKey_Insert;

    case XK_End:
      return FcitxKey_End;

    case XK_Num_Lock:
      return FcitxKey_Num_Lock;

    case XK_Shift_L:
      return FcitxKey_Shift_L;

    case XK_Shift_R:
      return FcitxKey_Shift_R;

    case XK_Control_L:
      return FcitxKey_Control_L;

    case XK_Control_R:
      return FcitxKey_Control_R;

    case XK_Caps_Lock:
      return FcitxKey_Caps_Lock;

    case XK_Meta_L:
      return FcitxKey_Meta_L;

    case XK_Meta_R:
      return FcitxKey_Meta_R;

    case XK_Alt_L:
      return FcitxKey_Alt_L;

    case XK_Alt_R:
      return FcitxKey_Alt_R;

    case XK_Delete:
      return FcitxKey_Delete;

    default:
      return ksym;
  }
}
#else
#define native_to_fcitx_ksym(ksym) (ksym)
#endif

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_fcitx_t *fcitx;
  u_int state;

  fcitx = (im_fcitx_t*)im;

#ifdef NO_XKB
  if (ksym == 0x08 && (event->state & ModMask)) {
    /* Alt+Backspace => Alt+Control+h (show yourei) */
    ksym = 'h';
    state = event->state | ControlMask;
  } else if (ksym == '[' && (event->state & ControlMask)) {
    /* Control+[ => Escape (exit yourei) */
    ksym = XK_Escape;
    state = event->state & ~ControlMask;
  } else
#endif
  {
    state = event->state;
  }

#ifdef USE_FCITX5
  if (!fcitx_g_client_is_valid(fcitx->client)) {
    connection_handler();
  } else
#endif
  if (event->state & FcitxKeyState_IgnoredMask) {
    /* Is put back in forward_key_event */
    event->state &= ~FcitxKeyState_IgnoredMask;
  } else if (fcitx_client_process_key_sync(
                 fcitx->client, native_to_fcitx_ksym(ksym),
#ifdef NO_XKB
                 event->keycode,
#else
                 event->keycode - 8,
#endif
                 state, event->type == KeyPress ? FCITX_PRESS_KEY : FCITX_RELEASE_KEY,
#ifdef NO_XKB
                 0L /* CurrentTime */
#else
                 event->time
#endif
                 )) {
    fcitx->is_enabled = TRUE;
    event->state = state;
    memcpy(&fcitx->prev_key, event, sizeof(XKeyEvent));

    g_main_context_iteration(g_main_context_default(), FALSE);

    return 0;
  } else {
    fcitx->is_enabled = FALSE;

    if (fcitx->im.preedit.filled_len > 0) {
      g_main_context_iteration(g_main_context_default(), FALSE);
    }
  }

  return 1;
}

static int switch_mode(ui_im_t *im) {
  im_fcitx_t *fcitx;

  fcitx = (im_fcitx_t*)im;

  if (fcitx->is_enabled) {
    fcitx_client_close_ic(fcitx->client);
    fcitx->is_enabled = FALSE;
  } else {
    fcitx_client_enable_ic(fcitx->client);
    fcitx->is_enabled = TRUE;
  }

  return 1;
}

static int is_active(ui_im_t *im) { return ((im_fcitx_t*)im)->is_enabled; }

static void focused(ui_im_t *im) {
  im_fcitx_t *fcitx;

  fcitx = (im_fcitx_t*)im;

#ifdef USE_FCITX5
  if (!fcitx_g_client_is_valid(fcitx->client)) {
    connection_handler();
  } else
#endif
  {
    fcitx_client_focus_in(fcitx->client);
  }

#ifdef USE_IM_CANDIDATE_SCREEN
#ifdef USE_FCITX5
  if (fcitx->im.cand_screen) {
    (*fcitx->im.cand_screen->show)(fcitx->im.cand_screen);
  }
#else
  if (fcitx->im.stat_screen) {
    (*fcitx->im.stat_screen->show)(fcitx->im.stat_screen);
  }
#endif
#endif
}

static void unfocused(ui_im_t *im) {
  im_fcitx_t *fcitx;

  fcitx = (im_fcitx_t*)im;

#ifdef USE_FCITX5
  if (!fcitx_g_client_is_valid(fcitx->client)) {
    connection_handler();
  } else
#endif
  {
    fcitx_client_focus_out(fcitx->client);
  }

#ifdef USE_IM_CANDIDATE_SCREEN
#ifdef USE_FCITX5
  if (fcitx->im.cand_screen) {
    (*fcitx->im.cand_screen->hide)(fcitx->im.cand_screen);
  }
#else
  if (fcitx->im.stat_screen) {
    (*fcitx->im.stat_screen->hide)(fcitx->im.stat_screen);
  }
#endif
#endif
}

static void connected(FcitxClient *client, void *data) {
  im_fcitx_t *fcitx;
  int x;
  int y;

#if 1
  bl_msg_printf("Connected to FCITX server\n");
#endif

  fcitx = data;

  fcitx_client_set_capacity(client,
#ifdef USE_IM_CANDIDATE_SCREEN
                            CAPACITY_CLIENT_SIDE_UI | CAPACITY_CLIENT_SIDE_CONTROL_STATE
#else
                            CAPACITY_PREEDIT | CAPACITY_FORMATTED_PREEDIT
#endif
                            );
  fcitx_client_focus_in(client);

#if 1
  /*
   * XXX
   * To show initial status window (e.g. "Mozc") at the correct position.
   * Should be moved to enable_im() but "enable-im" event doesn't work. (fcitx 4.2.9.1)
   */
  if ((*fcitx->im.listener->get_spot)(fcitx->im.listener->self, NULL, 0, &x, &y)) {
    u_int line_height;

    line_height = (*fcitx->im.listener->get_line_height)(fcitx->im.listener->self);
    fcitx_client_set_cursor_rect(fcitx->client, x, y - line_height, 0, line_height);
  }
#endif
}

#ifndef USE_FCITX5
static void disconnected(FcitxClient *client, void *data) {
  im_fcitx_t *fcitx;

  fcitx = data;

  fcitx->is_enabled = FALSE;

  if (fcitx->im.stat_screen) {
    (*fcitx->im.stat_screen->destroy)(fcitx->im.stat_screen);
    fcitx->im.stat_screen = NULL;
  }
}

#if 0
static void enable_im(FcitxClient *client, void *data) {}
#endif

static void close_im(FcitxClient *client, void *data) { disconnected(client, data); }
#endif

static void commit_string(FcitxClient *client, char *str, void *data) {
  im_fcitx_t *fcitx;
  size_t len;

  fcitx = data;

  if (fcitx->im.preedit.filled_len > 0) {
    /* Reset preedit */
    fcitx->im.preedit.filled_len = 0;
    fcitx->im.preedit.cursor_offset = 0;
    (*fcitx->im.listener->draw_preedit_str)(fcitx->im.listener->self, fcitx->im.preedit.chars,
                                            fcitx->im.preedit.filled_len,
                                            fcitx->im.preedit.cursor_offset);
  }

  if ((len = strlen(str)) == 0) {
    /* do nothing */
  } else {
    (*fcitx->im.listener->write_to_term)(fcitx->im.listener->self, (u_char *)str, len,
                                         fcitx->term_encoding == VT_UTF8 ? NULL : parser_utf8);
  }

#ifdef USE_IM_CANDIDATE_SCREEN
#ifdef USE_FCITX5
  if (fcitx->im.cand_screen) {
    (*fcitx->im.cand_screen->destroy)(fcitx->im.cand_screen);
    fcitx->im.cand_screen = NULL;
  }
#else
  if (fcitx->im.stat_screen) {
    (*fcitx->im.stat_screen->destroy)(fcitx->im.stat_screen);
    fcitx->im.stat_screen = NULL;
  }
#endif
#endif
}

static void forward_key(FcitxClient *client, guint keyval, guint state, gint type, void *data) {
  im_fcitx_t *fcitx;

  fcitx = data;

  if (fcitx->prev_key.keycode ==
#ifdef NO_XKB
      keyval
#else
      keyval + 8
#endif
      ) {
    fcitx->prev_key.state |= FcitxKeyState_IgnoredMask;
#ifdef USE_XLIB
    XPutBackEvent(fcitx->prev_key.display, (XEvent *)&fcitx->prev_key);
#endif
    memset(&fcitx->prev_key, 0, sizeof(XKeyEvent));
  }
}

#if !defined(USE_IM_CANDIDATE_SCREEN) || defined(USE_FCITX5)

static void update_formatted_preedit(FcitxClient *client, GPtrArray *list, int cursor_pos,
                                     void *data) {
  im_fcitx_t *fcitx;

  fcitx = data;

  if (list->len > 0) {
    FcitxPreeditItem *item;
    ef_char_t ch;
    vt_char_t *p;
    u_int num_chars;
    guint count;

    if (fcitx->im.preedit.filled_len == 0) {
      /* Start preediting. */
      int x;
      int y;

      if ((*fcitx->im.listener->get_spot)(fcitx->im.listener->self, NULL, 0, &x, &y)) {
        u_int line_height;

        line_height = (*fcitx->im.listener->get_line_height)(fcitx->im.listener->self);
        fcitx_client_set_cursor_rect(fcitx->client, x, y - line_height, 0, line_height);
      }
    }

    fcitx->im.preedit.cursor_offset = num_chars = 0;

    for (count = 0; count < list->len; count++) {
      size_t str_len;

      item = g_ptr_array_index(list, count);

      str_len = strlen(item->string);

      if (cursor_pos >= 0 && (cursor_pos -= str_len) < 0) {
        fcitx->im.preedit.cursor_offset = num_chars;
      }

      (*parser_utf8->init)(parser_utf8);
      (*parser_utf8->set_str)(parser_utf8, (u_char *)item->string, str_len);

      while ((*parser_utf8->next_char)(parser_utf8, &ch)) {
        num_chars++;
      }
    }

    if ((p = realloc(fcitx->im.preedit.chars, sizeof(vt_char_t) * num_chars)) == NULL) {
      return;
    }

    (*syms->vt_str_init)(fcitx->im.preedit.chars = p,
                         fcitx->im.preedit.num_chars = num_chars);
    fcitx->im.preedit.filled_len = 0;

    for (count = 0; count < list->len; count++) {
      item = g_ptr_array_index(list, count);

      (*parser_utf8->init)(parser_utf8);
      (*parser_utf8->set_str)(parser_utf8, (u_char *)item->string, strlen(item->string));

      while ((*parser_utf8->next_char)(parser_utf8, &ch)) {
        int is_fullwidth = 0;
        int is_comb = 0;
        vt_color_t fg_color;
        vt_color_t bg_color;

        if (item->type != 0) {
          fg_color = VT_BG_COLOR;
          bg_color = VT_FG_COLOR;
        } else {
          fg_color = VT_FG_COLOR;
          bg_color = VT_BG_COLOR;
        }

        if ((*syms->vt_convert_to_internal_ch)(fcitx->im.vtparser, &ch) <= 0) {
          continue;
        }

        if (ch.property & EF_FULLWIDTH) {
          is_fullwidth = 1;
        } else if (ch.property & EF_AWIDTH) {
          /* TODO: check col_size_of_width_a */
          is_fullwidth = 1;
        }

        if (ch.property & EF_COMBINING) {
          is_comb = 1;

          if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth,
                                       (ch.property & EF_AWIDTH) ? 1 : 0, is_comb,
                                       fg_color, bg_color, 0, 0, LS_UNDERLINE_SINGLE, 0, 0)) {
            continue;
          }

          /*
           * if combining failed , char is normally appended.
           */
        }

        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth,
                             (ch.property & EF_AWIDTH) ? 1 : 0, is_comb, fg_color,
                             bg_color, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);

        p++;
        fcitx->im.preedit.filled_len++;
      }
    }
  } else {
#ifdef USE_IM_CANDIDATE_SCREEN
    /* Fcitx5 */
    if (fcitx->im.cand_screen) {
      (*fcitx->im.cand_screen->destroy)(fcitx->im.cand_screen);
      fcitx->im.cand_screen = NULL;
    }
#endif

    if (fcitx->im.preedit.filled_len == 0) {
      return;
    }

    /* Stop preediting. */
    fcitx->im.preedit.filled_len = 0;
  }

  (*fcitx->im.listener->draw_preedit_str)(fcitx->im.listener->self, fcitx->im.preedit.chars,
                                          fcitx->im.preedit.filled_len,
                                          fcitx->im.preedit.cursor_offset);
}

#endif

#ifdef USE_IM_CANDIDATE_SCREEN
#ifdef USE_FCITX5

#ifdef IM_FCITX_DEBUG
static void print_preedit(FcitxGPreeditItem *item) {
  bl_msg_printf("%d:%s ", item->type, item->string);
}

static void print_candate(FcitxGCandidateItem *item) {
  bl_msg_printf("%s:%s ", item->label, item->candidate);
}
#endif

static void update_client_side_ui(FcitxClient *client, GPtrArray *preedit_strings,
                                  int preedit_cursor_pos,
                                  GPtrArray *aux_up_strings, GPtrArray *aux_down_strings,
                                  GPtrArray *cand_list, int cand_cursor_pos, void *data) {
  im_fcitx_t *fcitx;
  u_int i;
  int x;
  int y;

#if 0
  fcitx = (im_fcitx_t*)data;
#else
  /* XXX I don't know why, but 'data' argument of update_client_side_ui() gets NULL. */
  fcitx = g_object_get_data(G_OBJECT(client), "fcitx");
#endif

#ifdef IM_FCITX_DEBUG
  for (int i = 0; i < preedit_strings->len; i++) {
    print_preedit(g_ptr_array_index(preedit_strings, i));
  }
  bl_msg_printf("<= preedit\n");

  for (int i = 0; i < aux_up_strings->len; i++) {
    print_preedit(g_ptr_array_index(aux_up_strings, i));
  }
  bl_msg_printf("<= aux_up\n");

  for (int i = 0; i < aux_down_strings->len; i++) {
    print_preedit(g_ptr_array_index(aux_down_strings, i));
  }
  bl_msg_printf("<= aux_down\n");

  for (int i = 0; i < cand_list->len; i++) {
    print_candidate(g_ptr_array_index(cand_list, i));
  }
  bl_msg_printf("<= cand list\n");
#endif

  update_formatted_preedit(client, preedit_strings, preedit_cursor_pos, fcitx);

  (*fcitx->im.listener->get_spot)(fcitx->im.listener->self, fcitx->im.preedit.chars,
                                  fcitx->im.preedit.segment_offset, &x, &y);

  if (cand_list->len == 0) {
    if (fcitx->im.cand_screen) {
      (*fcitx->im.cand_screen->destroy)(fcitx->im.cand_screen);
      fcitx->im.cand_screen = NULL;
    }

    return;
  }

  if (fcitx->im.cand_screen == NULL) {
    if (cand_cursor_pos == 0) {
      return;
    }

    if (!(fcitx->im.cand_screen = (*syms->ui_im_candidate_screen_new)(
              fcitx->im.disp, fcitx->im.font_man, fcitx->im.color_man, fcitx->im.vtparser,
              (*fcitx->im.listener->is_vertical)(fcitx->im.listener->self), 1,
              (*fcitx->im.listener->get_line_height)(fcitx->im.listener->self), x, y))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ui_im_candidate_screen_new() failed.\n");
#endif

      return;
    }
  } else {
    (*fcitx->im.cand_screen->show)(fcitx->im.cand_screen);
  }

  if (!(*fcitx->im.cand_screen->init)(fcitx->im.cand_screen, cand_list->len, 10)) {
    (*fcitx->im.cand_screen->destroy)(fcitx->im.cand_screen);
    fcitx->im.cand_screen = NULL;

    return;
  }

  (*fcitx->im.cand_screen->set_spot)(fcitx->im.cand_screen, x, y);

  for (i = 0; i < cand_list->len; i++) {
    u_char *str = ((FcitxGCandidateItem*)g_ptr_array_index(cand_list, i))->candidate;

    if (fcitx->term_encoding != VT_UTF8) {
      u_char *p;

      if (im_convert_encoding(parser_utf8, fcitx->conv, str, &p, strlen(str) + 1)) {
        (*fcitx->im.cand_screen->set)(fcitx->im.cand_screen, fcitx->parser_term, p, i);
        free(p);
      }
    } else {
      (*fcitx->im.cand_screen->set)(fcitx->im.cand_screen, fcitx->parser_term, str, i);
    }
  }

  (*fcitx->im.cand_screen->select)(fcitx->im.cand_screen, cand_cursor_pos);
}

#else

static void update_client_side_ui(FcitxClient *client, char *auxup, char *auxdown, char *preedit,
                                  char *candidateword, char *imname, int cursor_pos, void *data) {
  im_fcitx_t *fcitx;
  int x;
  int y;
  ef_char_t ch;
  vt_char_t *p;
  u_int num_chars;
  size_t preedit_len;

  fcitx = (im_fcitx_t*)data;

  if ((preedit_len = strlen(preedit)) == 0) {
    if (fcitx->im.preedit.filled_len == 0) {
      return;
    }

    /* Stop preediting. */
    fcitx->im.preedit.filled_len = 0;
  } else {
    u_char *tmp = NULL;

    fcitx->im.preedit.cursor_offset = num_chars = 0;
    (*parser_utf8->init)(parser_utf8);
    (*parser_utf8->set_str)(parser_utf8, preedit, preedit_len);
    while ((*parser_utf8->next_char)(parser_utf8, &ch)) {
      if (preedit_len - parser_utf8->left > cursor_pos) {
        fcitx->im.preedit.cursor_offset = num_chars;
        cursor_pos = preedit_len; /* Not to enter here twice. */
      }
      num_chars++;
    }

    if ((p = realloc(fcitx->im.preedit.chars, sizeof(vt_char_t) * num_chars)) == NULL) {
      return;
    }

    if (NEED_TO_CONV(fcitx)) {
      if (im_convert_encoding(parser_utf8, fcitx->conv, preedit, &tmp, preedit_len + 1)) {
        preedit = tmp;
        preedit_len = strlen(preedit);
      }
    }

    (*syms->vt_str_init)(fcitx->im.preedit.chars = p,
                         fcitx->im.preedit.num_chars = num_chars);
    fcitx->im.preedit.filled_len = 0;

    (*fcitx->parser_term->init)(fcitx->parser_term);
    (*fcitx->parser_term->set_str)(fcitx->parser_term, preedit, preedit_len);

    while ((*fcitx->parser_term->next_char)(fcitx->parser_term, &ch)) {
      int is_fullwidth;
      int is_comb;

      if ((*syms->vt_convert_to_internal_ch)(fcitx->im.vtparser, &ch) <= 0) {
        continue;
      }

      if (ch.property & EF_FULLWIDTH) {
        is_fullwidth = 1;
      } else if (ch.property & EF_AWIDTH) {
        /* TODO: check col_size_of_width_a */
        is_fullwidth = 1;
      } else {
        is_fullwidth = IS_FULLWIDTH_CS(ch.cs);
      }

      if (ch.property & EF_COMBINING) {
        is_comb = 1;

        if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth,
                                     (ch.property & EF_AWIDTH) ? 1 : 0, is_comb,
                                     VT_FG_COLOR, VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0)) {
          continue;
        }

        /*
         * if combining failed , char is normally appended.
         */
      } else {
        is_comb = 0;
      }

      (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth,
                           (ch.property & EF_AWIDTH) ? 1 : 0, is_comb, VT_FG_COLOR,
                           VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);

      p++;
      fcitx->im.preedit.filled_len++;
    }

    if (tmp) {
      free(tmp);
    }
  }

  (*fcitx->im.listener->draw_preedit_str)(fcitx->im.listener->self, fcitx->im.preedit.chars,
                                          fcitx->im.preedit.filled_len,
                                          fcitx->im.preedit.cursor_offset);

  if (strlen(candidateword) == 0) {
#ifdef USE_IM_CANDIDATE_SCREEN
    if (fcitx->im.stat_screen) {
      (*fcitx->im.stat_screen->destroy)(fcitx->im.stat_screen);
      fcitx->im.stat_screen = NULL;
    }
#endif
  } else {
    u_char *tmp = NULL;

    (*fcitx->im.listener->get_spot)(fcitx->im.listener->self, fcitx->im.preedit.chars,
                                    fcitx->im.preedit.segment_offset, &x, &y);

    if (fcitx->im.stat_screen == NULL) {
      if (!(fcitx->im.stat_screen = (*syms->ui_im_status_screen_new)(
                fcitx->im.disp, fcitx->im.font_man, fcitx->im.color_man, fcitx->im.vtparser,
                (*fcitx->im.listener->is_vertical)(fcitx->im.listener->self),
                (*fcitx->im.listener->get_line_height)(fcitx->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_candidate_screen_new() failed.\n");
#endif

        return;
      }
    } else {
      (*fcitx->im.stat_screen->show)(fcitx->im.stat_screen);
      (*fcitx->im.stat_screen->set_spot)(fcitx->im.stat_screen, x, y);
    }

    if (strncmp(candidateword, "1.", 2) == 0) {
      int count = 2;
      char *src = candidateword;
      char *p;
      char digit[5];

#ifdef __DEBUG
      bl_msg_printf("Fcitx: %s\n", candidateword);
#endif

      do {
        sprintf(digit, " %d.", count);

        if (!(p = strstr(src, digit))) {
          if (count % 10 == 0 && (src = strstr(src, " 0."))) {
            count = 1;
          } else {
            break;
          }
        } else {
          count++;
          src = p;
        }

        *src = '\n';
        src += strlen(digit);
      } while (count < 100);

#ifdef __DEBUG
      bl_msg_printf("%s\n", candidateword);
#endif
    }

    if (NEED_TO_CONV(fcitx)) {
      if (im_convert_encoding(parser_utf8, fcitx->conv, candidateword, &tmp,
                              strlen(candidateword) + 1)) {
        candidateword = tmp;
      }
    }

    (*fcitx->im.stat_screen->set)(fcitx->im.stat_screen, fcitx->parser_term, candidateword);

    if (tmp) {
      free(tmp);
    }
  }
}

#endif
#endif


/* --- global functions --- */

ui_im_t *im_fcitx_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                      ui_im_export_syms_t *export_syms, char *engine,
                      u_int mod_ignore_mask /* Not used for now. */
                      ) {
  im_fcitx_t *fcitx = NULL;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

#ifdef DEBUG_MODKEY
  if (getenv("MOD_KEY_DEBUG")) {
    mod_key_debug = 1;
  }
#endif

  if (!syms) {
    syms = export_syms;

    g_type_init();
  }

  if (!(fcitx = calloc(1, sizeof(im_fcitx_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

#ifdef USE_FCITX5
#if 0
  {
    FcitxGWatcher *watcher;

    watcher = fcitx_g_watcher_new();
    fcitx_g_watcher_set_watch_portal(watcher, TRUE);
    fcitx_g_watcher_watch(watcher);
    g_object_ref_sink(watcher);

    if (!(fcitx->client = fcitx_g_client_new_with_watcher(watcher))) {
      goto error;
    }
  }
#else
  if (!(fcitx->client = fcitx_g_client_new())) {
    goto error;
  }
#endif

  fcitx_g_client_set_program(fcitx->client, "mlterm");
#if 0
  fcitx_g_client_set_display(fcitx->client, "x11:");
#endif
#else
  if (!(fcitx->client = fcitx_client_new())) {
    goto error;
  }
#endif

  g_signal_connect(fcitx->client, "connected", G_CALLBACK(connected), fcitx);
#ifndef USE_FCITX5
  g_signal_connect(fcitx->client, "disconnected", G_CALLBACK(disconnected), fcitx);
#endif
#if 0
  g_signal_connect(fcitx->client, "enable-im", G_CALLBACK(enable_im), fcitx);
#endif
#ifndef USE_FCITX5
  g_signal_connect(fcitx->client, "close-im", G_CALLBACK(close_im), fcitx);
#endif
  g_signal_connect(fcitx->client, "forward-key", G_CALLBACK(forward_key), fcitx);
  g_signal_connect(fcitx->client, "commit-string", G_CALLBACK(commit_string), fcitx);
#ifdef USE_IM_CANDIDATE_SCREEN
  g_signal_connect(fcitx->client, "update-client-side-ui", G_CALLBACK(update_client_side_ui),
                   fcitx);
#ifdef USE_FCITX5
  /* XXX I don't know why, but 'data' argument of update_client_side_ui() gets NULL. */
  g_object_set_data(G_OBJECT(fcitx->client), "fcitx", fcitx);
#endif
#else
  g_signal_connect(fcitx->client, "update-formatted-preedit", G_CALLBACK(update_formatted_preedit),
                   fcitx);
#endif

  fcitx->term_encoding = term_encoding;
  fcitx->is_enabled = FALSE;

  if (term_encoding != VT_UTF8) {
    if (!(fcitx->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
      goto error;
    }
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (!(fcitx->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }
#endif

  /*
   * set methods of ui_im_t
   */
  fcitx->im.destroy = destroy;
  fcitx->im.key_event = key_event;
  fcitx->im.switch_mode = switch_mode;
  fcitx->im.is_active = is_active;
  fcitx->im.focused = focused;
  fcitx->im.unfocused = unfocused;

  if (ref_count++ == 0) {
    (*syms->ui_event_source_add_fd)(FCITX_ID, connection_handler);

    if (!(parser_utf8 = (*syms->vt_char_encoding_parser_new)(VT_UTF8))) {
      goto error;
    }
  }

#ifdef IM_FCITX_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

#ifdef USE_FCITX5
  /* I don't know why, but connected() is not called without this. */
  usleep(100000);
#endif

  return (ui_im_t*)fcitx;

error:
  if (fcitx) {
    if (fcitx->conv) {
      (*fcitx->conv->destroy)(fcitx->conv);
    }

#ifdef USE_IM_CANDIDATE_SCREEN
    if (fcitx->parser_term) {
      (*fcitx->parser_term->destroy)(fcitx->parser_term);
    }
#endif

    if (fcitx->client) {
      g_object_unref(fcitx->client);
    }

    free(fcitx);
  }

  return NULL;
}

/* --- module entry point for external tools --- */

im_info_t *im_fcitx_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->id = strdup("fcitx");
  result->name = strdup("fcitx");
  result->num_args = 0;
  result->args = NULL;
  result->readable_args = NULL;

  return result;
}
