/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <ibus.h>

#include <pobl/bl_slist.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>

#include <ui_im.h>

#include "../im_common.h"
#include "../im_info.h"

#if 0
#define IM_IBUS_DEBUG 1
#endif

#define IBUS_ID -2

#if IBUS_CHECK_VERSION(1, 5, 0)
#define ibus_input_context_is_enabled(context) (TRUE)
#define ibus_input_context_disable(context) (0)
#define ibus_input_context_enable(context) (0)
#endif

#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)
#define USE_IM_CANDIDATE_SCREEN
#define NO_XKB
#elif defined(USE_SDL2)
#define NO_XKB
#endif

#ifdef USE_WAYLAND
#define KeyRelease 3
#define ibus_input_context_set_cursor_location(context, x, y, w, h) \
        ibus_input_context_set_cursor_location_relative(context, x, y, w, h)
#endif

typedef struct im_ibus {
  /* input method common object */
  ui_im_t im;

  IBusInputContext *context;

  vt_char_encoding_t term_encoding;

#ifdef USE_IM_CANDIDATE_SCREEN
  ef_parser_t *parser_term; /* for term encoding */
#endif
  ef_conv_t *conv; /* for term encoding */

  /*
   * Cache a result of ibus_input_context_is_enabled() which uses
   * DBus connection internally.
   */
  gboolean is_enabled;

  XKeyEvent prev_key;

#ifdef USE_IM_CANDIDATE_SCREEN
  gchar *prev_first_cand;
  u_int prev_num_cands;
#endif

  struct im_ibus *next;

} im_ibus_t;

/* --- static variables --- */

static IBusBus *ibus_bus;
static int ibus_bus_fd = -1;
static im_ibus_t *ibus_list = NULL;
static int ref_count = 0;
static ef_parser_t *parser_utf8 = NULL;
static ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
#ifdef DEBUG_MODKEY
static int mod_key_debug = 0;
#endif

/* --- static functions --- */

#if 0
static vt_color_t get_near_color(u_int rgb) {
  u_int rgb_bit = 0;

  if ((rgb & 0xff0000) > 0x7f0000) {
    rgb_bit |= 0x4;
  }
  if ((rgb & 0xff00) > 0x7f00) {
    rgb_bit |= 0x2;
  }
  if ((rgb & 0xff) > 0x7f) {
    rgb_bit |= 0x1;
  }

  switch (rgb_bit) {
    case 0:
      return VT_BLACK;
    case 1:
      return VT_BLUE;
    case 2:
      return VT_GREEN;
    case 3:
      return VT_CYAN;
    case 4:
      return VT_RED;
    case 5:
      return VT_MAGENTA;
    case 6:
      return VT_YELLOW;
    case 7:
      return VT_WHITE;
    default:
      return VT_BLACK;
  }
}
#endif

static void update_preedit_text(IBusInputContext *context, IBusText *text, gint cursor_pos,
                                gboolean visible, gpointer data) {
  im_ibus_t *ibus;
  vt_char_t *p;
  u_int len;
  ef_char_t ch;

  ibus = (im_ibus_t*)data;

  if ((len = ibus_text_get_length(text)) > 0) {
    u_int index;

    if (ibus->im.preedit.filled_len == 0) {
      /* Start preediting. */
      int x;
      int y;

      if ((*ibus->im.listener->get_spot)(ibus->im.listener->self, NULL, 0, &x, &y)) {
        u_int line_height;

        line_height = (*ibus->im.listener->get_line_height)(ibus->im.listener->self);
        ibus_input_context_set_cursor_location(ibus->context, x, y - line_height, 0, line_height);
      }
    }

    if ((p = realloc(ibus->im.preedit.chars, sizeof(vt_char_t) * len)) == NULL) {
      return;
    }

    (*syms->vt_str_init)(ibus->im.preedit.chars = p, ibus->im.preedit.num_chars = len);
    ibus->im.preedit.filled_len = 0;

    (*parser_utf8->init)(parser_utf8);
    (*parser_utf8->set_str)(parser_utf8, text->text, strlen(text->text));

    index = 0;
    while ((*parser_utf8->next_char)(parser_utf8, &ch)) {
      u_int count;
      IBusAttribute *attr;
      int is_fullwidth = 0;
      int is_comb = 0;
      int is_underlined = 0;
      vt_color_t fg_color = VT_FG_COLOR;
      vt_color_t bg_color = VT_BG_COLOR;

      for (count = 0; (attr = ibus_attr_list_get(text->attrs, count)); count++) {
        if (attr->start_index <= index && index < attr->end_index) {
          if (attr->type == IBUS_ATTR_TYPE_UNDERLINE) {
            is_underlined = (attr->value != IBUS_ATTR_UNDERLINE_NONE);
          }
#if 0
          else if (attr->type == IBUS_ATTR_TYPE_FOREGROUND) {
            fg_color = get_near_color(attr->value);
          } else if (attr->type == IBUS_ATTR_TYPE_BACKGROUND) {
            bg_color = get_near_color(attr->value);
          }
#else
          else if (attr->type == IBUS_ATTR_TYPE_BACKGROUND) {
            fg_color = VT_BG_COLOR;
            bg_color = VT_FG_COLOR;
          }
#endif
        }
      }

      if ((*syms->vt_convert_to_internal_ch)(ibus->im.vtparser, &ch) <= 0) {
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

        if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb,
                                     fg_color, bg_color, 0, 0, LS_UNDERLINE_SINGLE, 0, 0)) {
          continue;
        }

        /*
         * if combining failed , char is normally appended.
         */
      }

      (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, fg_color,
                           bg_color, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);

      p++;
      ibus->im.preedit.filled_len++;

      index++;
    }
  } else {
#ifdef USE_IM_CANDIDATE_SCREEN
    if (ibus->im.cand_screen) {
      (*ibus->im.cand_screen->delete)(ibus->im.cand_screen);
      ibus->im.cand_screen = NULL;
    }
#endif

    if (ibus->im.preedit.filled_len == 0) {
      return;
    }

    /* Stop preediting. */
    ibus->im.preedit.filled_len = 0;
  }

  ibus->im.preedit.cursor_offset = cursor_pos;

  (*ibus->im.listener->draw_preedit_str)(ibus->im.listener->self, ibus->im.preedit.chars,
                                         ibus->im.preedit.filled_len,
                                         ibus->im.preedit.cursor_offset);
}

static void hide_preedit_text(IBusInputContext *context, gpointer data) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)data;

  if (ibus->im.preedit.filled_len == 0) {
    return;
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->hide)(ibus->im.cand_screen);
  }
#endif

  /* Stop preediting. */
  ibus->im.preedit.filled_len = 0;
  ibus->im.preedit.cursor_offset = 0;

  (*ibus->im.listener->draw_preedit_str)(ibus->im.listener->self, ibus->im.preedit.chars,
                                         ibus->im.preedit.filled_len,
                                         ibus->im.preedit.cursor_offset);
}

static void commit_text(IBusInputContext *context, IBusText *text, gpointer data) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)data;

  if (ibus->im.preedit.filled_len > 0) {
    /* Reset preedit */
    ibus->im.preedit.filled_len = 0;
    ibus->im.preedit.cursor_offset = 0;
    (*ibus->im.listener->draw_preedit_str)(ibus->im.listener->self, ibus->im.preedit.chars,
                                           ibus->im.preedit.filled_len,
                                           ibus->im.preedit.cursor_offset);
  }

  if (ibus_text_get_length(text) == 0) {
    /* do nothing */
  } else if (ibus->term_encoding == VT_UTF8) {
    (*ibus->im.listener->write_to_term)(ibus->im.listener->self, text->text, strlen(text->text));
  } else {
    u_char conv_buf[256];
    size_t filled_len;

    (*parser_utf8->init)(parser_utf8);
    (*parser_utf8->set_str)(parser_utf8, text->text, strlen(text->text));

    (*ibus->conv->init)(ibus->conv);

    while (!parser_utf8->is_eos) {
      filled_len = (*ibus->conv->convert)(ibus->conv, conv_buf, sizeof(conv_buf), parser_utf8);

      if (filled_len == 0) {
        /* finished converting */
        break;
      }

      (*ibus->im.listener->write_to_term)(ibus->im.listener->self, conv_buf, filled_len);
    }
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->delete)(ibus->im.cand_screen);
    ibus->im.cand_screen = NULL;
  }
#endif
}

static void forward_key_event(IBusInputContext *context, guint keyval, guint keycode, guint state,
                              gpointer data) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)data;

  if (ibus->prev_key.keycode ==
#ifdef NO_XKB
      keycode
#else
      keycode + 8
#endif
      ) {
    ibus->prev_key.state |= IBUS_IGNORED_MASK;
#ifdef USE_XLIB
    XPutBackEvent(ibus->prev_key.display, &ibus->prev_key);
#endif
    memset(&ibus->prev_key, 0, sizeof(XKeyEvent));
  }
}

#ifdef USE_IM_CANDIDATE_SCREEN

static void show_lookup_table(IBusInputContext *context, gpointer data) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)data;

  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->show)(ibus->im.cand_screen);
  }
}

static void hide_lookup_table(IBusInputContext *context, gpointer data) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)data;

  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->hide)(ibus->im.cand_screen);
  }
}

static void update_lookup_table(IBusInputContext *context, IBusLookupTable *table, gboolean visible,
                                gpointer data) {
  im_ibus_t *ibus;
  u_int num_cands;
  int cur_pos;
  u_int i;
  int x;
  int y;

  ibus = (im_ibus_t*)data;

  if ((num_cands = ibus_lookup_table_get_number_of_candidates(table)) == 0) {
    return;
  }

  cur_pos = ibus_lookup_table_get_cursor_pos(table);

  (*ibus->im.listener->get_spot)(ibus->im.listener->self, ibus->im.preedit.chars,
                                 ibus->im.preedit.segment_offset, &x, &y);

  if (ibus->im.cand_screen == NULL) {
    if (cur_pos == 0) {
      return;
    }

    if (!(ibus->im.cand_screen = (*syms->ui_im_candidate_screen_new)(
              ibus->im.disp, ibus->im.font_man, ibus->im.color_man, ibus->im.vtparser,
              (*ibus->im.listener->is_vertical)(ibus->im.listener->self), 1,
              (*ibus->im.listener->get_line_height)(ibus->im.listener->self), x, y))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ui_im_candidate_screen_new() failed.\n");
#endif

      return;
    }
  } else {
    (*ibus->im.cand_screen->show)(ibus->im.cand_screen);
  }

  if (!(*ibus->im.cand_screen->init)(ibus->im.cand_screen, num_cands, 10)) {
    (*ibus->im.cand_screen->delete)(ibus->im.cand_screen);
    ibus->im.cand_screen = NULL;

    return;
  }

  (*ibus->im.cand_screen->set_spot)(ibus->im.cand_screen, x, y);

  for (i = 0; i < num_cands; i++) {
    u_char *str;

    /* ibus 1.4.1 on Ubuntu 12.10 can return NULL if num_cands > 0. */
    if (!(str = ibus_text_get_text(ibus_lookup_table_get_candidate(table, i)))) {
      continue;
    }

    if (ibus->term_encoding != VT_UTF8) {
      u_char *p;

      (*parser_utf8->init)(parser_utf8);
      (*ibus->conv->init)(ibus->conv);

      if (im_convert_encoding(parser_utf8, ibus->conv, str, &p, strlen(str) + 1)) {
        (*ibus->im.cand_screen->set)(ibus->im.cand_screen, ibus->parser_term, p, i);
        free(p);
      }
    } else {
      (*ibus->im.cand_screen->set)(ibus->im.cand_screen, ibus->parser_term, str, i);
    }
  }

  (*ibus->im.cand_screen->select)(ibus->im.cand_screen, cur_pos);
}

#endif

static void connection_handler(void) {
#ifdef DBUS_H
  DBusConnection *connection;

  connection = ibus_connection_get_connection(ibus_bus_get_connection(ibus_bus));

  dbus_connection_read_write(connection, 0);

  while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS)
    ;
#else
#if 0
  g_dbus_connection_flush_sync(ibus_bus_get_connection(ibus_bus), NULL, NULL);
#endif
  g_main_context_iteration(g_main_context_default(), FALSE);
#endif
}

static int add_event_source(void) {
#ifdef DBUS_H
  if (!dbus_connection_get_unix_fd(
          ibus_connection_get_connection(ibus_bus_get_connection(ibus_bus)), &ibus_bus_fd)) {
    return 0;
  }
#else
  /*
   * GIOStream returned by g_dbus_connection_get_stream() is forcibly
   * regarded as GSocketConnection.
   */
  if ((ibus_bus_fd = g_socket_get_fd(g_socket_connection_get_socket(
           g_dbus_connection_get_stream(ibus_bus_get_connection(ibus_bus))))) == -1) {
    return 0;
  }
#endif
  (*syms->ui_event_source_add_fd)(ibus_bus_fd, connection_handler);
  (*syms->ui_event_source_add_fd)(IBUS_ID, connection_handler);

  return 1;
}

static void remove_event_source(int complete) {
  if (ibus_bus_fd >= 0) {
    (*syms->ui_event_source_remove_fd)(ibus_bus_fd);
    ibus_bus_fd = -1;
  }

#ifndef USE_WAYLAND
  if (complete) {
    (*syms->ui_event_source_remove_fd)(IBUS_ID);
  }
#endif
}

/*
 * methods of ui_im_t
 */

static void delete(ui_im_t *im) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)im;

  if (ibus->context) {
#ifdef DBUS_H
    ibus_object_destroy((IBusObject*)ibus->context);
#else
    ibus_proxy_destroy((IBusProxy*)ibus->context);
#endif
  }

  bl_slist_remove(ibus_list, ibus);

  if (ibus->conv) {
    (*ibus->conv->delete)(ibus->conv);
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (ibus->parser_term) {
    (*ibus->parser_term->delete)(ibus->parser_term);
  }

  free(ibus->prev_first_cand);
#endif

  free(ibus);

  if (--ref_count == 0) {
    remove_event_source(1);

    ibus_object_destroy((IBusObject*)ibus_bus);
    ibus_bus = NULL;

    if (parser_utf8) {
      (*parser_utf8->delete)(parser_utf8);
      parser_utf8 = NULL;
    }
  }

#ifdef IM_IBUS_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count);
#endif
}

#ifdef NO_XKB
static KeySym native_to_ibus_ksym(KeySym ksym) {
  switch (ksym) {
    case XK_BackSpace:
      return IBUS_BackSpace;

    case XK_Tab:
      return IBUS_Tab;

    case XK_Return:
      return IBUS_Return;

    case XK_Escape:
      return IBUS_Escape;

    case XK_Zenkaku_Hankaku:
      return IBUS_Zenkaku_Hankaku;

    case XK_Hiragana_Katakana:
      return IBUS_Hiragana_Katakana;

    case XK_Muhenkan:
      return IBUS_Muhenkan;

    case XK_Henkan_Mode:
      return IBUS_Henkan_Mode;

    case XK_Home:
      return IBUS_Home;

    case XK_Left:
      return IBUS_Left;

    case XK_Up:
      return IBUS_Up;

    case XK_Right:
      return IBUS_Right;

    case XK_Down:
      return IBUS_Down;

    case XK_Prior:
      return IBUS_Prior;

    case XK_Next:
      return IBUS_Next;

    case XK_Insert:
      return IBUS_Insert;

    case XK_End:
      return IBUS_End;

    case XK_Num_Lock:
      return IBUS_Num_Lock;

    case XK_Shift_L:
      return IBUS_Shift_L;

    case XK_Shift_R:
      return IBUS_Shift_R;

    case XK_Control_L:
      return IBUS_Control_L;

    case XK_Control_R:
      return IBUS_Control_R;

    case XK_Caps_Lock:
      return IBUS_Caps_Lock;

#if 0
    case XK_Meta_L:
      return IBUS_Super_L; /* Windows key on linux */

    case XK_Meta_R:
      return IBUS_Super_R; /* Menu key on linux */
#else
    case XK_Meta_L:
      return IBUS_Meta_L;

    case XK_Meta_R:
      return IBUS_Meta_R;
#endif

    case XK_Alt_L:
      return IBUS_Alt_L;

    case XK_Alt_R:
      return IBUS_Alt_R;

    case XK_Delete:
      return IBUS_Delete;

    case XK_Super_L:
      return IBUS_Super_L;

    case XK_Super_R:
      return IBUS_Super_R;

    default:
      break;
  }

  return ksym;
}
#else
#define native_to_ibus_ksym(ksym) (ksym)
#endif

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)im;

  if (!ibus->context) {
    return 1;
  }

  if (event->state & IBUS_IGNORED_MASK) {
    /* Is put back in forward_key_event */
    event->state &= ~IBUS_IGNORED_MASK;
  } else if (ibus_input_context_process_key_event(ibus->context, native_to_ibus_ksym(ksym),
#ifdef NO_XKB
                                                  event->keycode, event->state
#else
                                                  event->keycode - 8,
                                                  event->state |
                                                      (event->type == KeyRelease ? IBUS_RELEASE_MASK
                                                                                 : 0)
#endif
                                                  )) {
    gboolean is_enabled_old;

    is_enabled_old = ibus->is_enabled;
    ibus->is_enabled = ibus_input_context_is_enabled(ibus->context);

#if !IBUS_CHECK_VERSION(1, 5, 0)
    if (ibus->is_enabled != is_enabled_old) {
      return 0;
    } else
#endif
        if (ibus->is_enabled) {
#ifndef DBUS_H
#if 0
      g_dbus_connection_flush_sync(ibus_bus_get_connection(ibus_bus), NULL, NULL);
#endif
      g_main_context_iteration(g_main_context_default(), FALSE);
#endif

      memcpy(&ibus->prev_key, event, sizeof(XKeyEvent));

      return 0;
    }
  } else if (ibus->im.preedit.filled_len > 0) {
/* Pressing "q" in preediting. */
#ifndef DBUS_H
#if 0
    g_dbus_connection_flush_sync(ibus_bus_get_connection(ibus_bus), NULL, NULL);
#endif
    g_main_context_iteration(g_main_context_default(), FALSE);
#endif
  }

  return 1;
}

static void set_engine(IBusInputContext *context, gchar *name) {
  bl_msg_printf("iBus engine is %s\n", name);
  ibus_input_context_set_engine(context, name);
}

#if IBUS_CHECK_VERSION(1, 5, 0)
static void next_engine(IBusInputContext *context) {
  IBusConfig *config;
  GVariant *var;

  if ((config = ibus_bus_get_config(ibus_bus)) &&
      (var = ibus_config_get_value(config, "general", "preload-engines"))) {
    static int show_engines = 1;
    const gchar *cur_name;
    GVariantIter *iter;
    gchar *name;

    cur_name = ibus_engine_desc_get_name(ibus_input_context_get_engine(context));

    g_variant_get(var, "as", &iter);

    if (show_engines) {
      bl_msg_printf("iBus engines: ");
      while (g_variant_iter_loop(iter, "s", &name)) {
        bl_msg_printf(name);
        bl_msg_printf(",");
      }
      bl_msg_printf("\n");

      g_variant_iter_init(iter, var);
      show_engines = 0;
    }

    if (g_variant_iter_loop(iter, "s", &name)) {
      gchar *first_name;
      int loop;

      first_name = g_strdup(name);
      loop = 1;

      do {
        if (strcmp(cur_name, name) == 0) {
          loop = 0;
        }

        if (!g_variant_iter_loop(iter, "s", &name)) {
          loop = 0;
          name = first_name;
        }
      } while (loop);

      set_engine(context, name);

      free(first_name);
    }

    g_variant_iter_free(iter);
    g_variant_unref(var);
  }
}
#endif

static int switch_mode(ui_im_t *im) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)im;

  if (!ibus->context) {
    return 0;
  }

#if IBUS_CHECK_VERSION(1, 5, 0)
  next_engine(ibus->context);
  ibus->is_enabled = !ibus->is_enabled;
#else
  if (ibus->is_enabled) {
    ibus_input_context_disable(ibus->context);
    ibus->is_enabled = FALSE;
  } else {
    ibus_input_context_enable(ibus->context);
    ibus->is_enabled = TRUE;
  }
#endif

  return 1;
}

static int is_active(ui_im_t *im) { return ((im_ibus_t*)im)->is_enabled; }

static void focused(ui_im_t *im) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)im;

  if (!ibus->context) {
    return;
  }

  ibus_input_context_focus_in(ibus->context);

  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->show)(ibus->im.cand_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_ibus_t *ibus;

  ibus = (im_ibus_t*)im;

  if (!ibus->context) {
    return;
  }

  ibus_input_context_focus_out(ibus->context);

  if (ibus->im.cand_screen) {
    (*ibus->im.cand_screen->hide)(ibus->im.cand_screen);
  }
}

static IBusInputContext *context_new(im_ibus_t *ibus, char *engine) {
  IBusInputContext *context;

  if (!(context = ibus_bus_create_input_context(ibus_bus, "mlterm"))) {
    return NULL;
  }

  ibus_input_context_set_capabilities(context,
                                      IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS
#ifdef USE_IM_CANDIDATE_SCREEN
                                      | IBUS_CAP_LOOKUP_TABLE
#endif
#if 0
                                      | IBUS_CAP_SURROUNDING_TEXT
#endif
                                      );

  g_signal_connect(context, "update-preedit-text", G_CALLBACK(update_preedit_text), ibus);
  g_signal_connect(context, "hide-preedit-text", G_CALLBACK(hide_preedit_text), ibus);
  g_signal_connect(context, "commit-text", G_CALLBACK(commit_text), ibus);
  g_signal_connect(context, "forward-key-event", G_CALLBACK(forward_key_event), ibus);
#ifdef USE_IM_CANDIDATE_SCREEN
  g_signal_connect(context, "update-lookup-table", G_CALLBACK(update_lookup_table), ibus);
  g_signal_connect(context, "show-lookup-table", G_CALLBACK(show_lookup_table), ibus);
  g_signal_connect(context, "hide-lookup-table", G_CALLBACK(hide_lookup_table), ibus);
#endif

  if (engine) {
    set_engine(context, engine);
  }
#if (defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)) && IBUS_CHECK_VERSION(1, 5, 0)
  else {
    next_engine(context);
  }
#endif

  return context;
}

static void connected(IBusBus *bus, gpointer data) {
  im_ibus_t *ibus;

  if (bus != ibus_bus || !ibus_bus_is_connected(ibus_bus) || !add_event_source()) {
    return;
  }

  for (ibus = ibus_list; ibus; ibus = bl_slist_next(ibus_list)) {
    ibus->context = context_new(ibus, NULL);
  }
}

static void disconnected(IBusBus *bus, gpointer data) {
  im_ibus_t *ibus;

  if (bus != ibus_bus) {
    return;
  }

  remove_event_source(0);

  for (ibus = ibus_list; ibus; ibus = bl_slist_next(ibus_list)) {
#ifdef DBUS_H
    ibus_object_destroy((IBusObject*)ibus->context);
#else
    ibus_proxy_destroy((IBusProxy*)ibus->context);
#endif
    ibus->context = NULL;
    ibus->is_enabled = FALSE;
  }
}

/* --- global functions --- */

ui_im_t *im_ibus_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                     ui_im_export_syms_t *export_syms, char *engine,
                     u_int mod_ignore_mask /* Not used for now. */
                     ) {
  static int is_init;
  im_ibus_t *ibus = NULL;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

#ifdef DEBUG_MODKEY
  if (getenv("MOD_KEY_DEBUG")) {
    mod_key_debug = 1;
  }
#endif

  if (!is_init) {
    ibus_init();

    /* Don't call ibus_init() again if ibus daemon is not found below. */
    is_init = 1;
  }

  if (!ibus_bus) {
    syms = export_syms;

/* g_getenv( "DISPLAY") will be called in ibus_get_socket_path(). */
#if 0
    ibus_set_display(g_getenv("DISPLAY"));
#endif

    ibus_bus = ibus_bus_new();

    if (!ibus_bus_is_connected(ibus_bus)) {
      bl_error_printf("IBus daemon is not found.\n");

      goto error;
    }

    if (!add_event_source()) {
      goto error;
    }

    if (!(parser_utf8 = (*syms->vt_char_encoding_parser_new)(VT_UTF8))) {
      goto error;
    }

    g_signal_connect(ibus_bus, "connected", G_CALLBACK(connected), NULL);
    g_signal_connect(ibus_bus, "disconnected", G_CALLBACK(disconnected), NULL);
  }

  if (!(ibus = calloc(1, sizeof(im_ibus_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  if (term_encoding != VT_UTF8) {
    if (!(ibus->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
      goto error;
    }
  }

#ifdef USE_IM_CANDIDATE_SCREEN
  if (!(ibus->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }
#endif

  ibus->term_encoding = term_encoding;

  if (!(ibus->context = context_new(ibus, engine))) {
    goto error;
  }

  ibus->is_enabled = FALSE;

  /*
   * set methods of ui_im_t
   */
  ibus->im.delete = delete;
  ibus->im.key_event = key_event;
  ibus->im.switch_mode = switch_mode;
  ibus->im.is_active = is_active;
  ibus->im.focused = focused;
  ibus->im.unfocused = unfocused;

  bl_slist_insert_head(ibus_list, ibus);

  ref_count++;

#ifdef IM_IBUS_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)ibus;

error:
  if (ref_count == 0) {
    remove_event_source(1);

    ibus_object_destroy((IBusObject*)ibus_bus);
    ibus_bus = NULL;

    if (parser_utf8) {
      (*parser_utf8->delete)(parser_utf8);
      parser_utf8 = NULL;
    }
  }

  if (ibus) {
    if (ibus->conv) {
      (*ibus->conv->delete)(ibus->conv);
    }

#ifdef USE_IM_CANDIDATE_SCREEN
    if (ibus->parser_term) {
      (*ibus->parser_term->delete)(ibus->parser_term);
    }
#endif

    free(ibus);
  }

  return NULL;
}

/* --- module entry point for external tools --- */

im_info_t *im_ibus_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->id = strdup("ibus");
  result->name = strdup("iBus");
  result->num_args = 0;
  result->args = NULL;
  result->readable_args = NULL;

  return result;
}
