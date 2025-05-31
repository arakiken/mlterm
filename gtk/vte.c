/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#undef VTE_SEAL_ENABLE
#define VTE_COMPILATION /* Don't define _VTE_GNUC_NONNULL as __attribute__(__nonnull__) */
#include <vte/vte.h>
#ifndef VTE_CHECK_VERSION
#define VTE_CHECK_VERSION(a, b, c) (0)
#endif
#ifndef _VTE_GTK
#define _VTE_GTK 3
#endif

/* for wayland/ui.h */
#define COMPAT_LIBVTE

#include <pobl/bl_def.h> /* USE_WIN32API */
#ifndef USE_WIN32API
#include <sys/wait.h> /* waitpid */
#include <pwd.h>      /* getpwuid */
#endif
#include <pobl/bl_sig_child.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_mem.h>
#include <pobl/bl_path.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_privilege.h>
#include <pobl/bl_unistd.h>
#include <pobl/bl_locale.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_pty.h> /* bl_pty_helper_set_flag */
#include <pobl/bl_conf.h>
#include <vt_str_parser.h>
#include <vt_term_manager.h>
#include <vt_pty_intern.h> /* XXX for config_menu.pid */
#include <ui_screen.h>
#include <ui_xic.h>
#include <ui_main_config.h>
#include <ui_imagelib.h>
#include <ui_selection_encoding.h>
#ifdef USE_BRLAPI
#include <ui_brltty.h>
#endif

#include "../main/version.h"

#include "marshal.h"

#if !VTE_CHECK_VERSION(0, 38, 0)
#include <vte/reaper.h>
#else
typedef struct _VteReaper VteReaper;
VteReaper *vte_reaper_get(void);
int vte_reaper_add_child(GPid pid);
#endif

#if defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#if 0
#define __DEBUG
#endif

#ifndef I_
#define I_(a) a
#endif

#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_adjustment_get_upper(adj) ((adj)->upper)
#define gtk_adjustment_get_value(adj) ((adj)->value)
#define gtk_adjustment_get_page_size(adj) ((adj)->page_size)
#define gtk_widget_get_window(widget) ((widget)->window)
#endif

#if !GTK_CHECK_VERSION(2, 18, 0)
#define gtk_widget_set_window(widget, win) ((widget)->window = (win))
#define gtk_widget_set_allocation(widget, alloc) ((widget)->allocation = *(alloc))
#define gtk_widget_get_allocation(widget, alloc) (*(alloc) = (widget)->allocation)
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
#define GTK_WIDGET_SET_REALIZED(widget) gtk_widget_set_realized(widget, TRUE)
#define GTK_WIDGET_UNSET_REALIZED(widget) gtk_widget_set_realized(widget, FALSE)
#define GTK_WIDGET_REALIZED(widget) gtk_widget_get_realized(widget)
#define GTK_WIDGET_SET_HAS_FOCUS(widget) (0)
#define GTK_WIDGET_UNSET_HAS_FOCUS(widget) (0)
#define GTK_WIDGET_UNSET_MAPPED(widget) gtk_widget_set_mapped(widget, FALSE)
#define GTK_WIDGET_MAPPED(widget) gtk_widget_get_mapped(widget)
#define GTK_WIDGET_SET_CAN_FOCUS(widget) gtk_widget_set_can_focus(widget, TRUE)
#else /* GTK_CHECK_VERSION(2,90,0) */
#define GTK_WIDGET_SET_REALIZED(widget) GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED)
#define GTK_WIDGET_UNSET_REALIZED(widget) GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED)
#define GTK_WIDGET_SET_HAS_FOCUS(widget) GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS)
#define GTK_WIDGET_UNSET_HAS_FOCUS(widget) GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS)
#define GTK_WIDGET_UNSET_MAPPED(widget) GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED)
#define GTK_WIDGET_SET_CAN_FOCUS(widget) GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS)
#endif /* GTK_CHECK_VERSION(2,90,0) */

#define VTE_WIDGET(screen) ((VteTerminal *)(screen)->system_listener->self)
/* XXX Hack to distinguish ui_screen_t from ui_{candidate|status}_screent_t */
#define IS_MLTERM_SCREEN(win) (!PARENT_WINDOWID_IS_TOP(win))

#if !VTE_CHECK_VERSION(0, 38, 0)
#define WINDOW_MARGIN 1
#define VteCursorBlinkMode VteTerminalCursorBlinkMode
#define VteCursorShape VteTerminalCursorShape
#define VteEraseBinding VteTerminalEraseBinding
#endif

#define STATIC_PARAMS (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#if VTE_CHECK_VERSION(0, 46, 0)
struct _VteRegex {
  int ref_count;
  GRegex *gregex;
};

G_DEFINE_BOXED_TYPE(VteRegex, vte_regex,
                    vte_regex_ref, (GBoxedFreeFunc)vte_regex_unref)
G_DEFINE_QUARK(vte-regex-error, vte_regex_error)
#endif

#define VTE_TERMINAL_CSS_NAME "vte-terminal"

#if VTE_CHECK_VERSION(0, 40, 0)
typedef struct _VteTerminalPrivate VteTerminalPrivate;
#endif

struct _VteTerminalPrivate {
  /* Not NULL until finalized. screen->term is NULL until widget is realized. */
  ui_screen_t *screen;

  /*
   * Not NULL until finalized. term->pty is NULL until pty is forked or
   * inherited from parent.
   */
  vt_term_t *term;

#if VTE_CHECK_VERSION(0, 26, 0)
  VtePty *pty;
#endif

  ui_system_event_listener_t system_listener;

  void (*line_scrolled_out)(void *);
  void (*xterm_resize)(void *, u_int, u_int, int, int);
  ui_screen_scroll_event_listener_t screen_scroll_listener;
  int8_t adj_value_changed_by_myself;

  /* for roxterm-2.6.5 */
  int8_t init_char_size;

  GIOChannel *io;
  guint src_id;

  GdkPixbuf *image; /* Original image which vte_terminal_set_background_image passed */
  Pixmap pixmap;
  u_int pix_width;
  u_int pix_height;
  ui_picture_modifier_t *pic_mod; /* caching previous pic_mod in update_wall_picture.*/

#if GLIB_CHECK_VERSION(2, 14, 0)
  GRegex *gregex;
  GRegex **match_gregexes;
#if VTE_CHECK_VERSION(0, 46, 0)
  VteRegex *vregex;
  VteRegex **match_vregexes;
  u_int16_t num_match_vregexes;
#endif
  u_int16_t num_match_gregexes;
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
  GtkAdjustment *adjustment;
  gchar *window_title;
  gchar *icon_title;
  glong char_width;
  glong char_height;
  glong row_count;
  glong column_count;
#if VTE_CHECK_VERSION(0, 40, 0)
#define ADJUSTMENT(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->adjustment
#define WINDOW_TITLE(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->window_title
#define ICON_TITLE(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->icon_title
#define CHAR_WIDTH(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->char_width
#define CHAR_HEIGHT(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->char_height
#define ROW_COUNT(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->row_count
#define COLUMN_COUNT(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])->column_count
#else
#define ADJUSTMENT(terminal) (terminal)->pvt->adjustment
#define WINDOW_TITLE(terminal) (terminal)->pvt->window_title
#define ICON_TITLE(terminal) (terminal)->pvt->icon_title
#define CHAR_WIDTH(terminal) (terminal)->pvt->char_width
#define CHAR_HEIGHT(terminal) (terminal)->pvt->char_height
#define ROW_COUNT(terminal) (terminal)->pvt->row_count
#define COLUMN_COUNT(terminal) (terminal)->pvt->column_count
#endif
#else
#define ADJUSTMENT(terminal) (terminal)->adjustment
#define WINDOW_TITLE(terminal) (terminal)->window_title
#define ICON_TITLE(terminal) (terminal)->icon_title
#define CHAR_WIDTH(terminal) (terminal)->char_width
#define CHAR_HEIGHT(terminal) (terminal)->char_height
#define ROW_COUNT(terminal) (terminal)->row_count
#define COLUMN_COUNT(terminal) (terminal)->column_count
#endif
};

#if VTE_CHECK_VERSION(0, 40, 0)
#define PVT(terminal) ((VteTerminalPrivate*)(terminal)->_unused_padding[0])
#else
#define PVT(terminal) (terminal)->pvt
#endif

enum {
  SIGNAL_BELL,
  SIGNAL_CHAR_SIZE_CHANGED,
  SIGNAL_CHILD_EXITED,
  SIGNAL_COMMIT,
  SIGNAL_CONTENTS_CHANGED,
  SIGNAL_COPY_CLIPBOARD,
  SIGNAL_CURRENT_DIRECTORY_URI_CHANGED,
  SIGNAL_CURRENT_FILE_URI_CHANGED,
  SIGNAL_CURSOR_MOVED,
  SIGNAL_DECREASE_FONT_SIZE,
  SIGNAL_DEICONIFY_WINDOW,
  SIGNAL_ENCODING_CHANGED,
  SIGNAL_EOF,
  SIGNAL_ICON_TITLE_CHANGED,
  SIGNAL_ICONIFY_WINDOW,
  SIGNAL_INCREASE_FONT_SIZE,
  SIGNAL_LOWER_WINDOW,
  SIGNAL_MAXIMIZE_WINDOW,
  SIGNAL_MOVE_WINDOW,
  SIGNAL_PASTE_CLIPBOARD,
  SIGNAL_RAISE_WINDOW,
  SIGNAL_REFRESH_WINDOW,
  SIGNAL_RESIZE_WINDOW,
  SIGNAL_RESTORE_WINDOW,
  SIGNAL_SELECTION_CHANGED,
  SIGNAL_TEXT_DELETED,
  SIGNAL_TEXT_INSERTED,
  SIGNAL_TEXT_MODIFIED,
  SIGNAL_TEXT_SCROLLED,
  SIGNAL_WINDOW_TITLE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
#if GTK_CHECK_VERSION(2, 90, 0)
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
#endif
  PROP_ALLOW_BOLD,
  PROP_AUDIBLE_BELL,
  PROP_BACKGROUND_IMAGE_FILE,
  PROP_BACKGROUND_IMAGE_PIXBUF,
  PROP_BACKGROUND_OPACITY,
  PROP_BACKGROUND_SATURATION,
  PROP_BACKGROUND_TINT_COLOR,
  PROP_BACKGROUND_TRANSPARENT,
  PROP_BACKSPACE_BINDING,
  PROP_CURSOR_BLINK_MODE,
  PROP_CURSOR_SHAPE,
  PROP_DELETE_BINDING,
  PROP_EMULATION,
  PROP_ENCODING,
  PROP_FONT_DESC,
  PROP_ICON_TITLE,
  PROP_MOUSE_POINTER_AUTOHIDE,
  PROP_PTY,
  PROP_SCROLL_BACKGROUND,
  PROP_SCROLLBACK_LINES,
  PROP_SCROLL_ON_KEYSTROKE,
  PROP_SCROLL_ON_OUTPUT,
  PROP_WINDOW_TITLE,
  PROP_WORD_CHARS,
  PROP_VISIBLE_BELL
};

#if GTK_CHECK_VERSION(2, 90, 0)
struct _VteTerminalClassPrivate {
  GtkStyleProvider *style_provider;
};

G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
                        g_type_add_class_private(g_define_type_id, sizeof(VteTerminalClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))
#else
G_DEFINE_TYPE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET);
#endif

#if GLIB_CHECK_VERSION(2, 14, 0)
static int match_gregex(size_t *beg, size_t *len, void *gregex, u_char *str, int backward);
#if VTE_CHECK_VERSION(0, 46, 0)
static int match_vteregex(size_t *beg, size_t *len, void *vregex, u_char *str, int backward);
#define IS_VTE_SEARCH(terminal) \
  (PVT(terminal)->screen->term->screen->search && \
   (PVT(terminal)->screen->term->screen->search->match == match_gregex || \
    PVT(terminal)->screen->term->screen->search->match == match_vteregex))
#else
#define IS_VTE_SEARCH(terminal) \
  (PVT(terminal)->screen->term->screen->search && \
   PVT(terminal)->screen->term->screen->search->match == match_gregex)
#endif
#else
#define IS_VTE_SEARCH(terminal) (0)
#endif

/* --- static variables --- */

static ui_main_config_t main_config;
static ui_shortcut_t shortcut;
static ui_display_t disp;

#if VTE_CHECK_VERSION(0, 19, 0)
static guint signals[LAST_SIGNAL];
#endif

static int (*orig_select_in_window)(void *);

static int is_sending_data;

#if defined(USE_XLIB)
#include "vte_xlib.c"
#elif defined(USE_WAYLAND)
#include "vte_wayland.c"
#elif defined(USE_WIN32API)
#include "vte_win32.c"
#else
#error "Unsupported platform for libvte compatible library."
#endif

/* --- static functions --- */

#if defined(__DEBUG) && defined(USE_XLIB)
static int error_handler(Display *display, XErrorEvent *event) {
  char buffer[1024];

  XGetErrorText(display, event->error_code, buffer, 1024);

  bl_msg_printf("%s\n", buffer);

  abort();

  return 1;
}
#endif

static int select_in_window(void *p) {
  int ret = (*orig_select_in_window)(p);
#if VTE_CHECK_VERSION(0, 19, 0)
  g_signal_emit(VTE_WIDGET((ui_screen_t*)p), signals[SIGNAL_SELECTION_CHANGED], 0);
#else
  g_signal_emit_by_name(VTE_WIDGET((ui_screen_t*)p), "selection-changed");
#endif

  return ret;
}

static int selection(ui_selection_t *sel, int char_index_1, int row_1, int char_index_2,
                     int row_2) {
  ui_sel_clear(sel);

  ui_start_selection(sel, char_index_1 - 1, row_1, char_index_1, row_1, SEL_CHAR, 0);
  ui_selecting(sel, char_index_2, row_2);
  ui_stop_selecting(sel);

  return 1;
}

#if GLIB_CHECK_VERSION(2, 14, 0)
static int match_gregex(size_t *beg, size_t *len, void *gregex, u_char *str, int backward) {
  GMatchInfo *info;

  if (g_regex_match(gregex, str, 0, &info)) {
    gchar *word;
    u_char *p;

    p = str;

    do {
      word = g_match_info_fetch(info, 0);

      p = strstr(p, word);
      *beg = p - str;
      *len = strlen(word);

      g_free(word);

      p += (*len);
    } while (g_match_info_next(info, NULL));

    g_match_info_free(info);

    return 1;
  }

  return 0;
}

#if VTE_CHECK_VERSION(0, 46, 0)
static int match_vteregex(size_t *beg, size_t *len, void *vregex, u_char *str, int backward) {
  return match_gregex(beg, len, ((VteRegex*)vregex)->gregex, str, backward);
}
#endif

static gboolean search_find(VteTerminal *terminal, int backward) {
  int beg_char_index;
  int beg_row;
  int end_char_index;
  int end_row;
  void *regex;

  if (!GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    return FALSE;
  }

#if VTE_CHECK_VERSION(0, 46, 0)
  regex = PVT(terminal)->gregex ? ((void*)PVT(terminal)->gregex) : ((void*)PVT(terminal)->vregex);
#else
  regex = PVT(terminal)->gregex;
#endif

  if (!regex) {
    return FALSE;
  }

  if (!IS_VTE_SEARCH(terminal)) {
    return FALSE;
  }

  if (vt_term_search_find(PVT(terminal)->term, &beg_char_index, &beg_row, &end_char_index, &end_row,
                          regex, backward)) {
    gdouble value;

    selection(&PVT(terminal)->screen->sel, beg_char_index, beg_row, end_char_index, end_row);

    value = vt_term_get_num_logged_lines(PVT(terminal)->term) + (beg_row >= 0 ? 0 : beg_row);

#if GTK_CHECK_VERSION(2, 14, 0)
    gtk_adjustment_set_value(ADJUSTMENT(terminal), value);
#else
    ADJUSTMENT(terminal)->value = value;
    gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif

    /*
     * XXX
     * Dirty hack, but without this, selection() above is not reflected to
     * window
     * if aother word is hit in the same line. (If row is not changed,
     * gtk_adjustment_set_value() doesn't call ui_screen_scroll_to().)
     */
    ui_window_update(&PVT(terminal)->screen->window, 1 /* UPDATE_SCREEN */);

    return TRUE;
  } else {
    return FALSE;
  }
}
#endif

#if !GTK_CHECK_VERSION(2, 12, 0)
/* gdk_color_to_string() was not supported by gtk+ < 2.12. */
static gchar *gdk_color_to_string(const GdkColor *color) {
  gchar *str;

  if ((str = g_malloc(14)) == NULL) {
    return NULL;
  }

  sprintf(str, "#%04x%04x%04x", color->red, color->green, color->blue);

  return str;
}
#endif

#if GTK_CHECK_VERSION(2, 99, 0)
static gchar *gdk_rgba_to_string2(const GdkRGBA *color) {
  gchar *str;

  if ((str = g_malloc(10)) == NULL) {
    return NULL;
  }

#ifdef USE_WIN32API
  /* XXX alpha is always 0xfe not to make fg/bg colors opaque. */
  sprintf(str, "#%02x%02x%02xfe",
          (int)((color->red > 0.999 ? 1 : color->red) * 255 + 0.5),
          (int)((color->green > 0.999 ? 1 : color->green) * 255 + 0.5),
          (int)((color->blue > 0.999 ? 1 : color->blue) * 255 + 0.5));
#else
  sprintf(str, color->alpha > 0.999 ? "#%02x%02x%02x" : "#%02x%02x%02x%02x",
          (int)((color->red > 0.999 ? 1 : color->red) * 255 + 0.5),
          (int)((color->green > 0.999 ? 1 : color->green) * 255 + 0.5),
          (int)((color->blue > 0.999 ? 1 : color->blue) * 255 + 0.5),
          (int)((color->alpha > 0.999 ? 1 : color->alpha) * 255 + 0.5));
#endif

  return str;
}
#endif

static int is_initial_allocation(GtkAllocation *allocation) {
  /* { -1 , -1 , 1 , 1 } is default value of GtkAllocation. */
  return (allocation->x == -1 && allocation->y == -1 && allocation->width == 1 &&
          allocation->height == 1);
}

#ifndef USE_WIN32API
static gboolean close_dead_terms(gpointer data) {
  vt_close_dead_terms();

  return FALSE; /* Never repeat to call this function. */
}
#endif

static void catch_child_exited(VteReaper *reaper, int pid, int status, VteTerminal *terminal) {
  /*
   * Don't use child-exited event of VteReaper in win32 where
   * wait_child_exited() in vt_pty_win32.c monitors child processes.
   */
#ifndef USE_WIN32API
  bl_trigger_sig_child(pid);

  g_timeout_add_full(G_PRIORITY_HIGH, 0, close_dead_terms, NULL, NULL);
#endif
}

static gboolean transfer_data(gpointer data) {
  vt_term_parse_vt100_sequence((vt_term_t*)data);

  if (!vt_term_is_sending_data((vt_term_t*)data)) {
    is_sending_data = 0;

    return FALSE; /* disable this timeout function */
  } else {
    return TRUE;
  }
}

#ifndef USE_WIN32API
/*
 * This handler works even if VteTerminal widget is not realized and vt_term_t
 * is not attached to ui_screen_t.
 * That's why the time ui_screen_attach is called is delayed (in
 * vte_terminal_fork* or vte_terminal_realized).
 *
 * See window_proc() in vte_win32.c
 */
static gboolean vte_terminal_io(GIOChannel *source, GIOCondition conditon,
                                gpointer data /* vt_term_t */) {
  vt_term_parse_vt100_sequence((vt_term_t *)data);

  if (!is_sending_data && vt_term_is_sending_data((vt_term_t *)data)) {
#if GTK_CHECK_VERSION(2, 12, 0)
    gdk_threads_add_timeout(1, transfer_data, data);
#else
    g_timeout_add(1, transfer_data, data);
#endif
    is_sending_data = 1;
  }

  vt_close_dead_terms();

  return TRUE;
}
#endif

static void create_io(VteTerminal *terminal) {
#ifdef USE_WIN32API
  /*
   * If you use g_io_channel_win32_new_fd(), define USE_OPEN_OSFHANDLE in
   * vt_pty_win32.c.
   *
   * g_io_channel_win32_new_fd() is disabled for now because it locks the fd
   * and g_io_channel_read() must be used to read bytes from it.
   * https://developer-old.gnome.org/glib/unstable/glib-IO-Channels.html#g-io-channel-win32-new-fd
   */
#if 0
  PVT(terminal)->io = g_io_channel_win32_new_fd(vt_term_get_master_fd(PVT(terminal)->term));
  PVT(terminal)->src_id =
    g_io_add_watch(PVT(terminal)->io, G_IO_IN, vte_terminal_io, PVT(terminal)->term);
#endif
#else
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Create GIO of pty master %d\n",
                  vt_term_get_master_fd(PVT(terminal)->term));
#endif

  PVT(terminal)->io = g_io_channel_unix_new(vt_term_get_master_fd(PVT(terminal)->term));
  PVT(terminal)->src_id =
      g_io_add_watch(PVT(terminal)->io, G_IO_IN, vte_terminal_io, PVT(terminal)->term);
#endif
}

static void destroy_io(VteTerminal *terminal) {
  if (PVT(terminal)->io) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Destroy GIO of pty master %d\n",
                    vt_term_get_master_fd(PVT(terminal)->term));
#endif

    g_source_destroy(g_main_context_find_source_by_id(NULL, PVT(terminal)->src_id));
#if 0
    g_io_channel_shutdown(PVT(terminal)->io, TRUE, NULL);
#endif
    g_io_channel_unref(PVT(terminal)->io);
    PVT(terminal)->src_id = 0;
    PVT(terminal)->io = NULL;
  }
}

/*
 * vt_pty_event_listener_t overriding handler.
 */
static void pty_closed(void *p /* screen->term->pty is NULL */
                       ) {
  ui_screen_t *screen;
  vt_term_t *term;
  VteTerminal *terminal;

  screen = p;
  terminal = VTE_WIDGET(screen);
  destroy_io(terminal);

  if ((term = vt_get_detached_term(NULL))) {
    PVT(terminal)->term = term;
    create_io(terminal);

    /*
     * Not screen->term but screen->term->pty is being destroyed in
     * vt_close_dead_terms()
     * because of vt_term_manager_enable_zombie_pty(1) in
     * vte_terminal_class_init().
     */
    term = screen->term;
    ui_screen_detach(screen);
    vt_destroy_term(term);

    /* It is after widget is realized that ui_screen_attach can be called. */
    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
      ui_screen_attach(screen, PVT(terminal)->term);

#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " pty is closed and detached pty is re-attached.\n");
#endif
    }
  } else {
#if VTE_CHECK_VERSION(0, 38, 0)
    g_signal_emit_by_name(terminal, "child-exited", 0);
#else
    g_signal_emit_by_name(terminal, "child-exited");
#endif

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " pty is closed\n");
#endif
  }
}

/*
 * ui_system_event_listener_t handlers
 */

static void font_config_updated(void) {
  u_int count;

  ui_font_cache_unload_all();

  for (count = 0; count < disp.num_roots; count++) {
    if (IS_MLTERM_SCREEN(disp.roots[count])) {
      ui_screen_reset_view((ui_screen_t *)disp.roots[count]);
    }
  }
}

static void color_config_updated(void) {
  u_int count;

  ui_color_cache_unload_all();

  for (count = 0; count < disp.num_roots; count++) {
    if (IS_MLTERM_SCREEN(disp.roots[count])) {
      ui_screen_reset_view((ui_screen_t *)disp.roots[count]);
    }
  }
}

static void open_pty(void *p, ui_screen_t *screen, char *dev) {
  if (dev) {
    vt_term_t *new;

    if ((new = vt_get_detached_term(dev))) {
      VteTerminal *terminal = VTE_WIDGET(screen);
      destroy_io(terminal);
      PVT(terminal)->term = new;
      create_io(terminal);

      ui_screen_detach(screen);

      /* It is after widget is reailzed that ui_screen_attach can be called. */
      if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
        ui_screen_attach(screen, new);
      }
    }
  }
}

/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
#ifdef BL_DEBUG
#include <pobl/bl_locale.h> /* bl_locale_final */
#endif
static void __exit(void *p, int status) {
#ifdef BL_DEBUG
  u_int count;

#if 1
  bl_mem_dump_all();
#endif

  vt_free_word_separators();
  ui_free_mod_meta_prefix();
  bl_set_msg_log_file_name(NULL);

  /*
   * Don't loop from 0 to dis.num_roots owing to processing inside
   * ui_display_remove_root.
   */
  for (count = disp.num_roots; count > 0; count--) {
    if (IS_MLTERM_SCREEN(disp.roots[count - 1])) {
      gtk_widget_destroy(GTK_WIDGET(VTE_WIDGET((ui_screen_t *)disp.roots[count - 1])));
    } else {
      ui_display_remove_root(&disp, disp.roots[count - 1]);
    }
  }
  free(disp.roots);
  ui_gc_destroy(disp.gc);
#ifdef USE_XLIB
  ui_xim_display_closed(disp.display);
#endif
  ui_picture_display_closed(disp.display);

#ifdef USE_BRLAPI
  ui_brltty_final();
#endif

  vt_term_manager_final();
  bl_locale_final();
  ui_main_config_final(&main_config);
  vt_color_config_final();
  ui_shortcut_final(&shortcut);
#ifdef USE_XLIB
  ui_xim_final();
#endif
  bl_sig_child_final();

  bl_alloca_garbage_collect();

  bl_msg_printf("reporting unfreed memories --->\n");
  bl_mem_free_all();

  bl_dl_close_all();
#endif

#if 1
  exit(1);
#else
  gtk_main_quit();
#endif
}

/*
 * vt_xterm_event_listener_t (overriding) handlers
 */

static void xterm_resize(void *p, u_int width, u_int height, int mx_flag, int sb_flag) {
  ui_screen_t *screen = p;
  VteTerminal *terminal = VTE_WIDGET(screen);

  if (mx_flag) {
    mx_flag --; /* converting to ui_maximize_flag_t */
    if (mx_flag == MAXIMIZE_FULL) {
      gtk_window_maximize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(terminal))));
    } else if (mx_flag == MAXIMIZE_RESTORE) {
      gtk_window_unmaximize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(terminal))));
    }
  } else {
    (*PVT(terminal)->xterm_resize)(p, width, height, 0, 0);
  }
}

static void set_window_name(void *p, u_char *name) {
  ui_screen_t *screen;
  VteTerminal *terminal;

  screen = p;
  terminal = VTE_WIDGET(screen);

  WINDOW_TITLE(terminal) = vt_term_window_name(screen->term);

  gdk_window_set_title(gtk_widget_get_window(GTK_WIDGET(terminal)),
                       WINDOW_TITLE(terminal));
  g_signal_emit_by_name(terminal, "window-title-changed");

#if VTE_CHECK_VERSION(0, 20, 0)
  g_object_notify(G_OBJECT(terminal), "window-title");
#endif
}

static void set_icon_name(void *p, u_char *name) {
  ui_screen_t *screen;
  VteTerminal *terminal;

  screen = p;
  terminal = VTE_WIDGET(screen);

  ICON_TITLE(terminal) = vt_term_icon_name(screen->term);

  gdk_window_set_icon_name(gtk_widget_get_window(GTK_WIDGET(terminal)),
                           ICON_TITLE(terminal));
  g_signal_emit_by_name(terminal, "icon-title-changed");

#if VTE_CHECK_VERSION(0, 20, 0)
  g_object_notify(G_OBJECT(terminal), "icon-title");
#endif
}

/*
 * vt_screen_event_listener_t (overriding) handler
 */

static void line_scrolled_out(void *p /* must be ui_screen_t */
                              ) {
  ui_screen_t *screen;
  VteTerminal *terminal;
  gdouble upper;
  gdouble value;

  screen = p;
  terminal = VTE_WIDGET(screen);

  (*PVT(terminal)->line_scrolled_out)(p);

  /*
   * line_scrolled_out is called in vt100 mode
   * (after vt_xterm_event_listener_t::start_vt100 event), so
   * don't call ui_screen_scroll_to() in adjustment_value_changed()
   * in this context.
   */
  PVT(terminal)->adj_value_changed_by_myself = 1;

  value = gtk_adjustment_get_value(ADJUSTMENT(terminal));

  if ((upper = gtk_adjustment_get_upper(ADJUSTMENT(terminal))) <
      vt_term_get_log_size(PVT(terminal)->term) + ROW_COUNT(terminal)) {
#if GTK_CHECK_VERSION(2, 14, 0)
    gtk_adjustment_set_upper(ADJUSTMENT(terminal), upper + 1);
#else
    ADJUSTMENT(terminal)->upper++;
    gtk_adjustment_changed(ADJUSTMENT(terminal));
#endif

    if (vt_term_is_backscrolling(PVT(terminal)->term) != BSM_STATIC) {
#if GTK_CHECK_VERSION(2, 14, 0)
      gtk_adjustment_set_value(ADJUSTMENT(terminal), value + 1);
#else
      ADJUSTMENT(terminal)->value++;
      gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
    }
  } else if (vt_term_is_backscrolling(PVT(terminal)->term) == BSM_STATIC && value > 0) {
#if GTK_CHECK_VERSION(2, 14, 0)
    gtk_adjustment_set_value(ADJUSTMENT(terminal), value - 1);
#else
    ADJUSTMENT(terminal)->value--;
    gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " line_scrolled_out upper %f value %f\n",
                  gtk_adjustment_get_upper(ADJUSTMENT(terminal)),
                  gtk_adjustment_get_value(ADJUSTMENT(terminal)));
#endif
}

/*
 * ui_screen_scroll_event_listener_t handlers
 */

static void bs_mode_exited(void *p) {
  VteTerminal *terminal;
  int upper;
  int page_size;

  terminal = p;

  PVT(terminal)->adj_value_changed_by_myself = 1;

  upper = gtk_adjustment_get_upper(ADJUSTMENT(terminal));
  page_size = gtk_adjustment_get_page_size(ADJUSTMENT(terminal));

#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_value(ADJUSTMENT(terminal), upper - page_size);
#else
  ADJUSTMENT(terminal)->value = upper - page_size;
  gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " bs_mode_exited upper %d page_size %d\n", upper, page_size);
#endif
}

static void scrolled_upward(void *p, u_int size) {
  VteTerminal *terminal;
  int value;
  int upper;
  int page_size;

  terminal = p;

  value = gtk_adjustment_get_value(ADJUSTMENT(terminal));
  upper = gtk_adjustment_get_upper(ADJUSTMENT(terminal));
  page_size = gtk_adjustment_get_page_size(ADJUSTMENT(terminal));

  if (value + page_size >= upper) {
    return;
  }

  if (value + page_size + size > upper) {
    size = upper - value - page_size;
  }

  PVT(terminal)->adj_value_changed_by_myself = 1;

#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_value(ADJUSTMENT(terminal), value + size);
#else
  ADJUSTMENT(terminal)->value += size;
  gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
}

static void scrolled_downward(void *p, u_int size) {
  VteTerminal *terminal;
  int value;

  terminal = p;

  if ((value = gtk_adjustment_get_value(ADJUSTMENT(terminal))) == 0) {
    return;
  }

  if (value < size) {
    value = size;
  }

  PVT(terminal)->adj_value_changed_by_myself = 1;

#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_value(ADJUSTMENT(terminal), value - size);
#else
  ADJUSTMENT(terminal)->value -= size;
  gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
}

static void scrolled_to(void *p, int row) {
  VteTerminal *terminal;

  terminal = p;

  PVT(terminal)->adj_value_changed_by_myself = 1;

#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_value(ADJUSTMENT(terminal),
                           vt_term_get_num_logged_lines(PVT(terminal)->term) + row);
#else
  ADJUSTMENT(terminal)->value = row;
  gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
}

static void log_size_changed(void *p, u_int log_size) {
  VteTerminal *terminal;
  int upper;
  int page_size;

  terminal = p;

  PVT(terminal)->adj_value_changed_by_myself = 1;

  upper = gtk_adjustment_get_upper(ADJUSTMENT(terminal));
  page_size = gtk_adjustment_get_page_size(ADJUSTMENT(terminal));
  if (upper > log_size + page_size) {
#if GTK_CHECK_VERSION(2, 14, 0)
    gtk_adjustment_set_upper(ADJUSTMENT(terminal), log_size + page_size);
    gtk_adjustment_set_value(ADJUSTMENT(terminal), log_size);
#else
    ADJUSTMENT(terminal)->upper = log_size + page_size;
    ADJUSTMENT(terminal)->value = log_size;
    gtk_adjustment_changed(ADJUSTMENT(terminal));
    gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
  }
}

static void term_changed(void *p, u_int log_size, u_int logged_lines) {
  VteTerminal *terminal;
  int page_size;

  terminal = p;

  PVT(terminal)->adj_value_changed_by_myself = 1;

  page_size = gtk_adjustment_get_page_size(ADJUSTMENT(terminal));
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_upper(ADJUSTMENT(terminal), logged_lines + page_size);
  gtk_adjustment_set_value(ADJUSTMENT(terminal), logged_lines);
#else
  ADJUSTMENT(terminal)->upper = logged_lines + page_size;
  ADJUSTMENT(terminal)->value = logged_lines;
  gtk_adjustment_changed(ADJUSTMENT(terminal));
  gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
}

static void adjustment_value_changed(VteTerminal *terminal) {
  int value;
  int upper;
  int page_size;

  if (PVT(terminal)->adj_value_changed_by_myself) {
    PVT(terminal)->adj_value_changed_by_myself = 0;

    return;
  }

  value = gtk_adjustment_get_value(ADJUSTMENT(terminal));
  upper = gtk_adjustment_get_upper(ADJUSTMENT(terminal));
  page_size = gtk_adjustment_get_page_size(ADJUSTMENT(terminal));

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " scroll to %d\n", value - (upper - page_size));
#endif

  ui_screen_scroll_to(PVT(terminal)->screen, value - (upper - page_size));
}

static void set_adjustment(VteTerminal *terminal, GtkAdjustment *adjustment) {
  if (adjustment == ADJUSTMENT(terminal) || adjustment == NULL) {
    return;
  }

  if (ADJUSTMENT(terminal)) {
    g_signal_handlers_disconnect_by_func(ADJUSTMENT(terminal),
                                         G_CALLBACK(adjustment_value_changed), terminal);
    g_object_unref(ADJUSTMENT(terminal));
  }

  g_object_ref_sink(adjustment);
  ADJUSTMENT(terminal) = adjustment;
  g_signal_connect_swapped(ADJUSTMENT(terminal), "value-changed",
                           G_CALLBACK(adjustment_value_changed), terminal);
  PVT(terminal)->adj_value_changed_by_myself = 0;
}

static void reset_vte_size_member(VteTerminal *terminal) {
  int emit;

  emit = 0;

  if (/* If char_width == 0, reset_vte_size_member is called from
         vte_terminal_init */
      CHAR_WIDTH(terminal) != 0 &&
      CHAR_WIDTH(terminal) != ui_col_width(PVT(terminal)->screen)) {
    emit = 1;
  }
  CHAR_WIDTH(terminal) = ui_col_width(PVT(terminal)->screen);

  if (/* If char_height == 0, reset_vte_size_member is called from
         vte_terminal_init */
      CHAR_HEIGHT(terminal) != 0 &&
      CHAR_HEIGHT(terminal) != ui_line_height(PVT(terminal)->screen)) {
    emit = 1;
  }
  CHAR_HEIGHT(terminal) = ui_line_height(PVT(terminal)->screen);

  if (emit) {
    g_signal_emit_by_name(terminal, "char-size-changed", CHAR_WIDTH(terminal),
                          CHAR_HEIGHT(terminal));
  }

#if !VTE_CHECK_VERSION(0, 38, 0)
  terminal->char_ascent = ui_line_ascent(PVT(terminal)->screen);
  terminal->char_descent = CHAR_HEIGHT(terminal) - terminal->char_ascent;
#endif

  emit = 0;

  if (/* If row_count == 0, reset_vte_size_member is called from
         vte_terminal_init */
      ROW_COUNT(terminal) != 0 &&
      ROW_COUNT(terminal) != vt_term_get_rows(PVT(terminal)->term)) {
    emit = 1;
  }

  ROW_COUNT(terminal) = vt_term_get_rows(PVT(terminal)->term);

  if (/* If column_count == 0, reset_vte_size_member is called from
         vte_terminal_init */
      COLUMN_COUNT(terminal) != 0 &&
      COLUMN_COUNT(terminal) != vt_term_get_cols(PVT(terminal)->term)) {
    emit = 1;
  }

  COLUMN_COUNT(terminal) = vt_term_get_cols(PVT(terminal)->term);

  if (emit) {
#if GTK_CHECK_VERSION(2, 14, 0)
    int value;

    value = vt_term_get_num_logged_lines(PVT(terminal)->term);
    gtk_adjustment_configure(ADJUSTMENT(terminal), value /* value */, 0 /* lower */,
                             value + ROW_COUNT(terminal) /* upper */, 1 /* step increment */,
                             ROW_COUNT(terminal) /* page increment */,
                             ROW_COUNT(terminal) /* page size */);
#else
    ADJUSTMENT(terminal)->value = vt_term_get_num_logged_lines(PVT(terminal)->term);
    ADJUSTMENT(terminal)->upper = ADJUSTMENT(terminal)->value + ROW_COUNT(terminal);
    ADJUSTMENT(terminal)->page_increment = ROW_COUNT(terminal);
    ADJUSTMENT(terminal)->page_size = ROW_COUNT(terminal);

    gtk_adjustment_changed(ADJUSTMENT(terminal));
    gtk_adjustment_value_changed(ADJUSTMENT(terminal));
#endif
  }

#if !GTK_CHECK_VERSION(2, 90, 0)
  /*
   * XXX
   * Vertical writing mode and screen_(width|height)_ratio option are not
   *supported.
   *
   * Processing similar to vte_terminal_get_preferred_{width|height}().
   */
  GTK_WIDGET(terminal)->requisition.width =
      COLUMN_COUNT(terminal) * CHAR_WIDTH(terminal) + PVT(terminal)->screen->window.hmargin * 2;
  GTK_WIDGET(terminal)->requisition.height =
      ROW_COUNT(terminal) * CHAR_HEIGHT(terminal) + PVT(terminal)->screen->window.vmargin * 2;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG
                  " char_width %d char_height %d row_count %d column_count %d "
                  "width %d height %d\n",
                  CHAR_WIDTH(terminal), CHAR_HEIGHT(terminal), ROW_COUNT(terminal),
                  COLUMN_COUNT(terminal), GTK_WIDGET(terminal)->requisition.width,
                  GTK_WIDGET(terminal)->requisition.height);
#endif
#endif /* ! GTK_CHECK_VERSION(2,90,0) */
}

static gboolean vte_terminal_timeout(gpointer data) {
/*
 * If gdk_threads_add_timeout (2.12 or later) doesn't exist,
 * call gdk_threads_{enter|leave} manually for MT-safe.
 */
#if !GTK_CHECK_VERSION(2, 12, 0)
  gdk_threads_enter();
#endif

  vt_close_dead_terms();

  ui_display_idling(&disp);

#if !GTK_CHECK_VERSION(2, 12, 0)
  gdk_threads_leave();
#endif

  return TRUE;
}

static void vte_terminal_finalize(GObject *obj) {
  VteTerminal *terminal;
  GtkSettings *settings;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " vte terminal finalized.\n");
#endif

  terminal = VTE_TERMINAL(obj);

#if GLIB_CHECK_VERSION(2, 14, 0)
  if (PVT(terminal)->gregex) {
    g_regex_unref(PVT(terminal)->gregex);
    PVT(terminal)->gregex = NULL;
  }

#if VTE_CHECK_VERSION(0, 46, 0)
  if (PVT(terminal)->vregex) {
    vte_regex_unref(PVT(terminal)->vregex);
    PVT(terminal)->vregex = NULL;
  }
#endif
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
  vte_terminal_match_remove_all(terminal);
#else
  vte_terminal_match_clear_all(terminal);
#endif

#if VTE_CHECK_VERSION(0, 26, 0)
  if (PVT(terminal)->pty) {
    g_object_unref(PVT(terminal)->pty);
    PVT(terminal)->pty = NULL;
  }
#endif

  ui_font_manager_destroy(PVT(terminal)->screen->font_man);
  ui_color_manager_destroy(PVT(terminal)->screen->color_man);

  if (PVT(terminal)->image) {
    g_object_unref(PVT(terminal)->image);
    PVT(terminal)->image = NULL;
  }

  if (PVT(terminal)->pixmap) {
#ifdef USE_XLIB
    XFreePixmap(disp.display, PVT(terminal)->pixmap);
#else
    /* XXX */
#endif
    PVT(terminal)->pixmap = None;
  }

  free(PVT(terminal)->pic_mod);

  ui_window_final(&PVT(terminal)->screen->window);
  PVT(terminal)->screen = NULL;

  if (ADJUSTMENT(terminal)) {
    g_object_unref(ADJUSTMENT(terminal));
  }

  settings = gtk_widget_get_settings(GTK_WIDGET(obj));
  g_signal_handlers_disconnect_matched(settings, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, terminal);

  G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(obj);

#ifdef __DEBUG
  bl_debug_printf("End of vte_terminal_finalize() \n");
#endif
}

static void vte_terminal_get_property(GObject *obj, guint prop_id, GValue *value,
                                      GParamSpec *pspec) {
  VteTerminal *terminal;

  terminal = VTE_TERMINAL(obj);

  switch (prop_id) {
#if GTK_CHECK_VERSION(2, 90, 0)
    case PROP_VADJUSTMENT:
      g_value_set_object(value, ADJUSTMENT(terminal));
      break;
#endif

    case PROP_ICON_TITLE:
      g_value_set_string(value, vte_terminal_get_icon_title(terminal));
      break;

    case PROP_WINDOW_TITLE:
      g_value_set_string(value, vte_terminal_get_window_title(terminal));
      break;

#if 0
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
#endif
  }
}

static void vte_terminal_set_property(GObject *obj, guint prop_id, const GValue *value,
                                      GParamSpec *pspec) {
  VteTerminal *terminal;

  terminal = VTE_TERMINAL(obj);

  switch (prop_id) {
#if GTK_CHECK_VERSION(2, 90, 0)
    case PROP_VADJUSTMENT:
      set_adjustment(terminal, g_value_get_object(value));
      break;
#endif

#if 0
    case PROP_ICON_TITLE:
      set_icon_name(PVT(terminal)->screen, g_value_get_string(value));
      break;

    case PROP_WINDOW_TITLE:
      set_window_name(PVT(terminal)->screen, g_value_get_string(value));
      break;
#endif

#if 0
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
#endif
  }
}

static void init_screen(VteTerminal *terminal, ui_font_manager_t *font_man,
                        ui_color_manager_t *color_man) {
  u_int hmargin;
  u_int vmargin;

#if VTE_CHECK_VERSION(0, 38, 0)
  GtkBorder padding;

  gtk_style_context_get_padding(gtk_widget_get_style_context(GTK_WIDGET(terminal)),
                                gtk_widget_get_state_flags(GTK_WIDGET(terminal)), &padding);
  hmargin = BL_MIN(padding.left, padding.right);
  vmargin = BL_MIN(padding.top, padding.bottom);
#else
  hmargin = WINDOW_MARGIN;
  vmargin = WINDOW_MARGIN;
#endif

  /*
   * XXX
   * PVT(terminal)->term is specified to ui_screen_new in order to set
   * ui_window_t::width and height property, but screen->term is NULL
   * until widget is realized.
   */
  PVT(terminal)->screen = ui_screen_new(PVT(terminal)->term, font_man, color_man,
                                        main_config.brightness, main_config.contrast,
                                        main_config.gamma, main_config.alpha,
                                        main_config.fade_ratio, &shortcut,
                                        /* main_config.screen_width_ratio */ 100,
                                        main_config.mod_meta_key, main_config.mod_meta_mode,
                                        main_config.bel_mode, main_config.receive_string_via_ucs,
                                        main_config.pic_file_path, main_config.use_transbg,
                                        main_config.use_vertical_cursor, main_config.borderless,
                                        main_config.line_space, main_config.input_method,
                                        main_config.allow_osc52, hmargin, vmargin,
                                        main_config.hide_underline, main_config.underline_offset,
                                        main_config.baseline_offset);
  if (PVT(terminal)->term) {
    vt_term_detach(PVT(terminal)->term);
    PVT(terminal)->screen->term = NULL;
  } else {
    /*
     * PVT(terminal)->term can be NULL if this function is called from
     * vte_terminal_unrealize.
     */
  }

  memset(&PVT(terminal)->system_listener, 0, sizeof(ui_system_event_listener_t));
  PVT(terminal)->system_listener.self = terminal;
  PVT(terminal)->system_listener.font_config_updated = font_config_updated;
  PVT(terminal)->system_listener.color_config_updated = color_config_updated;
  PVT(terminal)->system_listener.open_pty = open_pty;
  PVT(terminal)->system_listener.exit = __exit;
  ui_set_system_listener(PVT(terminal)->screen, &PVT(terminal)->system_listener);

  memset(&PVT(terminal)->screen_scroll_listener, 0, sizeof(ui_screen_scroll_event_listener_t));
  PVT(terminal)->screen_scroll_listener.self = terminal;
  PVT(terminal)->screen_scroll_listener.bs_mode_exited = bs_mode_exited;
  PVT(terminal)->screen_scroll_listener.scrolled_upward = scrolled_upward;
  PVT(terminal)->screen_scroll_listener.scrolled_downward = scrolled_downward;
  PVT(terminal)->screen_scroll_listener.scrolled_to = scrolled_to;
  PVT(terminal)->screen_scroll_listener.log_size_changed = log_size_changed;
  PVT(terminal)->screen_scroll_listener.term_changed = term_changed;
  ui_set_screen_scroll_listener(PVT(terminal)->screen, &PVT(terminal)->screen_scroll_listener);

  PVT(terminal)->line_scrolled_out = PVT(terminal)->screen->screen_listener.line_scrolled_out;
  PVT(terminal)->screen->screen_listener.line_scrolled_out = line_scrolled_out;

  PVT(terminal)->screen->xterm_listener.set_window_name = set_window_name;
  PVT(terminal)->screen->xterm_listener.set_icon_name = set_icon_name;
  PVT(terminal)->xterm_resize = PVT(terminal)->screen->xterm_listener.resize;
  PVT(terminal)->screen->xterm_listener.resize = xterm_resize;

  orig_select_in_window = PVT(terminal)->screen->sel_listener.select_in_window;
  PVT(terminal)->screen->sel_listener.select_in_window = select_in_window;

  /* overriding */
  PVT(terminal)->screen->pty_listener.closed = pty_closed;

  /*
   * If the screen is finalized before show_root() is called,
   * functions related to ui_display_t (such as ui_display_clear_selection())
   * can be called in ui_window_final().
   */
  PVT(terminal)->screen->window.disp = &disp;
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_background_image(VteTerminal *terminal, GdkPixbuf *image);
void vte_terminal_set_background_image_file(VteTerminal *terminal, const char *path);
#endif

static void update_wall_picture(VteTerminal *terminal) {
  ui_window_t *win;
  ui_picture_modifier_t *pic_mod;
  GdkPixbuf *image;
  char file[7 + DIGIT_STR_LEN(PVT(terminal)->pixmap) + 1];

  if (!PVT(terminal)->image) {
    return;
  }

  win = &PVT(terminal)->screen->window;
  pic_mod = ui_screen_get_picture_modifier(PVT(terminal)->screen);

  if (PVT(terminal)->pix_width == ACTUAL_WIDTH(win) &&
      PVT(terminal)->pix_height == ACTUAL_WIDTH(win) &&
      ui_picture_modifiers_equal(pic_mod, PVT(terminal)->pic_mod) && PVT(terminal)->pixmap) {
    goto set_bg_image;
  } else if (gdk_pixbuf_get_width(PVT(terminal)->image) != ACTUAL_WIDTH(win) ||
             gdk_pixbuf_get_height(PVT(terminal)->image) != ACTUAL_HEIGHT(win)) {
#ifdef DEBUG
    bl_debug_printf(
        BL_DEBUG_TAG " Scaling bg img %d %d => %d %d\n", gdk_pixbuf_get_width(PVT(terminal)->image),
        gdk_pixbuf_get_height(PVT(terminal)->image), ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win));
#endif

    image = gdk_pixbuf_scale_simple(PVT(terminal)->image, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
                                    GDK_INTERP_BILINEAR);
  } else {
    image = PVT(terminal)->image;
  }

  if (PVT(terminal)->pixmap) {
#ifdef USE_XLIB
    XFreePixmap(disp.display, PVT(terminal)->pixmap);
#else
    /* XXX */
#endif
  }

  PVT(terminal)->pixmap = ui_imagelib_pixbuf_to_pixmap(win, pic_mod, image);

  if (image != PVT(terminal)->image) {
    g_object_unref(image);
  }

  if (PVT(terminal)->pixmap == None) {
    bl_msg_printf(
        "Failed to convert pixbuf to pixmap. "
        "Rebuild mlterm with gdk-pixbuf.\n");

    PVT(terminal)->pix_width = 0;
    PVT(terminal)->pix_height = 0;
    PVT(terminal)->pic_mod = NULL;

    return;
  }

  PVT(terminal)->pix_width = ACTUAL_WIDTH(win);
  PVT(terminal)->pix_height = ACTUAL_HEIGHT(win);
  if (pic_mod) {
    if (PVT(terminal)->pic_mod == NULL) {
      PVT(terminal)->pic_mod = malloc(sizeof(ui_picture_modifier_t));
    }

    *PVT(terminal)->pic_mod = *pic_mod;
  } else {
    free(PVT(terminal)->pic_mod);
    PVT(terminal)->pic_mod = NULL;
  }

set_bg_image:
  ui_change_true_transbg_alpha(PVT(terminal)->screen->color_man, 255);

  sprintf(file, "pixmap:%lu", PVT(terminal)->pixmap);
  vte_terminal_set_background_image_file(terminal, file);
}

static void vte_terminal_realize(GtkWidget *widget) {
  VteTerminal *terminal = VTE_TERMINAL(widget);
  GdkWindowAttr attr;
  GtkAllocation allocation;

  if (gtk_widget_get_window(widget)) {
    return;
  }

  ui_screen_attach(PVT(terminal)->screen, PVT(terminal)->term);

  gtk_widget_get_allocation(widget, &allocation);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " vte terminal realized with size x %d y %d w %d h %d\n",
                  allocation.x, allocation.y, allocation.width, allocation.height);
#endif

  GTK_WIDGET_SET_REALIZED(widget);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.x = allocation.x;
  attr.y = allocation.y;
  attr.width = allocation.width;
  attr.height = allocation.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.visual = gtk_widget_get_visual(widget);
#if !GTK_CHECK_VERSION(2, 90, 0)
  attr.colormap = gtk_widget_get_colormap(widget);
#endif
  attr.event_mask = gtk_widget_get_events(widget) | GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                    GDK_SUBSTRUCTURE_MASK; /* DestroyNotify from child */

  gtk_widget_set_window(widget,
                        gdk_window_new(gtk_widget_get_parent_window(widget), &attr,
                                       GDK_WA_X | GDK_WA_Y | (attr.visual ? GDK_WA_VISUAL : 0)
#if !GTK_CHECK_VERSION(2, 90, 0)
                                           | (attr.colormap ? GDK_WA_COLORMAP : 0)
#endif
                                           ));

#ifdef USE_WIN32API
  /* XXX Scrollbar gets transparent unexpectedly without this. */
  gtk_widget_set_opacity(widget, 0.99);
#endif

  /*
   * Note that hook key and button events in vte_terminal_filter doesn't work
   * without this.
   */
#if GTK_CHECK_VERSION(3, 8, 0)
  gtk_widget_register_window(widget, gtk_widget_get_window(widget));
#else
  gdk_window_set_user_data(gtk_widget_get_window(widget), widget);
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  if (widget->style->font_desc) {
    pango_font_description_free(widget->style->font_desc);
    widget->style->font_desc = NULL;
  }

  /* private_font(_desc) should be NULL if widget->style->font_desc is set NULL
   * above. */
  if (widget->style->private_font) {
    gdk_font_unref(widget->style->private_font);
    widget->style->private_font = NULL;
  }

  if (widget->style->private_font_desc) {
    pango_font_description_free(widget->style->private_font_desc);
    widget->style->private_font_desc = NULL;
  }
#endif

#ifdef USE_XLIB
  g_signal_connect_swapped(gtk_widget_get_toplevel(widget), "configure-event",
                           G_CALLBACK(toplevel_configure), terminal);
#endif

  show_root(&disp, widget);

  /*
   * allocation passed by size_allocate is not necessarily to be reflected
   * to ui_window_t or vt_term_t, so ui_window_resize must be called here.
   */
  if (!vt_term_is_zombie(PVT(terminal)->term) && !is_initial_allocation(&allocation)) {
    if (ui_window_resize_with_margin(&PVT(terminal)->screen->window, allocation.width,
                                     allocation.height, NOTIFY_TO_MYSELF)) {
      reset_vte_size_member(terminal);
    }
  }

  update_wall_picture(terminal);
}

static void vte_terminal_unrealize(GtkWidget *widget) {
  VteTerminal *terminal = VTE_TERMINAL(widget);
  ui_screen_t *screen = PVT(terminal)->screen;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " vte terminal unrealized.\n");
#endif

  ui_screen_detach(screen);

  if (vt_term_is_zombie(PVT(terminal)->term)) {
    /*
     * PVT(terminal)->term was not destroyed in pty_closed() and remains
     * in a terms list of vt_term_manager as a zombie.
     */
    vt_destroy_term(PVT(terminal)->term);
    PVT(terminal)->term = NULL;
  }

  /* Create dummy screen in case terminal will be realized again. */
  init_screen(terminal, screen->font_man, screen->color_man);
  ui_display_remove_root(&disp, &screen->window);

#ifdef USE_XLIB
  g_signal_handlers_disconnect_by_func(gtk_widget_get_toplevel(GTK_WIDGET(terminal)),
                                       G_CALLBACK(toplevel_configure), terminal);
#endif

  /* gtk_widget_unregister_window() and gdk_window_destroy() are called. */
  GTK_WIDGET_CLASS(vte_terminal_parent_class)->unrealize(widget);

#ifdef __DEBUG
  bl_debug_printf("End of vte_terminal_unrealize()\n");
#endif
}

static gboolean vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event) {
  GTK_WIDGET_SET_HAS_FOCUS(widget);

  if (GTK_WIDGET_MAPPED(widget)) {
    ui_window_t *win = &PVT(VTE_TERMINAL(widget))->screen->window;
#ifdef USE_WAYLAND
    win->disp->display->wlserv->current_kbd_surface = win->disp->display->surface;
#endif

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " focus in\n");
#endif

    ui_window_set_input_focus(win);
  }

  return FALSE;
}

static gboolean vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event) {
#ifdef USE_WAYLAND
  ui_window_t *win = &PVT(VTE_TERMINAL(widget))->screen->window;
  if (win->window_unfocused) {
    win->is_focused = 0;
    (*win->window_unfocused)(win);
  }
#endif

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " focus out\n");
#endif

  GTK_WIDGET_UNSET_HAS_FOCUS(widget);

  return FALSE;
}

#if GTK_CHECK_VERSION(2, 90, 0)

static void vte_terminal_get_preferred_width(GtkWidget *widget, gint *minimum_width,
                                             gint *natural_width) {
  /* Processing similar to setting GtkWidget::requisition in
   * reset_vte_size_member(). */
  VteTerminal *terminal = VTE_TERMINAL(widget);

  if (minimum_width) {
    *minimum_width =
        CHAR_WIDTH(terminal) + PVT(terminal)->screen->window.hmargin * 2;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " preferred minimum width %d\n", *minimum_width);
#endif
  }

  if (natural_width) {
    *natural_width = COLUMN_COUNT(terminal) * CHAR_WIDTH(terminal) +
                     PVT(terminal)->screen->window.hmargin * 2;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " preferred natural width %d\n", *natural_width);
#endif
  }
}

static void vte_terminal_get_preferred_width_for_height(GtkWidget *widget, gint height,
                                                        gint *minimum_width, gint *natural_width) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " preferred width for height %d\n", height);
#endif

  vte_terminal_get_preferred_width(widget, minimum_width, natural_width);
}

static void vte_terminal_get_preferred_height(GtkWidget *widget, gint *minimum_height,
                                              gint *natural_height) {
  /* Processing similar to setting GtkWidget::requisition in
   * reset_vte_size_member(). */
  VteTerminal *terminal = VTE_TERMINAL(widget);

  /* XXX */
  if (!PVT(terminal)->init_char_size) {
    if (strstr(g_get_prgname(), "roxterm") ||
        /*
         * Hack for roxterm started by "x-terminal-emulator" or
         * "exo-open --launch TerminalEmulator" (which calls
         * "x-terminal-emulator" internally)
         */
        g_object_get_data(G_OBJECT(gtk_widget_get_parent(widget)), "roxterm_tab")) {
      /*
       * XXX
       * I don't know why, but the size of roxterm 2.6.5 (GTK+3) is
       * minimized unless "char-size-changed" signal is emit once in
       * vte_terminal_get_preferred_height() or
       * vte_terminal_get_preferred_height() in startup.
       */
      g_signal_emit_by_name(widget, "char-size-changed", CHAR_WIDTH(terminal),
                            CHAR_HEIGHT(terminal));
    }

    PVT(terminal)->init_char_size = 1;
  }

  if (minimum_height) {
    *minimum_height =
        CHAR_HEIGHT(terminal) + PVT(terminal)->screen->window.vmargin * 2;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " preferred minimum height %d\n", *minimum_height);
#endif
  }

  if (natural_height) {
    *natural_height = ROW_COUNT(terminal) * CHAR_HEIGHT(terminal) +
                      PVT(terminal)->screen->window.vmargin * 2;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " preferred natural height %d\n", *natural_height);
#endif
  }
}

static void vte_terminal_get_preferred_height_for_width(GtkWidget *widget, gint width,
                                                        gint *minimum_height,
                                                        gint *natural_height) {
#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " preferred height for width %d\n", width);
#endif

  vte_terminal_get_preferred_height(widget, minimum_height, natural_height);
}

#else /* GTK_CHECK_VERSION(2,90,0) */

static void vte_terminal_size_request(GtkWidget *widget, GtkRequisition *req) {
  *req = widget->requisition;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " size_request %d %d cur alloc %d %d\n", req->width, req->height,
                  widget->allocation.width, widget->allocation.height);
#endif
}

#endif /* GTK_CHECK_VERSION(2,90,0) */

static void vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
  int is_resized;
  GtkAllocation cur_allocation;

  gtk_widget_get_allocation(widget, &cur_allocation);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " size_allocate %d %d %d %d => %d %d %d %d\n", cur_allocation.x,
                  cur_allocation.y, cur_allocation.width, cur_allocation.height, allocation->x,
                  allocation->y, allocation->width, allocation->height);
#endif

  if (!(is_resized = (cur_allocation.width != allocation->width ||
                      cur_allocation.height != allocation->height)) &&
      cur_allocation.x == allocation->x && cur_allocation.y == allocation->y) {
    return;
  }

  gtk_widget_set_allocation(widget, allocation);

  if (GTK_WIDGET_REALIZED(widget)) {
    VteTerminal *terminal = VTE_TERMINAL(widget);
    if (is_resized && !vt_term_is_zombie(PVT(terminal)->term)) {
      /*
       * Even if ui_window_resize_with_margin returns 0,
       * reset_vte_size_member etc functions must be called,
       * because VTE_TERMNAL(widget)->pvt->screen can be already
       * resized and vte_terminal_size_allocate can be called
       * from vte_terminal_filter.
       */
      ui_window_resize_with_margin(&PVT(terminal)->screen->window, allocation->width,
                                   allocation->height, NOTIFY_TO_MYSELF);
      reset_vte_size_member(terminal);
      update_wall_picture(terminal);
      /*
       * gnome-terminal(2.29.6 or later ?) is not resized correctly
       * without this.
       */
      gtk_widget_queue_resize_no_redraw(widget);
    }

    gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x, allocation->y,
                           allocation->width, allocation->height);
#ifdef USE_WAYLAND
    /* Multiple displays can coexist on wayland, so '&disp' isn't used. */
    display_move(terminal, allocation->x, allocation->y);
#endif
  } else {
    /*
     * ui_window_resize_with_margin(widget->allocation.width, height)
     * will be called in vte_terminal_realize() or vte_terminal_fork*().
     */
  }
}

#if VTE_CHECK_VERSION(0, 38, 0)
static void vte_terminal_screen_changed(GtkWidget *widget, GdkScreen *previous_screen) {
  GdkScreen *screen;
  GtkSettings *settings;

  screen = gtk_widget_get_screen(widget);
  if (previous_screen != NULL && (screen != previous_screen || screen == NULL)) {
    settings = gtk_settings_get_for_screen(previous_screen);
    g_signal_handlers_disconnect_matched(settings, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, widget);
  }

  if (GTK_WIDGET_CLASS(vte_terminal_parent_class)->screen_changed) {
    GTK_WIDGET_CLASS(vte_terminal_parent_class)->screen_changed(widget, previous_screen);
  }
}
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
static gboolean vte_terminal_draw(GtkWidget *widget, cairo_t *cr) {
  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
  cairo_fill(cr);

  return TRUE;
}
#endif

static gboolean vte_terminal_key_press(GtkWidget *widget, GdkEventKey *event) {
  /* Check if GtkWidget's behavior already does something with this key. */
  GTK_WIDGET_CLASS(vte_terminal_parent_class)->key_press_event(widget, event);

  /* If FALSE is returned, tab operation is unexpectedly started in
   * gnome-terminal. */
  return TRUE;
}

static void vte_terminal_class_init(VteTerminalClass *vclass) {
  char *value;
  bl_conf_t *conf;
  char *argv[] = {"mlterm", NULL};
  GObjectClass *oclass;
  GtkWidgetClass *wclass;

#if defined(__DEBUG) && defined(USE_XLIB)
  XSetErrorHandler(error_handler);
  XSynchronize(gdk_x11_display_get_xdisplay(gdk_display_get_default()), True);
#endif

/* bl_sig_child_start() calls signal(3) internally. */
#if 0
  bl_sig_child_start();
#endif

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

#if 0
  bindtextdomain("vte", LOCALEDIR);
  bind_textdomain_codeset("vte", "UTF-8");
#endif

  if (!bl_locale_init("")) {
    bl_msg_printf("locale settings failed.\n");
  }

  bl_set_sys_conf_dir(CONFIG_PATH);

  vt_term_manager_init(1);
  vt_term_manager_enable_zombie_pty();
#ifdef USE_WIN32API
  /*
   * Call final() in vt_pty_win32.c to terminate child processes.
   * Without this, child processes survive even if all windows are closed by
   * pressing X button (GtkWindow (gtk_widget_get_toplevel(widget)) doesn't
   * receive "destroy" signal).
   */
  g_atexit(vt_term_manager_final);
#endif

#ifdef USE_WAYLAND
  gdk_threads_add_timeout(25, vte_terminal_timeout, NULL); /* 25 miliseconds */
#else
#if GTK_CHECK_VERSION(2, 12, 0)
  gdk_threads_add_timeout(100, vte_terminal_timeout, NULL); /* 100 miliseconds */
#else
  g_timeout_add(100, vte_terminal_timeout, NULL); /* 100 miliseconds */
#endif
#endif

  vt_color_config_init();

  ui_shortcut_init(&shortcut);
  ui_shortcut_parse(&shortcut, "Button3", "\"none\"");
  ui_shortcut_parse(&shortcut, "UNUSED", "OPEN_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "OPEN_PTY");
  ui_shortcut_parse(&shortcut, "UNUSED", "NEXT_PTY");
  ui_shortcut_parse(&shortcut, "UNUSED", "PREV_PTY");
  ui_shortcut_parse(&shortcut, "UNUSED", "HSPLIT_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "VSPLIT_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "NEXT_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "CLOSE_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "HEXPAND_SCREEN");
  ui_shortcut_parse(&shortcut, "UNUSED", "VEXPAND_SCREEN");

#ifdef USE_XLIB
  ui_xim_init(1);
#endif
  ui_font_use_point_size(1);

  bl_init_prog(g_get_prgname(), VERSION);

  if ((conf = bl_conf_new()) == NULL) {
    return;
  }

  ui_prepare_for_main_config(conf);

/*
 * Same processing as main_loop_init().
 * Following options are not possible to specify as arguments of mlclient.
 * 1) Options which are used only when mlterm starts up and which aren't
 *    changed dynamically. (e.g. "startup_screens")
 * 2) Options which change status of all ptys or windows. (Including ones
 *    which are possible to change dynamically.)
 *    (e.g. "font_size_range")
 */

#if 0
  bl_conf_add_opt(conf, 'R', "fsrange", 0, "font_size_range", NULL);
#endif
  bl_conf_add_opt(conf, 'Y', "decsp", 1, "compose_dec_special_font", NULL);
  bl_conf_add_opt(conf, 'c', "cp932", 1, "use_cp932_ucs_for_xft", NULL);
#if 0
  bl_conf_add_opt(conf, '\0', "maxptys", 0, "max_ptys", NULL);
#endif
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  /* USE_WAYLAND */
  bl_conf_add_opt(conf, '\0', "aafont", 1, "use_aafont",
                  "use [tv]aafont files with the use of fontconfig [true]");
#endif

  ui_main_config_init(&main_config, conf, 1, argv);

#if 0
  if ((value = bl_conf_get_value(conf, "font_size_range"))) {
    u_int min_font_size;
    u_int max_font_size;

    if (get_font_size_range(&min_font_size, &max_font_size, value)) {
      ui_set_font_size_range(min_font_size, max_font_size);
    }
  }
#endif

/* BACKWARD COMPAT (3.1.7 or before) */
#if 1
  {
    size_t count;
    /*
     * Compat with button3_behavior (shortcut_str[3]) is not applied
     * because button3 is disabled by
     * ui_shortcut_parse( &shortcut , "Button3" , "\"none\"") above.
     */
    char key0[] = "Control+Button1";
    char key1[] = "Control+Button2";
    char key2[] = "Control+Button3";
    char *keys[] = {
        key0, key1, key2,
    };

    for (count = 0; count < BL_ARRAY_SIZE(keys); count++) {
      if (main_config.shortcut_strs[count]) {
        ui_shortcut_parse(&shortcut, keys[count], main_config.shortcut_strs[count]);
      }
    }
  }
#endif

  if (main_config.type_engine == TYPE_XCORE) {
    /*
     * XXX Hack
     * Default value of type_engine is TYPE_XCORE in normal mlterm,
     * but default value in libvte compatible library of mlterm is TYPE_XFT.
     */
    char *value;

    if ((value = bl_conf_get_value(conf, "type_engine")) == NULL || strcmp(value, "xcore") != 0) {
/*
 * cairo is prefered if mlterm works as libvte because gtk+
 * usually depends on cairo.
 */
#if !defined(USE_TYPE_CAIRO) && defined(USE_TYPE_XFT)
      main_config.type_engine = TYPE_XFT;
#else
      main_config.type_engine = TYPE_CAIRO;
#endif
    }
  }

  /* Default value of vte "audible-bell" is TRUE, while "visible-bell" is FALSE.
   */
  main_config.bel_mode = BEL_SOUND;

#ifdef USE_WAYLAND
  /*
   * XXX
   * Without this hack, background of the bottom screen is not drawn on
   * sakura 3.7.1 or gnome-terminal 3.40.0 in starting initially.
   */
  main_config.cols = 1;
  main_config.rows = 1;
#endif

  if ((value = bl_conf_get_value(conf, "compose_dec_special_font"))) {
    if (strcmp(value, "true") == 0) {
      ui_compose_dec_special_font();
    }
  }

#ifdef USE_XLIB
  if ((value = bl_conf_get_value(conf, "use_cp932_ucs_for_xft")) == NULL ||
      strcmp(value, "true") == 0) {
    ui_use_cp932_ucs_for_xft();
  }
#endif

#ifdef KEY_REPEAT_BY_MYSELF
  if ((value = bl_conf_get_value(conf, "kbd_repeat_1"))) {
    extern int kbd_repeat_1;

    bl_str_to_int(&kbd_repeat_1, value);
  }

  if ((value = bl_conf_get_value(conf, "kbd_repeat_N"))) {
    extern int kbd_repeat_N;

    bl_str_to_int(&kbd_repeat_N, value);
  }
#endif
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  /* USE_WAYLAND */
  if (!(value = bl_conf_get_value(conf, "use_aafont")) || strcmp(value, "false") != 0) {
    ui_use_aafont();
  }
#endif

  bl_conf_destroy(conf);

#ifdef USE_BRLAPI
  ui_brltty_init();
#endif

  g_signal_connect(vte_reaper_get(), "child-exited", G_CALLBACK(catch_child_exited), NULL);

  g_type_class_add_private(vclass, sizeof(VteTerminalPrivate));

  memset(&disp, 0, sizeof(ui_display_t));
  init_display(&disp, vclass);

  ui_picture_display_opened(disp.display);

  oclass = G_OBJECT_CLASS(vclass);
  wclass = GTK_WIDGET_CLASS(vclass);

  oclass->finalize = vte_terminal_finalize;
  oclass->get_property = vte_terminal_get_property;
  oclass->set_property = vte_terminal_set_property;
  wclass->realize = vte_terminal_realize;
  wclass->unrealize = vte_terminal_unrealize;
  wclass->focus_in_event = vte_terminal_focus_in;
  wclass->focus_out_event = vte_terminal_focus_out;
  wclass->size_allocate = vte_terminal_size_allocate;
#if GTK_CHECK_VERSION(2, 90, 0)
  wclass->get_preferred_width = vte_terminal_get_preferred_width;
  wclass->get_preferred_height = vte_terminal_get_preferred_height;
  wclass->get_preferred_width_for_height = vte_terminal_get_preferred_width_for_height;
  wclass->get_preferred_height_for_width = vte_terminal_get_preferred_height_for_width;
#if VTE_CHECK_VERSION(0, 38, 0)
  wclass->screen_changed = vte_terminal_screen_changed;
#endif
#else
  wclass->size_request = vte_terminal_size_request;
#endif
  wclass->key_press_event = vte_terminal_key_press;
#if GTK_CHECK_VERSION(3, 0, 0)
  wclass->draw = vte_terminal_draw;
#endif

#if GTK_CHECK_VERSION(3, 19, 5)
  gtk_widget_class_set_css_name(wclass, VTE_TERMINAL_CSS_NAME);
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
  g_object_class_override_property(oclass, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property(oclass, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property(oclass, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property(oclass, PROP_VSCROLL_POLICY, "vscroll-policy");
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->eof_signal =
#else
  signals[SIGNAL_EOF] =
#endif
      g_signal_new(I_("eof"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, eof), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->child_exited_signal =
#else
  signals[SIGNAL_CHILD_EXITED] =
#endif
#if VTE_CHECK_VERSION(0, 38, 0)
      g_signal_new(I_("child-exited"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, child_exited), NULL, NULL,
                   g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
#else
      g_signal_new(I_("child-exited"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, child_exited), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->window_title_changed_signal =
#else
  signals[SIGNAL_WINDOW_TITLE_CHANGED] =
#endif
      g_signal_new(I_("window-title-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, window_title_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->icon_title_changed_signal =
#else
  signals[SIGNAL_ICON_TITLE_CHANGED] =
#endif
      g_signal_new(I_("icon-title-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, icon_title_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if VTE_CHECK_VERSION(0, 34, 0)
  signals[SIGNAL_CURRENT_FILE_URI_CHANGED] =
      g_signal_new(I_("current-file-uri-changed"), G_OBJECT_CLASS_TYPE(vclass),
                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[SIGNAL_CURRENT_DIRECTORY_URI_CHANGED] =
      g_signal_new(I_("current-directory-uri-changed"), G_OBJECT_CLASS_TYPE(vclass),
                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->encoding_changed_signal =
#else
  signals[SIGNAL_ENCODING_CHANGED] =
#endif
      g_signal_new(I_("encoding-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, encoding_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->commit_signal =
#else
  signals[SIGNAL_COMMIT] =
#endif
      g_signal_new(I_("commit"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, commit), NULL, NULL,
                   _vte_marshal_VOID__STRING_UINT, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);

#if !VTE_CHECK_VERSION(0, 38, 0)
#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->emulation_changed_signal =
#endif
      g_signal_new(I_("emulation-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, emulation_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->char_size_changed_signal =
#else
  signals[SIGNAL_CHAR_SIZE_CHANGED] =
#endif
      g_signal_new(I_("char-size-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, char_size_changed), NULL, NULL,
                   _vte_marshal_VOID__UINT_UINT, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->selection_changed_signal =
#else
  signals[SIGNAL_SELECTION_CHANGED] =
#endif
      g_signal_new(I_("selection-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, selection_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->contents_changed_signal =
#else
  signals[SIGNAL_CONTENTS_CHANGED] =
#endif
      g_signal_new(I_("contents-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, contents_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->cursor_moved_signal =
#else
  signals[SIGNAL_CURSOR_MOVED] =
#endif
      g_signal_new(I_("cursor-moved"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, cursor_moved), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->deiconify_window_signal =
#else
  signals[SIGNAL_DEICONIFY_WINDOW] =
#endif
      g_signal_new(I_("deiconify-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, deiconify_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->iconify_window_signal =
#else
  signals[SIGNAL_ICONIFY_WINDOW] =
#endif
      g_signal_new(I_("iconify-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, iconify_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->raise_window_signal =
#else
  signals[SIGNAL_RAISE_WINDOW] =
#endif
      g_signal_new(I_("raise-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, raise_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->lower_window_signal =
#else
  signals[SIGNAL_LOWER_WINDOW] =
#endif
      g_signal_new(I_("lower-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, lower_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->refresh_window_signal =
#else
  signals[SIGNAL_REFRESH_WINDOW] =
#endif
      g_signal_new(I_("refresh-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, refresh_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->restore_window_signal =
#else
  signals[SIGNAL_RESTORE_WINDOW] =
#endif
      g_signal_new(I_("restore-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, restore_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->maximize_window_signal =
#else
  signals[SIGNAL_MAXIMIZE_WINDOW] =
#endif
      g_signal_new(I_("maximize-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, maximize_window), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->resize_window_signal =
#else
  signals[SIGNAL_RESIZE_WINDOW] =
#endif
      g_signal_new(I_("resize-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, resize_window), NULL, NULL,
                   _vte_marshal_VOID__UINT_UINT, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->move_window_signal =
#else
  signals[SIGNAL_MOVE_WINDOW] =
#endif
      g_signal_new(I_("move-window"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, move_window), NULL, NULL,
                   _vte_marshal_VOID__UINT_UINT, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

#if !VTE_CHECK_VERSION(0, 38, 0)
#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->status_line_changed_signal =
#endif
      g_signal_new(I_("status-line-changed"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, status_line_changed), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->increase_font_size_signal =
#else
  signals[SIGNAL_INCREASE_FONT_SIZE] =
#endif
      g_signal_new(I_("increase-font-size"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, increase_font_size), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->decrease_font_size_signal =
#else
  signals[SIGNAL_DECREASE_FONT_SIZE] =
#endif
      g_signal_new(I_("decrease-font-size"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, decrease_font_size), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->text_modified_signal =
#else
  signals[SIGNAL_TEXT_MODIFIED] =
#endif
      g_signal_new(I_("text-modified"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, text_modified), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->text_inserted_signal =
#else
  signals[SIGNAL_TEXT_INSERTED] =
#endif
      g_signal_new(I_("text-inserted"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, text_inserted), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->text_deleted_signal =
#else
  signals[SIGNAL_TEXT_DELETED] =
#endif
      g_signal_new(I_("text-deleted"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, text_deleted), NULL, NULL,
                   g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION(2, 90, 0)
  vclass->text_scrolled_signal =
#else
  signals[SIGNAL_TEXT_SCROLLED] =
#endif
      g_signal_new(I_("text-scrolled"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(VteTerminalClass, text_scrolled), NULL, NULL,
                   g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

#if VTE_CHECK_VERSION(0, 19, 0)
  signals[SIGNAL_COPY_CLIPBOARD] = g_signal_new(I_("copy-clipboard"), G_OBJECT_CLASS_TYPE(vclass),
                                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                G_STRUCT_OFFSET(VteTerminalClass, copy_clipboard),
                                                NULL, NULL, g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);

  signals[SIGNAL_PASTE_CLIPBOARD] = g_signal_new(I_("paste-clipboard"), G_OBJECT_CLASS_TYPE(vclass),
                                                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                 G_STRUCT_OFFSET(VteTerminalClass, paste_clipboard),
                                                 NULL, NULL, g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);
#endif
#if VTE_CHECK_VERSION(0, 44, 0)
  signals[SIGNAL_BELL] = g_signal_new(I_("bell"), G_OBJECT_CLASS_TYPE(vclass), G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET(VteTerminalClass, bell), NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif

#if VTE_CHECK_VERSION(0, 20, 0)
  g_object_class_install_property(
      oclass, PROP_WINDOW_TITLE,
      g_param_spec_string("window-title", NULL, NULL, NULL, G_PARAM_READABLE | STATIC_PARAMS));

  g_object_class_install_property(
      oclass, PROP_ICON_TITLE,
      g_param_spec_string("icon-title", NULL, NULL, NULL, G_PARAM_READABLE | STATIC_PARAMS));
#endif

#if VTE_CHECK_VERSION(0, 23, 2)
/*
 * doc/references/html/VteTerminal.html describes that inner-border property
 * is since 0.24.0, but actually it is added at Nov 30 2009 (between 0.23.1 and
 * 0.23.2)
 * in ChangeLog.
 */

#if !VTE_CHECK_VERSION(0, 38, 0)
  gtk_widget_class_install_style_property(
      wclass, g_param_spec_boxed("inner-border", NULL, NULL, GTK_TYPE_BORDER,
                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
  vclass->priv = G_TYPE_CLASS_GET_PRIVATE(vclass, VTE_TYPE_TERMINAL, VteTerminalClassPrivate);
  vclass->priv->style_provider = GTK_STYLE_PROVIDER(gtk_css_provider_new());
#if !VTE_CHECK_VERSION(0, 38, 0)
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(vclass->priv->style_provider) ,
                                  "VteTerminal {\n"
                                  "-VteTerminal-inner-border: " BL_INT_TO_STR(WINDOW_MARGIN) ";\n"
                                  "}\n",
                                  -1, NULL) ;
#else
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(vclass->priv->style_provider) ,
                                  "VteTerminal, " VTE_TERMINAL_CSS_NAME " {\n"
                                  "padding: 1px 1px 1px 1px;\n"
                                  "background-color: @theme_base_color;\n"
                                  "color: @theme_fg_color;\n"
                                  "}\n",
                                  -1 , NULL) ;
#endif
#else /* VTE_CHECK_VERSION(0,23,2) */
  gtk_rc_parse_string("style \"vte-default-style\" {\n"
                      "VteTerminal::inner-border = { "
                      BL_INT_TO_STR(WINDOW_MARGIN) " , "
                      BL_INT_TO_STR(WINDOW_MARGIN) " , "
                      BL_INT_TO_STR(WINDOW_MARGIN) " , "
                      BL_INT_TO_STR(WINDOW_MARGIN) " }\n"
                      "}\n"
                      "class \"VteTerminal\" style : gtk \"vte-default-style\"\n") ;
#endif
#endif /* VTE_CHECK_VERSION(0,23,2) */
}

static void vte_terminal_init(VteTerminal *terminal) {
  static int init_inherit_ptys;
  ef_charset_t usascii_font_cs;
  gdouble dpi;

  GTK_WIDGET_SET_CAN_FOCUS(GTK_WIDGET(terminal));
#if VTE_CHECK_VERSION(0, 38, 0)
  gtk_widget_set_app_paintable(GTK_WIDGET(terminal), TRUE);
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(terminal), FALSE);
#endif

#if VTE_CHECK_VERSION(0, 40, 0)
  terminal->_unused_padding[0] = (void**)G_TYPE_INSTANCE_GET_PRIVATE(terminal, VTE_TYPE_TERMINAL, VteTerminalPrivate);
#else
  terminal->pvt = G_TYPE_INSTANCE_GET_PRIVATE(terminal, VTE_TYPE_TERMINAL, VteTerminalPrivate);
#endif

#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_widget_set_has_window(GTK_WIDGET(terminal), TRUE);
#endif

  /* We do our own redrawing. */
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(terminal), FALSE);

  ADJUSTMENT(terminal) = NULL;
  set_adjustment(terminal, GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, main_config.rows, 1,
                                                             main_config.rows, main_config.rows)));

#ifdef USE_XLIB
  g_signal_connect(terminal, "hierarchy-changed", G_CALLBACK(vte_terminal_hierarchy_changed), NULL);
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
  gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(terminal)),
                                 VTE_TERMINAL_GET_CLASS(terminal)->priv->style_provider,
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif

  PVT(terminal)->term = vt_create_term(main_config.term_type, main_config.cols, main_config.rows,
                                       main_config.tab_size, main_config.num_log_lines,
                                       main_config.encoding, main_config.is_auto_encoding,
                                       main_config.use_auto_detect, main_config.logging_vt_seq,
                                       main_config.unicode_policy, main_config.col_size_of_width_a,
                                       main_config.use_char_combining,
                                       main_config.use_multi_col_char, main_config.use_ctl,
                                       main_config.bidi_mode, main_config.bidi_separators,
                                       main_config.use_dynamic_comb, main_config.bs_mode,
                                       /* main_config.vertical_mode */ 0,
                                       main_config.use_local_echo, main_config.title,
                                       main_config.icon_name, main_config.use_ansi_colors,
                                       main_config.alt_color_mode, main_config.use_ot_layout,
                                       main_config.blink_cursor ? CS_BLINK|CS_BLOCK : CS_BLOCK,
                                       main_config.ignore_broadcasted_chars,
                                       main_config.use_locked_title);
  if (!init_inherit_ptys) {
    u_int num;
    vt_term_t **terms;
    u_int count;

    num = vt_get_all_terms(&terms);
    for (count = 0; count < num; count++) {
      if (terms[count] != PVT(terminal)->term) {
        /* GPid in win32 is void* (HANDLE). (see glib/gmain.h) */
        vte_reaper_add_child((GPid)vt_term_get_child_pid(terms[count]));
      }
    }

    init_inherit_ptys = 1;
  }

  if (main_config.unlimit_log_size) {
    vt_term_unlimit_log_size(PVT(terminal)->term);
  }

#if VTE_CHECK_VERSION(0, 26, 0)
  PVT(terminal)->pty = NULL;
#endif

  if (main_config.unicode_policy & NOT_USE_UNICODE_FONT || main_config.iso88591_font_for_usascii) {
    usascii_font_cs = ui_get_usascii_font_cs(VT_ISO8859_1);
  } else if (main_config.unicode_policy & ONLY_USE_UNICODE_FONT) {
    usascii_font_cs = ui_get_usascii_font_cs(VT_UTF8);
  } else {
    usascii_font_cs = ui_get_usascii_font_cs(vt_term_get_encoding(PVT(terminal)->term));
  }

  /* related to ui_font_use_point_size(1) in vte_terminal_class_init. */
  if ((dpi = gdk_screen_get_resolution(gtk_widget_get_screen(GTK_WIDGET(terminal)))) != -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Setting dpi %f\n", dpi);
#endif

    ui_font_set_dpi_for_fc(dpi);
  }

  init_screen(terminal, ui_font_manager_new(
                            disp.display, main_config.type_engine, main_config.font_present,
                            main_config.font_size, usascii_font_cs,
                            main_config.step_in_changing_font_size, main_config.letter_space,
                            main_config.use_bold_font, main_config.use_italic_font),
              ui_color_manager_new(&disp, main_config.fg_color, main_config.bg_color,
                                   main_config.cursor_fg_color, main_config.cursor_bg_color,
                                   main_config.bd_color, main_config.ul_color,
                                   main_config.bl_color, main_config.rv_color,
                                   main_config.it_color, main_config.co_color));

  PVT(terminal)->io = NULL;
  PVT(terminal)->src_id = 0;

  PVT(terminal)->image = NULL;
  PVT(terminal)->pixmap = None;
  PVT(terminal)->pix_width = 0;
  PVT(terminal)->pix_height = 0;
  PVT(terminal)->pic_mod = NULL;

#if GLIB_CHECK_VERSION(2, 14, 0)
  PVT(terminal)->gregex = NULL;
  PVT(terminal)->match_gregexes = NULL;
  PVT(terminal)->num_match_gregexes = 0;
#if VTE_CHECK_VERSION(0, 46, 0)
  PVT(terminal)->vregex = NULL;
  PVT(terminal)->match_vregexes = NULL;
  PVT(terminal)->num_match_vregexes = 0;
#endif
#endif

  WINDOW_TITLE(terminal) = vt_term_window_name(PVT(terminal)->term);
  ICON_TITLE(terminal) = vt_term_icon_name(PVT(terminal)->term);

#if !GTK_CHECK_VERSION(2, 90, 0)
  /* XXX */
  if (strstr(g_get_prgname(), "roxterm") ||
      /*
       * Hack for roxterm started by "x-terminal-emulator" or
       * "exo-open --launch TerminalEmulator" (which calls
       * "x-terminal-emulator" internally)
       */
      g_object_get_data(gtk_widget_get_parent(GTK_WIDGET(terminal)), "roxterm_tab")) {
    /*
     * XXX
     * I don't know why, but gtk_widget_ensure_style() doesn't apply
     * "inner-border"
     * and min width/height of roxterm are not correctly set.
     */
    gtk_widget_set_rc_style(&terminal->widget);
  } else
#endif
  {
    /*
     * gnome-terminal(2.32.1) fails to set "inner-border" and
     * min width/height without this.
     */
    gtk_widget_ensure_style(&terminal->widget);
  }

  reset_vte_size_member(terminal);
}

static int vt_term_open_pty_wrap(VteTerminal *terminal, const char *cmd_path, char **argv,
                                 char **envv, const char *work_dir, const char *pass,
                                 const char *pubkey, const char *privkey) {
  const char *host;
  char **env_p;
  u_int num;

  host = gdk_display_get_name(gtk_widget_get_display(GTK_WIDGET(terminal)));

  if (argv) {
    char **argv_p;

    num = 0;
    argv_p = argv;
    while (*(argv_p++)) {
      num++;
    }

    if (num > 0 && !strstr(cmd_path, argv[0]) && (argv_p = alloca(sizeof(char *) * (num + 2)))) {
      memcpy(argv_p + 1, argv, sizeof(char *) * (num + 1));
      argv_p[0] = (char*)cmd_path; /* avoid warning "discard qualifiers" */
      argv = argv_p;

#if 0
      for (argv_p = argv; *argv_p; argv_p++) {
        bl_debug_printf("%s\n", *argv_p);
      }
#endif
    }
  }

  num = 0;
  if (envv) {
    env_p = envv;
    while (*(env_p++)) {
      num++;
    }
  }

  /* 7 => MLTERM,TERM,WINDOWID,WAYLAND_DISPLAY,DISPLAY,COLORFGBG,NULL */
  if ((env_p = alloca(sizeof(char *) * (num + 7)))) {
    if (num > 0) {
      envv = memcpy(env_p, envv, sizeof(char *) * num);
      env_p += num;
    } else {
      envv = env_p;
    }

    *(env_p++) = "MLTERM=" VERSION;

#ifdef USE_XLIB
    /* "WINDOWID="(9) + [32bit digit] + NULL(1) */
    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)) &&
        (*env_p = alloca(9 + DIGIT_STR_LEN(Window) + 1))) {
      sprintf(*(env_p++), "WINDOWID=%ld",
#if 1
              gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)))
#else
              PVT(terminal)->screen->window.my_window
#endif
                  );
    }
#endif

#ifdef USE_WAYLAND
    /* "WAYLAND_DISPLAY="(16) + NULL(1) */
    if (host && (*env_p = alloca(16 + strlen(host) + 1))) {
      sprintf(*(env_p++), "WAYLAND_DISPLAY=%s", host);
    }
    *(env_p++) = "DISPLAY=:0.0";
#else
    /* "DISPLAY="(8) + NULL(1) */
    if ((*env_p = alloca(8 + strlen(host) + 1))) {
      sprintf(*(env_p++), "DISPLAY=%s", host);
    }
#endif

    /* "TERM="(5) + NULL(1) */
    if ((*env_p = alloca(5 + strlen(main_config.term_type) + 1))) {
      sprintf(*(env_p++), "TERM=%s", main_config.term_type);
    }

    *(env_p++) = "COLORFGBG=default;default";

    /* NULL terminator */
    *env_p = NULL;
  }

#if 0
  env_p = envv;
  while (*env_p) {
    bl_debug_printf("%s\n", *(env_p++));
  }
#endif

  if (vt_term_open_pty(PVT(terminal)->term, cmd_path, argv, envv, host, work_dir, pass, pubkey,
                       privkey, PVT(terminal)->screen->window.width,
                       PVT(terminal)->screen->window.height)) {
    return 1;
  } else {
    return 0;
  }
}

static void set_alpha(VteTerminal *terminal, u_int8_t alpha) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " ALPHA => %d\n", alpha);
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    char value[DIGIT_STR_LEN(u_int8_t) + 1];

    sprintf(value, "%d", (int)alpha);

    ui_screen_set_config(PVT(terminal)->screen, NULL, "alpha", value);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
    update_wall_picture(terminal);
  } else {
    PVT(terminal)->screen->pic_mod.alpha = alpha;
    ui_change_true_transbg_alpha(PVT(terminal)->screen->color_man, alpha);
  }
}

static void set_color_bold(VteTerminal *terminal, const void *bold,
                           gchar *(*to_string)(const void *)) {
  gchar *str;

  if (!bold) {
    str = strdup("");
  } else {
    /* #rrrrggggbbbb */
    str = (*to_string)(bold);
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_color_bold %s\n", str);
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "bd_color", str);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
  } else {
    if (ui_color_manager_set_alt_color(PVT(terminal)->screen->color_man, VT_BOLD_COLOR,
                                       *str ? str : NULL)) {
      vt_term_set_alt_color_mode(PVT(terminal)->term,
                                 *str ? (vt_term_get_alt_color_mode(PVT(terminal)->term) | 1)
                                      : (vt_term_get_alt_color_mode(PVT(terminal)->term) & ~1));
    }
  }

  g_free(str);
}

static void set_color_foreground(VteTerminal *terminal, const void *foreground,
                                 gchar *(*to_string)(const void *)) {
  gchar *str;

  if (!foreground) {
    return;
  }

  /* #rrrrggggbbbb */
  str = (*to_string)(foreground);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_color_foreground %s\n", str);
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "fg_color", str);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
  } else {
    ui_color_manager_set_fg_color(PVT(terminal)->screen->color_man, str);
  }

  g_free(str);
}

static void set_color_background(VteTerminal *terminal, const void *background,
                                 gchar *(*to_string)(const void *)) {
  gchar *str;

  if (!background) {
    return;
  }

  /* #rrrrggggbbbb */
  str = (*to_string)(background);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_color_background %s\n", str);
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "bg_color", str);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);

    if (PVT(terminal)->image && PVT(terminal)->screen->pic_mod.alpha < 255) {
      update_wall_picture(terminal);
    }
  } else {
    ui_color_manager_set_bg_color(PVT(terminal)->screen->color_man, str);
  }

  g_free(str);
}

static void set_color_cursor(VteTerminal *terminal, const void *cursor_background,
                             gchar *(*to_string)(const void *)) {
  gchar *str;

  if (!cursor_background) {
    return;
  }

  /* #rrrrggggbbbb */
  str = (*to_string)(cursor_background);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_color_cursor %s\n", str);
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "cursor_bg_color", str);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
  } else {
    ui_color_manager_set_cursor_bg_color(PVT(terminal)->screen->color_man, str);
  }

  g_free(str);
}

static int set_colors(VteTerminal *terminal, const void *palette, glong palette_size,
                      size_t color_size, gchar *(*to_string)(const void *)) {
  int need_redraw = 0;
  vt_color_t color;

  if (palette_size != 0 && palette_size != 8 && palette_size != 16 &&
      (palette_size < 24 || 256 < palette_size)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " palette_size %d is illegal\n", palette_size);
#endif

    return 0;
  }

  if (palette_size >= 8) {
    for (color = 0; color < palette_size; color++) {
      gchar *rgb;
      char *name;

      rgb = (*to_string)(palette);
      name = vt_get_color_name(color);

#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Setting rgb %s=%s\n", name, rgb);
#endif

      need_redraw |= vt_customize_color_file(name, rgb, 0);

      g_free(rgb);
      palette += color_size;
    }
  } else {
    for (color = 0; color < 256; color++) {
      char *name = vt_get_color_name(color);

#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Erase rgb of %s\n", name);
#endif

      need_redraw |= vt_customize_color_file(name, "", 0);
    }
  }

  if (need_redraw && GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_color_cache_unload_all();
    ui_screen_reset_view(PVT(terminal)->screen);
  }

  return 1;
}

/* --- global functions --- */

GtkWidget *vte_terminal_new() { return g_object_new(VTE_TYPE_TERMINAL, NULL); }

/*
 * vte_terminal_spawn_sync, vte_terminal_fork_command or vte_terminal_forkpty
 * functions
 * are possible to call before VteTerminal widget is realized.
 */

pid_t vte_terminal_fork_command(VteTerminal *terminal,
                                const char *command, /* If NULL, open default shell. */
                                char **argv,         /* If NULL, open default shell. */
                                char **envv, const char *directory, gboolean lastlog, gboolean utmp,
                                gboolean wtmp) {
  /*
   * If pty is inherited from dead parent, PVT(terminal)->term->pty is non-NULL
   * but create_io() and vte_reaper_add_child() aren't executed.
   * So PVT(terminal)->io is used to check if pty is completely set up.
   */
  if (!PVT(terminal)->io) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " forking with %s\n", command);
#endif

    if (!command) {
      if (main_config.cmd_path) {
        command = main_config.cmd_path;
        argv = main_config.cmd_argv;
      } else if (!(command = getenv("SHELL")) || *command == '\0') {
#ifdef USE_WIN32API
        command = "c:\\Windows\\System32\\cmd.exe";
#else
        struct passwd *pw;

        if ((pw = getpwuid(getuid())) == NULL || *(command = pw->pw_shell) == '\0') {
          command = "/bin/sh";
        }
#endif
      }
    }

    if (!argv || !argv[0]) {
      argv = alloca(sizeof(char *) * 2);
      argv[0] = (char*)command; /* avoid warning "discard qualifiers" */
      argv[1] = NULL;
    }

    bl_pty_helper_set_flag(lastlog, utmp, wtmp);

    if (!vt_term_open_pty_wrap(terminal, command, argv, envv, directory, NULL, NULL, NULL)) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " fork failed\n");
#endif

      return -1;
    }

    create_io(terminal);

    /*
     * These two routes for watching ptys exist and 1) is preferred in win32.
     * 1) wait_child_exited -> sig_child -> vt_close_dead_terms
     * 2) VteReaper -> child-exited event -> vt_close_dead_terms
     */
#ifndef USE_WIN32API
    vte_reaper_add_child(vt_term_get_child_pid(PVT(terminal)->term));
#endif

    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
      GtkAllocation allocation;

      gtk_widget_get_allocation(GTK_WIDGET(terminal), &allocation);

      if (!is_initial_allocation(&allocation) &&
          ui_window_resize_with_margin(&PVT(terminal)->screen->window, allocation.width,
                                       allocation.height, NOTIFY_TO_MYSELF)) {
        reset_vte_size_member(terminal);
        update_wall_picture(terminal);
      }
    }

    /*
     * In order to receive pty_closed() event even if vte_terminal_realize()
     * isn't called.
     */
    vt_pty_set_listener(PVT(terminal)->term->pty, &PVT(terminal)->screen->pty_listener);
  }

  return vt_term_get_child_pid(PVT(terminal)->term);
}

#if VTE_CHECK_VERSION(0, 26, 0)
gboolean
#if VTE_CHECK_VERSION(0, 38, 0)
vte_terminal_spawn_sync(VteTerminal *terminal, VtePtyFlags pty_flags, const char *working_directory,
                        char **argv, char **envv, GSpawnFlags spawn_flags,
                        GSpawnChildSetupFunc child_setup, gpointer child_setup_data,
                        GPid *child_pid /* out */, GCancellable *cancellable, GError **error)
#else
vte_terminal_fork_command_full(VteTerminal *terminal, VtePtyFlags pty_flags,
                               const char *working_directory, char **argv, char **envv,
                               GSpawnFlags spawn_flags, GSpawnChildSetupFunc child_setup,
                               gpointer child_setup_data, GPid *child_pid /* out */, GError **error)
#endif
{
  GPid pid;

  /* GPid in win32 is void* (HANDLE). (see glib/gmain.h) */
  pid = (GPid)vte_terminal_fork_command(terminal, argv[0], argv[0] == NULL ? NULL : argv + 1,
                                        envv, working_directory,
                                        (pty_flags & VTE_PTY_NO_LASTLOG) ? FALSE : TRUE,
                                        (pty_flags & VTE_PTY_NO_UTMP) ? FALSE : TRUE,
                                        (pty_flags & VTE_PTY_NO_WTMP) ? FALSE : TRUE);
  if (child_pid) {
    *child_pid = pid;
  }

  if (error) {
    *error = NULL;
  }

  if (pid > 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}
#endif

#if VTE_CHECK_VERSION(0, 48, 0)
void vte_terminal_spawn_async(VteTerminal *terminal, VtePtyFlags pty_flags,
                              const char *working_directory,
                              char **argv, char **envv, GSpawnFlags spawn_flags,
                              GSpawnChildSetupFunc child_setup, gpointer child_setup_data,
                              GDestroyNotify child_setup_data_destroy,
                              int timeout, GCancellable *cancellable,
                              VteTerminalSpawnAsyncCallback callback, gpointer user_data) {
  GPid child_pid;
  GError *error;

  vte_terminal_spawn_sync(terminal, pty_flags, working_directory, (char**)argv, (char**)envv,
                          spawn_flags, child_setup, child_setup_data, &child_pid, cancellable,
                          &error);
  if (callback) {
    (*callback)(terminal, child_pid, NULL, user_data);
  }
}
#endif

#if VTE_CHECK_VERSION(0, 62, 0)
void vte_terminal_spawn_with_fds_async(VteTerminal *terminal, VtePtyFlags pty_flags,
                                       const char *working_directory,
                                       char const* const* argv, char const* const* envv,
                                       int const *fds, int n_fds,
                                       int const *fd_map_to, int n_fd_map_to,
                                       GSpawnFlags spawn_flags,
                                       GSpawnChildSetupFunc child_setup, gpointer child_setup_data,
                                       GDestroyNotify child_setup_data_destroy,
                                       int timeout, GCancellable *cancellable,
                                       VteTerminalSpawnAsyncCallback callback, gpointer user_data) {
  GPid child_pid;
  GError *error;

  /* XXX fds and fd_map_to */

  if (n_fds > 0 || n_fd_map_to > 0) {
    bl_msg_printf("vte_terminal_spawn_with_fds_async() ignores fds and fd_map_to.\n");
  }

  vte_terminal_spawn_sync(terminal, pty_flags, working_directory, (char**)argv, (char**)envv,
                          spawn_flags, child_setup, child_setup_data, &child_pid, cancellable,
                          &error);
  if (callback) {
    (*callback)(terminal, child_pid, NULL, user_data);
  }
}
#endif

pid_t vte_terminal_forkpty(VteTerminal *terminal, char **envv, const char *directory,
                           gboolean lastlog, gboolean utmp, gboolean wtmp) {
  /*
   * If pty is inherited from dead parent, PVT(terminal)->term->pty is non-NULL
   * but create_io() and vte_reaper_add_child() aren't executed.
   * So PVT(terminal)->io is used to check if pty is completely set up.
   */
  if (!PVT(terminal)->io) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " forking pty\n");
#endif

    bl_pty_helper_set_flag(lastlog, utmp, wtmp);

    if (!vt_term_open_pty_wrap(terminal, NULL, NULL, envv, directory, NULL, NULL, NULL)) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " fork failed\n");
#endif

      return -1;
    }

    if (vt_term_get_child_pid(PVT(terminal)->term) == 0) {
      /* Child process */

      return 0;
    }

    create_io(terminal);

    /*
     * These two routes for watching ptys exist and 1) is preferred in win32.
     * 1) wait_child_exited -> sig_child -> vt_close_dead_terms
     * 2) VteReaper -> child-exited event -> vt_close_dead_terms
     */
#ifndef USE_WIN32API
    vte_reaper_add_child(vt_term_get_child_pid(PVT(terminal)->term));
#endif

    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
      GtkAllocation allocation;

      gtk_widget_get_allocation(GTK_WIDGET(terminal), &allocation);

      if (!is_initial_allocation(&allocation) &&
          ui_window_resize_with_margin(&PVT(terminal)->screen->window, allocation.width,
                                       allocation.height, NOTIFY_TO_MYSELF)) {
        reset_vte_size_member(terminal);
        update_wall_picture(terminal);
      }
    }

    /*
     * In order to receive pty_closed() event even if vte_terminal_realize()
     * isn't called.
     */
    vt_pty_set_listener(PVT(terminal)->term->pty, &PVT(terminal)->screen->pty_listener);
  }

  return vt_term_get_child_pid(PVT(terminal)->term);
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_feed(VteTerminal *terminal, const char *data, gssize length)
#else
void vte_terminal_feed(VteTerminal *terminal, const char *data, glong length)
#endif
{
  vt_term_write_loopback(PVT(terminal)->term, data, length == -1 ? strlen(data) : length);
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_feed_child(VteTerminal *terminal, const char *text, gssize length)
#else
void vte_terminal_feed_child(VteTerminal *terminal, const char *text, glong length)
#endif
{
  vt_term_write(PVT(terminal)->term, text, length == -1 ? strlen(text) : length);
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_feed_child_binary(VteTerminal *terminal, const guint8 *data, gsize length)
#else
void vte_terminal_feed_child_binary(VteTerminal *terminal, const char *data, glong length)
#endif
{
  vt_term_write(PVT(terminal)->term, data, length);
}

static char *convert_vtstr_to_utf8(vt_char_t *str, u_int len, int tohtml, size_t *len_ret) {
  ef_conv_t *conv;
  ef_parser_t *parser;
  u_char *buf;

  if (!(conv = vt_char_encoding_conv_new(VT_UTF8))) {
    return NULL;
  }

  if (!(parser = vt_str_parser_new())) {
    (*conv->destroy)(conv);
    return NULL;
  }

  (*parser->init)(parser);
  vt_str_parser_set_str(parser, str, len);

  len = len * VTCHAR_UTF_MAX_SIZE + 1 + (tohtml ? 11 : 0);
  if ((buf = g_malloc(len))) {
    (*conv->init)(conv);
    *(buf + (len = (*conv->convert)(conv, buf, len, parser))) = '\0';

    if (tohtml) {
      /* XXX */
      memmove(buf + 5, buf, len);
      memcpy(buf, "<pre>", 5);
      strcpy(buf + 5 + len, "</pre>");
      len += 11;
    }

    if (len_ret) {
      *len_ret = len;
    }
  }

  (*conv->destroy)(conv);
  (*parser->destroy)(parser);

  return buf;
}

static void copy_clipboard(VteTerminal *terminal, int tohtml) {
  GtkClipboard *clipboard;
  u_char *buf;
  size_t len;

  if (!vte_terminal_get_has_selection(terminal)) {
    return;
  }

  /*
   * Don't use alloca() here because len can be too big value.
   * (VTCHAR_UTF_MAX_SIZE defined in vt_char.h is 48 byte.)
   */
  if ((buf = convert_vtstr_to_utf8(PVT(terminal)->screen->sel.sel_str,
                                   PVT(terminal)->screen->sel.sel_len, tohtml, &len))) {
    if (len > 0 && (clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD))) {
#ifdef USE_WAYLAND
      extern gint64 deadline_ignoring_source_cancelled_event;
      deadline_ignoring_source_cancelled_event = g_get_monotonic_time() + 1000000; /* microsecond */
#endif

      gtk_clipboard_set_text(clipboard, buf, len);
      gtk_clipboard_store(clipboard);
#if GTK_CHECK_VERSION(2, 6, 0)
      /*
       * XXX gnome-terminal 3.24.2 doesn't make "EditPaste" menuitem sensitive after
       * clicking "EditCopy" menu item without invoking "owner-change" event explicitly.
       */
      g_signal_emit_by_name(clipboard, "owner-change");
#endif
    }

    free(buf);
  }
}

void vte_terminal_copy_clipboard(VteTerminal *terminal) {
  copy_clipboard(terminal, 0);
}

#if VTE_CHECK_VERSION(0, 50, 0)
void vte_terminal_copy_clipboard_format(VteTerminal *terminal, VteFormat format) {
  copy_clipboard(terminal, format == VTE_FORMAT_HTML);
}
#endif

void vte_terminal_paste_clipboard(VteTerminal *terminal) {
  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    char cmd[] = "paste clipboard";
    ui_screen_exec_cmd(PVT(terminal)->screen, cmd);
  }
}

#if VTE_CHECK_VERSION(0, 68, 0)
void vte_terminal_paste_text(VteTerminal *terminal, const char *text) {
  vte_terminal_feed_child(terminal, text, strlen(text));
}
#endif

void vte_terminal_copy_primary(VteTerminal *terminal) {}

void vte_terminal_paste_primary(VteTerminal *terminal) {
  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_exec_cmd(PVT(terminal)->screen, "paste");
  }
}

void vte_terminal_select_all(VteTerminal *terminal) {
  int beg_row;
  int end_row;
  vt_line_t *line;

  if (!GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    return;
  }

  beg_row = -vt_term_get_num_logged_lines(PVT(terminal)->term);

  for (end_row = vt_term_get_rows(PVT(terminal)->term) - 1; end_row >= 0; end_row--) {
    line = vt_term_get_line(PVT(terminal)->term, end_row); /* Always non-NULL */
    if (!vt_line_is_empty(line)) {
      break;
    }
  }

  selection(&PVT(terminal)->screen->sel, 0, beg_row, line->num_filled_chars - 1, end_row);

  ui_window_update(&PVT(terminal)->screen->window, 1 /* UPDATE_SCREEN */);
}

#if VTE_CHECK_VERSION(0, 52, 0)
void vte_terminal_unselect_all(VteTerminal *terminal) {
  /* see restore_selected_region_color_instantly() in ui_screen.c */
  if (ui_restore_selected_region_color(&PVT(terminal)->screen->sel)) {
    ui_window_update(&PVT(terminal)->screen->window, 0x3/*UPDATE_SCREEN|UPDATE_CURSOR*/);
  }
}
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_select_none(VteTerminal *terminal)
#else
void vte_terminal_unselect_all(VteTerminal *terminal)
#endif
{
  if (!GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    return;
  }

  ui_sel_clear(&PVT(terminal)->screen->sel);

  ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
}

void vte_terminal_set_size(VteTerminal *terminal, glong columns, glong rows) {
  ui_screen_t *screen = PVT(terminal)->screen;
  vt_term_t *term = PVT(terminal)->term;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set cols %d rows %d\n", columns, rows);
#endif

  vt_term_resize(term, columns, rows,
                 /*
                  * Vertical writing mode and screen_(width|height)_ratio option
                  * aren't supported.
                  * See reset_vte_size_member().
                  */
                 CHAR_WIDTH(terminal) * columns, CHAR_HEIGHT(terminal) * rows);
  reset_vte_size_member(terminal);

  /* gnome-terminal(2.29.6 or later ?) is not resized correctly without this. */
  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
#ifdef USE_WAYLAND
    /*
     * Screen may be redrawn before gtk_widget_queue_resize_no_redraw() results in resizing.
     * This causes illegal memory access without this ui_window_resize_with_margin()
     * on wayland.
     */
    gint width;
    gint height;
    int orig_init_char_size;

    vte_terminal_get_preferred_width(GTK_WIDGET(terminal), NULL, &width);

    /*
     * vte_terminal_get_preferred_height() invokes 'char-size-changed' event
     * on roxterm if init_char_size is 0.
     * (See vte_terminal_get_preferred_height()).
     */
    orig_init_char_size = PVT(terminal)->init_char_size;
    PVT(terminal)->init_char_size = 1;
    vte_terminal_get_preferred_height(GTK_WIDGET(terminal), NULL, &height);
    PVT(terminal)->init_char_size = orig_init_char_size;

    ui_window_resize_with_margin(&screen->window, width, height, 0);
#endif
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(terminal));
  } else {
    /*
     * XXX
     * The same processing as window_resized() in ui_screen.c.
     * It is assumed that vertical_mode and screen_width_ratio are ignored
     * in libvte compatible library.
     */
    screen->width = screen->window.width = CHAR_WIDTH(terminal) * columns;
    screen->height = screen->window.height = CHAR_HEIGHT(terminal) * rows;
  }
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_font_scale(VteTerminal *terminal, gdouble scale) {}

gdouble vte_terminal_get_font_scale(VteTerminal *terminal) { return 14; }
#endif

void vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean is_audible) {
  ui_screen_set_config(
      PVT(terminal)->screen, NULL, "bel_mode",
      ui_get_bel_mode_name(is_audible ? (BEL_SOUND | PVT(terminal)->screen->bel_mode)
                                      : (~BEL_SOUND & PVT(terminal)->screen->bel_mode)));
}

gboolean vte_terminal_get_audible_bell(VteTerminal *terminal) {
  if (PVT(terminal)->screen->bel_mode & BEL_SOUND) {
    return TRUE;
  } else {
    return FALSE;
  }
}

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_visible_bell(VteTerminal *terminal, gboolean is_visible) {
  ui_screen_set_config(
      PVT(terminal)->screen, NULL, "bel_mode",
      ui_get_bel_mode_name(is_visible ? (BEL_VISUAL | PVT(terminal)->screen->bel_mode)
                                      : (~BEL_VISUAL & PVT(terminal)->screen->bel_mode)));
}

gboolean vte_terminal_get_visible_bell(VteTerminal *terminal) {
  if (PVT(terminal)->screen->bel_mode & BEL_VISUAL) {
    return TRUE;
  } else {
    return FALSE;
  }
}
#endif

void vte_terminal_set_scroll_background(VteTerminal *terminal, gboolean scroll) {}

void vte_terminal_set_scroll_on_output(VteTerminal *terminal, gboolean scroll) {
  ui_exit_backscroll_by_pty(scroll);
}

#if VTE_CHECK_VERSION(0, 52, 0)
gboolean vte_terminal_get_scroll_on_output(VteTerminal *terminal) {
  if (vt_term_is_backscrolling(PVT(terminal)->term) == BSM_STATIC) {
    return FALSE;
  } else {
    return TRUE;
  }
}
#endif

#if VTE_CHECK_VERSION(0, 76, 0)
void vte_terminal_set_scroll_on_insert(VteTerminal *terminal, gboolean scroll) {
}

gboolean vte_terminal_get_scroll_on_insert(VteTerminal *terminal) {
  return FALSE;
}
#endif

void vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal, gboolean scroll) {}

#if VTE_CHECK_VERSION(0, 36, 0)
void vte_terminal_set_rewrap_on_resize(VteTerminal *terminal, gboolean rewrap) {}

gboolean vte_terminal_get_rewrap_on_resize(VteTerminal *terminal) { return TRUE; }
#endif

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_color_dim(VteTerminal *terminal, const GdkColor *dim) {}

void vte_terminal_set_color_bold(VteTerminal *terminal, const GdkColor *bold) {
  set_color_bold(terminal, bold, gdk_color_to_string);
}

void vte_terminal_set_color_foreground(VteTerminal *terminal, const GdkColor *foreground) {
  set_color_foreground(terminal, foreground, gdk_color_to_string);
}

void vte_terminal_set_color_background(VteTerminal *terminal, const GdkColor *background) {
  set_color_background(terminal, background, gdk_color_to_string);
}

void vte_terminal_set_color_cursor(VteTerminal *terminal, const GdkColor *cursor_background) {
  set_color_cursor(terminal, cursor_background, gdk_color_to_string);
}

void vte_terminal_set_color_highlight(VteTerminal *terminal, const GdkColor *highlight_background) {
}

#if VTE_CHECK_VERSION(0, 36, 0)
void vte_terminal_set_color_highlight_foreground(VteTerminal *terminal,
                                                 const GdkColor *highlight_foreground) {}
#endif

void vte_terminal_set_colors(VteTerminal *terminal, const GdkColor *foreground,
                             const GdkColor *background, const GdkColor *palette,
                             glong palette_size) {
  if (set_colors(terminal, palette, palette_size, sizeof(GdkColor), gdk_color_to_string) &&
      palette_size > 0) {
    if (foreground == NULL) {
      foreground = &palette[7];
    }
    if (background == NULL) {
      background = &palette[0];
    }
  }

  if (foreground) {
    vte_terminal_set_color_foreground(terminal, foreground);
  } else {
    GdkColor color = { 0xc000, 0xc000, 0xc000 };

    vte_terminal_set_color_foreground(terminal, &color);
  }

  if (background) {
    vte_terminal_set_color_background(terminal, background);
  } else {
    GdkColor color = { 0, 0, 0 };

    vte_terminal_set_color_background(terminal, &color);
  }

  /*
   * XXX
   * VTE_BOLD_FG, VTE_DIM_BG, VTE_DEF_HL and VTE_CUR_BG should be reset.
   * (See vte_terminal_set_colors() in vte-0.xx.xx/src/vte.c)
   */
}

#if GTK_CHECK_VERSION(2, 99, 0)
void vte_terminal_set_color_dim_rgba(VteTerminal *terminal, const GdkRGBA *dim) {}
#endif
#else /* VTE_CHECK_VERSION(0,38,0) */
#define vte_terminal_set_color_bold_rgba vte_terminal_set_color_bold
#define vte_terminal_set_color_foreground_rgba vte_terminal_set_color_foreground
#define vte_terminal_set_color_background_rgba vte_terminal_set_color_background
#define vte_terminal_set_color_cursor_rgba vte_terminal_set_color_cursor
#define vte_terminal_set_color_highlight_rgba vte_terminal_set_color_highlight
#define vte_terminal_set_color_highlight_foreground_rgba vte_terminal_set_color_highlight_foreground
#define vte_terminal_set_colors_rgba vte_terminal_set_colors
#endif /* VTE_CHECK_VERSION(0,38,0) */

#if GTK_CHECK_VERSION(2, 99, 0)
void vte_terminal_set_color_bold_rgba(VteTerminal *terminal, const GdkRGBA *bold) {
  set_color_bold(terminal, bold, (gchar*(*)(const void*))gdk_rgba_to_string2);
}

void vte_terminal_set_color_foreground_rgba(VteTerminal *terminal, const GdkRGBA *foreground) {
  set_color_foreground(terminal, foreground, (gchar*(*)(const void*))gdk_rgba_to_string2);
}

void vte_terminal_set_color_background_rgba(VteTerminal *terminal, const GdkRGBA *background) {
  set_color_background(terminal, background, (gchar*(*)(const void*))gdk_rgba_to_string2);
}

void vte_terminal_set_color_cursor_rgba(VteTerminal *terminal, const GdkRGBA *cursor_background) {
  set_color_cursor(terminal, cursor_background, (gchar*(*)(const void*))gdk_rgba_to_string2);
}

#if VTE_CHECK_VERSION(0, 44, 0)
void vte_terminal_set_color_cursor_foreground(VteTerminal *terminal,
                                              const GdkRGBA *cursor_foreground) {
  gchar *str;

  if (!cursor_foreground) {
    return;
  }

  /* #rrrrggggbbbb */
  str = gdk_rgba_to_string2(cursor_foreground);

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "cursor_fg_color", str);
    ui_window_update(&PVT(terminal)->screen->window, 3 /* UPDATE_SCREEN|UPDATE_CURSOR */);
  } else {
    ui_color_manager_set_cursor_bg_color(PVT(terminal)->screen->color_man, str);
  }

  g_free(str);
}
#endif

void vte_terminal_set_color_highlight_rgba(VteTerminal *terminal,
                                           const GdkRGBA *highlight_background) {}

#if VTE_CHECK_VERSION(0, 36, 0)
void vte_terminal_set_color_highlight_foreground_rgba(VteTerminal *terminal,
                                                      const GdkRGBA *highlight_foreground) {}
#endif

void vte_terminal_set_colors_rgba(VteTerminal *terminal, const GdkRGBA *foreground,
                                  const GdkRGBA *background, const GdkRGBA *palette,
                                  gsize palette_size) {
  if (set_colors(terminal, (const void*)palette, palette_size, sizeof(GdkRGBA),
                 (gchar*(*)(const void*))gdk_rgba_to_string2) &&
      palette_size > 0) {
    if (foreground == NULL) {
      foreground = &palette[7];
    }
    if (background == NULL) {
      background = &palette[0];
    }
  }

  if (foreground) {
    vte_terminal_set_color_foreground_rgba(terminal, foreground);
  } else {
    /* 192 = 0xc0 */
    GdkRGBA color = { 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0, 1.0 };

    vte_terminal_set_color_foreground_rgba(terminal, &color);
  }

  if (background) {
    vte_terminal_set_color_background_rgba(terminal, background);
  } else {
    GdkRGBA color = { 0.0, 0.0, 0.0, 1.0 };

    vte_terminal_set_color_background_rgba(terminal, &color);
  }

  /*
   * XXX
   * VTE_BOLD_FG, VTE_HIGHLIGHT_BG, VTE_HIGHLIGHT_FG, VTE_CURSOR_BG and VTE_CURSOR_FG
   * should be reset.
   * (see Terminal::set_colors in vte-0.xx.xx/src/vte.cc)
   */
}
#endif /* GTK_CHECK_VERSION(2,99,0) */

void vte_terminal_set_default_colors(VteTerminal *terminal) {
  set_colors(terminal, NULL, 0, 0, NULL);
}

void vte_terminal_set_background_image(
    VteTerminal *terminal,
    GdkPixbuf *image /* can be NULL and same as current PVT(terminal)->image */
    ) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Setting image %p\n", image);
#endif

  if (PVT(terminal)->image) {
    if (PVT(terminal)->image == image) {
      return;
    }

    g_object_unref(PVT(terminal)->image);
  }

  if ((PVT(terminal)->image = image) == NULL) {
    vte_terminal_set_background_image_file(terminal, "");

    return;
  }

  g_object_ref(image);

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    update_wall_picture(terminal);
  }
}

void vte_terminal_set_background_image_file(VteTerminal *terminal, const char *path) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Setting image file %s\n", path);
#endif

  /*
   * Don't unref PVT(terminal)->image if path is
   * "pixmap:<pixmap id>" (Ex. the case of
   * vte_terminal_set_background_image_file()
   * being called from update_wall_picture().)
   */
  if (PVT(terminal)->image && strncmp(path, "pixmap:", 7) != 0) {
    g_object_unref(PVT(terminal)->image);
    PVT(terminal)->image = NULL;
  }

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "wall_picture", path);
  } else {
    free(PVT(terminal)->screen->pic_file_path);
    PVT(terminal)->screen->pic_file_path = (*path == '\0') ? NULL : strdup(path);
  }
}

void vte_terminal_set_background_tint_color(VteTerminal *terminal, const GdkColor *color) {}

void vte_terminal_set_background_saturation(VteTerminal *terminal, double saturation) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " SATURATION => %f\n", saturation);
#endif

  if (PVT(terminal)->screen->pic_file_path || PVT(terminal)->screen->window.is_transparent) {
    set_alpha(terminal, 255 * (1.0 - saturation));
  }
}

void vte_terminal_set_background_transparent(VteTerminal *terminal, gboolean transparent) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Pseudo transparent %s.\n", transparent ? "on" : "off");
#endif

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    char *value;

    if (transparent) {
      value = "true";
    } else {
      value = "false";
    }

    ui_screen_set_config(PVT(terminal)->screen, NULL, "use_transbg", value);
  } else if (transparent) {
    ui_window_set_transparent(&PVT(terminal)->screen->window,
                              ui_screen_get_picture_modifier(PVT(terminal)->screen));
  }
}

void vte_terminal_set_opacity(VteTerminal *terminal, guint16 opacity) {
  u_int8_t alpha;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " OPACITY => %x\n", opacity);
#endif

  alpha = ((opacity >> 8) & 0xff);

  if (!PVT(terminal)->screen->pic_file_path && !PVT(terminal)->screen->window.is_transparent) {
    set_alpha(terminal, alpha);
  }
}

#if VTE_CHECK_VERSION(0, 17, 1)
void vte_terminal_set_cursor_blink_mode(VteTerminal *terminal, VteCursorBlinkMode mode) {
  char *value;

  if (mode == VTE_CURSOR_BLINK_OFF) {
    value = "false";
  } else {
    value = "true";
  }

  ui_screen_set_config(PVT(terminal)->screen, NULL, "blink_cursor", value);
}

VteCursorBlinkMode vte_terminal_get_cursor_blink_mode(VteTerminal *terminal) {
  if (vt_term_get_cursor_style(PVT(terminal)->term) & CS_BLINK) {
    return VTE_CURSOR_BLINK_ON;
  } else {
    return VTE_CURSOR_BLINK_OFF;
  }
}
#endif

#if VTE_CHECK_VERSION(0, 20, 0)
void vte_terminal_set_cursor_shape(VteTerminal *terminal, VteCursorShape shape) {}

VteCursorShape vte_terminal_get_cursor_shape(VteTerminal *terminal) {
  return VTE_CURSOR_SHAPE_IBEAM;
}
#endif

void vte_terminal_set_scrollback_lines(VteTerminal *terminal, glong lines) {
  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    char value[DIGIT_STR_LEN(glong) + 1];

    sprintf(value, "%ld", lines);

    ui_screen_set_config(PVT(terminal)->screen, NULL, "logsize", value);
  } else {
    vt_term_change_log_size(PVT(terminal)->term, lines);
  }
}

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_im_append_menuitems(VteTerminal *terminal, GtkMenuShell *menushell) {}
#endif

void vte_terminal_set_font_from_string(VteTerminal *terminal, const char *name) {
  char *p;
  int font_changed;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_font_from_string %s\n", name);
#endif

  if (!name || strcmp(name, "(null)") == 0) {
    name = "monospace";
  } else {
    p = ((char*)name) /* avoid warning "discard qualifiers" */ + strlen(name) - 1;
    if ('0' <= *p && *p <= '9') {
      int fontsize;

      do {
        p--;
      } while ('0' <= *p && *p <= '9');

      if ((fontsize = atoi(p + 1)) > 0) {
        /* XXX Screen is redraw in ui_screen_reset_view() below. */
        ui_change_font_size(PVT(terminal)->screen->font_man, atoi(p + 1));
      }
    }

    if ((p = strchr(name, ','))) {
      /*
       * name contains font list like "Ubuntu Mono,monospace 13"
       * (see manual of pango_font_description_from_string())
       */
      char *new_name;

      if (!(new_name = alloca(p - name + 1))) {
        return;
      }

      memcpy(new_name, name, p - name);
      new_name[p - name] = '\0';

      name = new_name;
    }
  }

  font_changed = ui_customize_font_file("aafont", "DEFAULT",
                                        (char*)name /* avoid warning "discard qualifiers" */, 0);
  font_changed |= ui_customize_font_file("aafont", "ISO10646_UCS4_1",
                                         (char*)name /* avoid warning "discard qualifiers" */, 0);
  if (font_changed) {
    ui_font_cache_unload_all();

    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
      ui_screen_reset_view(PVT(terminal)->screen);
    } else {
      /*
       * XXX
       * Forcibly fix width and height members of ui_window_t,
       * or widget->requisition is not set correctly in
       * reset_vte_size_member.
       */
      PVT(terminal)->screen->width = PVT(terminal)->screen->window.width =
          ui_col_width(PVT(terminal)->screen) * vt_term_get_cols(PVT(terminal)->term);
      PVT(terminal)->screen->height = PVT(terminal)->screen->window.height =
          ui_line_height(PVT(terminal)->screen) * vt_term_get_rows(PVT(terminal)->term);

      PVT(terminal)->screen->window.width_inc = PVT(terminal)->screen->window.min_width =
          ui_col_width(PVT(terminal)->screen);
      PVT(terminal)->screen->window.height_inc = PVT(terminal)->screen->window.min_height =
          ui_line_height(PVT(terminal)->screen);
    }

    reset_vte_size_member(terminal);

    if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
      /*
       * gnome-terminal(2.29.6 or later?) is not resized correctly
       * without this.
       */
      gtk_widget_queue_resize_no_redraw(GTK_WIDGET(terminal));
    }
  }
}

void vte_terminal_set_font(VteTerminal *terminal, const PangoFontDescription *font_desc) {
  char *name;

  name = pango_font_description_to_string(font_desc);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set_font %s\n", name);
#endif

  vte_terminal_set_font_from_string(terminal, name);

  g_free(name);
}

const PangoFontDescription *vte_terminal_get_font(VteTerminal *terminal) { return NULL; }

#if VTE_CHECK_VERSION(0, 74, 0)
cairo_font_options_t const *vte_terminal_get_font_options(VteTerminal *terminal) {
  return NULL;
}

void vte_terminal_set_font_options(VteTerminal *terminal,
                                   cairo_font_options_t const *font_options) {}
#endif

void vte_terminal_set_allow_bold(VteTerminal *terminal, gboolean allow_bold) {}

gboolean vte_terminal_get_allow_bold(VteTerminal *terminal) { return TRUE; }

gboolean vte_terminal_get_has_selection(VteTerminal *terminal) {
  if (PVT(terminal)->screen->sel.sel_str && PVT(terminal)->screen->sel.sel_len > 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

#if VTE_CHECK_VERSION(0, 40, 0)
void vte_terminal_set_word_char_exceptions(VteTerminal *terminal, const char *exception) {
  char *seps;

  if (!exception || !*exception) {
    vt_set_word_separators(NULL);

    return;
  }

  if ((seps = alloca(strlen(vt_get_word_separators()) + 1))) {
    char *p = strcpy(seps, vt_get_word_separators());
    char *end = seps + strlen(seps) - 1;
    int do_replace = 0;

    while (*p) {
      if (strchr(exception, *p)) {
        *p = *end;
        *(end--) = '\0';
        do_replace = 1;
      }
      p ++;
    }

    if (do_replace) {
      vt_set_word_separators(seps);
    }
  }
}

const char *vte_terminal_get_word_chars_exceptions(VteTerminal *terminal) {
  return "";
}
#endif

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec) {
  char *sep;

  if (!spec || !*spec) {
    sep = ",. ";
  } else if ((sep = alloca(0x5f))) {
    char *sep_p;
    char c;

    sep_p = sep;
    c = 0x20;

    do {
      const char *spec_p;

      spec_p = spec;

      while (*spec_p) {
        if (*spec_p == '-' && spec_p > spec && *(spec_p + 1) != '\0') {
          if (*(spec_p - 1) < c && c < *(spec_p + 1)) {
            goto next;
          }
        } else if (*spec_p == c) {
          goto next;
        }

        spec_p++;
      }

      *(sep_p++) = c;

    next:
      c++;
    } while (c < 0x7f);

    *(sep_p) = '\0';
  } else {
    return;
  }

  vt_set_word_separators(sep);
}

gboolean vte_terminal_is_word_char(VteTerminal *terminal, gunichar c) { return TRUE; }
#endif

void vte_terminal_set_backspace_binding(VteTerminal *terminal, VteEraseBinding binding) {
  char *seq;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " set backtrace binding => %d\n", binding);
#endif

  if (binding == VTE_ERASE_ASCII_BACKSPACE) {
    seq = "\x08";
  } else if (binding == VTE_ERASE_ASCII_DELETE) {
    seq = "\x7f";
  } else if (binding == VTE_ERASE_DELETE_SEQUENCE) {
    seq = "\x1b[3~";
  }
#if VTE_CHECK_VERSION(0, 20, 4)
  else if (binding == VTE_ERASE_TTY) {
    return;
  }
#endif
  else {
    return;
  }

  vt_termcap_set_key_seq(PVT(terminal)->term->parser->termcap, SPKEY_BACKSPACE, seq);
}

void vte_terminal_set_delete_binding(VteTerminal *terminal, VteEraseBinding binding) {
  char *seq;

  if (binding == VTE_ERASE_ASCII_BACKSPACE) {
    seq = "\x08";
  } else if (binding == VTE_ERASE_ASCII_DELETE) {
    seq = "\x7f";
  } else if (binding == VTE_ERASE_DELETE_SEQUENCE) {
    seq = "\x1b[3~";
  }
#if VTE_CHECK_VERSION(0, 20, 4)
  else if (binding == VTE_ERASE_TTY) {
    return;
  }
#endif
  else {
    return;
  }

  vt_termcap_set_key_seq(PVT(terminal)->term->parser->termcap, SPKEY_DELETE, seq);
}

void vte_terminal_set_mouse_autohide(VteTerminal *terminal, gboolean setting) {}

gboolean vte_terminal_get_mouse_autohide(VteTerminal *terminal) { return TRUE; }

void vte_terminal_reset(VteTerminal *terminal, gboolean full, gboolean clear_history) {
  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_exec_cmd(PVT(terminal)->screen, "full_reset");
  }
}

#if VTE_CHECK_VERSION(0, 76, 0)
char *vte_terminal_get_text_format(VteTerminal *terminal, VteFormat format) {
  return NULL;
}
#endif

char *vte_terminal_get_text(VteTerminal *terminal,
                            gboolean (*is_selected)(VteTerminal *terminal, glong column, glong row,
                                                    gpointer data),
                            gpointer data, GArray *attributes) {
  return NULL;
}

char *vte_terminal_get_text_include_trailing_spaces(VteTerminal *terminal,
                                                    gboolean (*is_selected)(VteTerminal *terminal,
                                                                            glong column, glong row,
                                                                            gpointer data),
                                                    gpointer data, GArray *attributes) {
  return NULL;
}

char *vte_terminal_get_text_range(VteTerminal *terminal, glong start_row, glong start_col,
                                  glong end_row, glong end_col,
                                  gboolean (*is_selected)(VteTerminal *terminal, glong column,
                                                          glong row, gpointer data),
                                  gpointer data, GArray *attributes) {
  return NULL;
}

#if VTE_CHECK_VERSION(0, 70, 0)
static char *get_text_selected(VteTerminal *terminal, VteFormat format, gsize *length) {
  if (length) {
    *length = 0;
  }

  if (!vte_terminal_get_has_selection(terminal)) {
    return NULL;
  }

  return convert_vtstr_to_utf8(PVT(terminal)->screen->sel.sel_str,
                               PVT(terminal)->screen->sel.sel_len,
                               format == VTE_FORMAT_HTML, length);
}

char *vte_terminal_get_text_selected(VteTerminal *terminal, VteFormat format) {
  return get_text_selected(terminal, format, NULL);
}
#endif

#if VTE_CHECK_VERSION(0, 72, 0)
char *vte_terminal_get_text_range_format(VteTerminal *terminal, VteFormat format,
                                         long start_row, long start_col, long end_row,
                                         long end_col, gsize *length) {
  return NULL;
}

char *vte_terminal_get_text_selected_full(VteTerminal *terminal, VteFormat format, gsize *length) {
  return get_text_selected(terminal, format, length);
}
#endif

void vte_terminal_get_cursor_position(VteTerminal *terminal, glong *column, glong *row) {
  *column = vt_term_cursor_col(PVT(terminal)->term);
  *row = vt_term_cursor_row(PVT(terminal)->term);
}

#if GLIB_CHECK_VERSION(2, 14, 0)
int vte_terminal_match_add_gregex(VteTerminal *terminal, GRegex *regex, GRegexMatchFlags flags) {
  /* XXX */
  void *p;

  if ((p = realloc(PVT(terminal)->match_gregexes,
                   sizeof(GRegex*) * (PVT(terminal)->num_match_gregexes + 1))) == NULL) {
    return 0;
  }
  PVT(terminal)->match_gregexes = p;
  PVT(terminal)->match_gregexes[PVT(terminal)->num_match_gregexes++] = regex;
  g_regex_ref(regex);

  if (strstr(g_regex_get_pattern(regex), "http")) {
    /* tag == 1 */
    return 1;
  } else {
    /* tag == 0 */
    return 0;
  }
}
#endif

void vte_terminal_match_set_cursor(VteTerminal *terminal, int tag, GdkCursor *cursor) {}

void vte_terminal_match_set_cursor_type(VteTerminal *terminal, int tag, GdkCursorType cursor_type) {
}

void vte_terminal_match_set_cursor_name(VteTerminal *terminal, int tag, const char *cursor_name) {}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_match_remove_all(VteTerminal *terminal)
#else
void vte_terminal_match_clear_all(VteTerminal *terminal)
#endif
{
#if GLIB_CHECK_VERSION(2, 14, 0)
  if (PVT(terminal)->match_gregexes) {
    u_int count;

    for (count = 0; count < PVT(terminal)->num_match_gregexes; count++) {
      g_regex_unref(PVT(terminal)->match_gregexes[count]);
    }
    free(PVT(terminal)->match_gregexes);
    PVT(terminal)->match_gregexes = NULL;
  }

#if VTE_CHECK_VERSION(0, 46, 0)
  if (PVT(terminal)->match_vregexes) {
    u_int count;

    for (count = 0; count < PVT(terminal)->num_match_vregexes; count++) {
      vte_regex_unref(PVT(terminal)->match_vregexes[count]);
    }
    free(PVT(terminal)->match_vregexes);
    PVT(terminal)->match_vregexes = NULL;
  }
#endif
#endif
}

void vte_terminal_match_remove(VteTerminal *terminal, int tag) {}

char *vte_terminal_match_check(VteTerminal *terminal, glong column, glong row, int *tag) {
  u_char *buf;
  size_t len;

  if (!vte_terminal_get_has_selection(terminal)) {
    return NULL;
  }

  if ((buf = convert_vtstr_to_utf8(PVT(terminal)->screen->sel.sel_str,
                                   PVT(terminal)->screen->sel.sel_len, 0, &len))) {
    /* XXX */
    if (tag) {
      *tag = 1; /* For pattern including "http" (see vte_terminal_match_add_gregex) */
    }
  }

  return buf;
}

#if VTE_CHECK_VERSION(0, 38, 0)
char *vte_terminal_match_check_event(VteTerminal *terminal, GdkEvent *event, int *tag) {
  return NULL;
}
#endif

#if VTE_CHECK_VERSION(0, 70, 0)
char *vte_terminal_check_match_at(VteTerminal *terminal, double x, double y, int *tag) {
  return NULL;
}

char *vte_terminal_check_hyperlink_at(VteTerminal *terminal, double x, double y) {
  return NULL;
}

char **vte_terminal_check_regex_array_at(VteTerminal *terminal, double x, double y,
                                         VteRegex **regexes, gsize n_regexes,
                                         guint32 match_flags, gsize *n_matches) {
  return NULL;
}

gboolean vte_terminal_check_regex_simple_at(VteTerminal *terminal, double x, double y,
                                            VteRegex **regexes, gsize n_regexes,
                                            guint32 match_flags, char **matches) {
  return FALSE;
}
#endif

#if GLIB_CHECK_VERSION(2, 14, 0)
#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_search_set_gregex(VteTerminal *terminal, GRegex *regex, GRegexMatchFlags flags)
#else
void vte_terminal_search_set_gregex(VteTerminal *terminal, GRegex *regex)
#endif
{
  if (regex) {
    if (!PVT(terminal)->gregex) {
      if (!vt_term_search_init(PVT(terminal)->term, -1, -1, match_gregex)) {
        regex = NULL;
      }
    }
  } else {
    if (IS_VTE_SEARCH(terminal)) {
      vt_term_search_final(PVT(terminal)->term);
    }
  }

  PVT(terminal)->gregex = regex;
}

GRegex *vte_terminal_search_get_gregex(VteTerminal *terminal) { return PVT(terminal)->gregex; }

gboolean vte_terminal_search_find_previous(VteTerminal *terminal) {
  return search_find(terminal, 1);
}

gboolean vte_terminal_search_find_next(VteTerminal *terminal) { return search_find(terminal, 0); }
#endif /* VTE_CHECK_VERSION(0, 40, 0) */

#if VTE_CHECK_VERSION(0, 44, 0)
gboolean vte_terminal_event_check_gregex_simple(VteTerminal *terminal, GdkEvent *event,
                                                GRegex **regexes, gsize n_regexes,
                                                GRegexMatchFlags match_flags, char **matches) {
  return FALSE;
}
#endif

void vte_terminal_search_set_wrap_around(VteTerminal *terminal, gboolean wrap_around) {}

gboolean vte_terminal_search_get_wrap_around(VteTerminal *terminal) { return FALSE; }

#if VTE_CHECK_VERSION(0, 28, 0)
char *vte_get_user_shell(void) { return NULL; }
#endif

#if VTE_CHECK_VERSION(0, 44, 0)
const char *vte_get_features(void) { return "-GNUTLS"; }
#endif

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_emulation(VteTerminal *terminal, const char *emulation) {}

const char *vte_terminal_get_emulation(VteTerminal *terminal) { return main_config.term_type; }

const char *vte_terminal_get_default_emulation(VteTerminal *terminal) {
  return main_config.term_type;
}
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
gboolean vte_terminal_set_encoding(VteTerminal *terminal, const char *codeset, GError **error)
#else
void vte_terminal_set_encoding(VteTerminal *terminal, const char *codeset)
#endif
{
  if (codeset == NULL) {
    codeset = "AUTO";
  }

  if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "encoding", codeset);
  } else {
    vt_term_change_encoding(PVT(terminal)->term, vt_get_char_encoding(codeset));
  }

#if VTE_CHECK_VERSION(0, 38, 0)
  if (error) {
    *error = NULL;
  }
#endif

  g_signal_emit_by_name(terminal, "encoding-changed");

#if VTE_CHECK_VERSION(0, 38, 0)
  return TRUE;
#endif
}

const char *vte_terminal_get_encoding(VteTerminal *terminal) {
  return vt_get_char_encoding_name(vt_term_get_encoding(PVT(terminal)->term));
}

#if VTE_CHECK_VERSION(0, 24, 0)
gboolean
#if VTE_CHECK_VERSION(0, 38, 0)
vte_terminal_write_contents_sync(VteTerminal *terminal, GOutputStream *stream, VteWriteFlags flags,
                                 GCancellable *cancellable, GError **error)
#else
vte_terminal_write_contents(VteTerminal *terminal, GOutputStream *stream,
                            VteTerminalWriteFlags flags, GCancellable *cancellable, GError **error)
#endif
{
  char cmd[] = "snapshot vtetmp UTF8";
  char *path;
  gboolean ret;

  vt_term_exec_cmd(PVT(terminal)->term, cmd);

  if (error) {
    *error = NULL;
  }

  ret = TRUE;

  if ((path = bl_get_user_rc_path("mlterm/vtetmp.snp"))) {
    FILE *fp;

    if ((fp = fopen(path, "r"))) {
      char buf[10240];
      size_t len;

      while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        gsize bytes_written;

        if (!g_output_stream_write_all(stream, buf, len, &bytes_written, cancellable, error)) {
          ret = FALSE;
          break;
        }
      }

      fclose(fp);
      unlink(path);
    }

    free(path);
  }

  return ret;
}
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_cjk_ambiguous_width(VteTerminal *terminal, int width) {
  vt_term_set_config(PVT(terminal)->term, "col_size_of_width_a", width == 2 ? "2" : "1");
}

int vte_terminal_get_cjk_ambiguous_width(VteTerminal *terminal) {
  return PVT(terminal)->term->parser->col_size_of_width_a;
}
#endif

#if !VTE_CHECK_VERSION(0, 38, 0)
const char *vte_terminal_get_status_line(VteTerminal *terminal) { return ""; }

void vte_terminal_get_padding(VteTerminal *terminal, int *xpad, int *ypad) {
  *xpad = PVT(terminal)->screen->window.hmargin * 2 /* left + right */;
  *ypad = PVT(terminal)->screen->window.vmargin * 2 /* top + bottom */;
}

void vte_terminal_set_pty(VteTerminal *terminal, int pty_master) {}

int vte_terminal_get_pty(VteTerminal *terminal) {
  return vt_term_get_master_fd(PVT(terminal)->term);
}

GtkAdjustment *vte_terminal_get_adjustment(VteTerminal *terminal) { return ADJUSTMENT(terminal); }
#endif

glong vte_terminal_get_char_width(VteTerminal *terminal) { return CHAR_WIDTH(terminal); }

glong vte_terminal_get_char_height(VteTerminal *terminal) { return CHAR_HEIGHT(terminal); }

glong vte_terminal_get_row_count(VteTerminal *terminal) { return ROW_COUNT(terminal); }

glong vte_terminal_get_column_count(VteTerminal *terminal) { return COLUMN_COUNT(terminal); }

const char *vte_terminal_get_window_title(VteTerminal *terminal) {
  return WINDOW_TITLE(terminal);
}

const char *vte_terminal_get_icon_title(VteTerminal *terminal) { return ICON_TITLE(terminal); }

int vte_terminal_get_child_exit_status(VteTerminal *terminal) { return 0; }

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_cursor_blinks(VteTerminal *terminal, gboolean blink) {
  ui_screen_set_config(PVT(terminal)->screen, NULL, "blink_cursor", blink ? "true" : "false");
}

gboolean vte_terminal_get_using_xft(VteTerminal *terminal) {
  if (ui_get_type_engine(PVT(terminal)->screen->font_man) == TYPE_XFT) {
    return TRUE;
  } else {
    return FALSE;
  }
}

int vte_terminal_match_add(VteTerminal *terminal, const char *match) { return 1; }

glong vte_terminal_get_char_descent(VteTerminal *terminal) { return terminal->char_descent; }

glong vte_terminal_get_char_ascent(VteTerminal *terminal) { return terminal->char_ascent; }

static void set_anti_alias(VteTerminal *terminal, VteTerminalAntiAlias antialias) {
  char *value;
  int term_is_null;

  if (antialias == VTE_ANTI_ALIAS_FORCE_ENABLE) {
    value = "true";
  } else if (antialias == VTE_ANTI_ALIAS_FORCE_ENABLE) {
    value = "false";
  } else {
    return;
  }

  /*
   * XXX
   * Hack for the case of calling this function before fork pty because
   * change_font_present() in ui_screen.c calls vt_term_get_vertical_mode().
   */
  if (PVT(terminal)->screen->term == NULL) {
    PVT(terminal)->screen->term = PVT(terminal)->term;
    term_is_null = 1;
  } else {
    term_is_null = 0;
  }

  ui_screen_set_config(PVT(terminal)->screen, NULL, "use_anti_alias", value);

  if (term_is_null) {
    PVT(terminal)->screen->term = NULL;
  }
}

void vte_terminal_set_font_full(VteTerminal *terminal, const PangoFontDescription *font_desc,
                                VteTerminalAntiAlias antialias) {
  set_anti_alias(terminal, antialias);
  vte_terminal_set_font(terminal, font_desc);
}

void vte_terminal_set_font_from_string_full(VteTerminal *terminal, const char *name,
                                            VteTerminalAntiAlias antialias) {
  set_anti_alias(terminal, antialias);
  vte_terminal_set_font_from_string(terminal, name);
}
#endif

#if VTE_CHECK_VERSION(0, 26, 0)

#ifndef USE_WIN32API
#include <sys/ioctl.h>
#endif
#include <vt_pty_intern.h>  /* XXX in order to operate vt_pty_t::child_pid directly. */
#include <pobl/bl_config.h> /* HAVE_SETSID */

struct _VtePty {
  GObject parent_instance;

  VteTerminal *terminal;
  VtePtyFlags flags;
};

struct _VtePtyClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE(VtePty, vte_pty, G_TYPE_OBJECT);

static void vte_pty_init(VtePty *pty) {}

static void vte_pty_class_init(VtePtyClass *kclass) {}

#if VTE_CHECK_VERSION(0, 38, 0)
VtePty *vte_terminal_pty_new_sync(VteTerminal *terminal, VtePtyFlags flags,
                                  GCancellable *cancellable, GError **error) {
  VtePty *pty;

  if (error) {
    *error = NULL;
  }

  if (PVT(terminal)->pty) {
    return PVT(terminal)->pty;
  }

  if (!(pty = vte_pty_new_sync(flags, cancellable, error))) {
    return NULL;
  }

  vte_terminal_set_pty(terminal, pty);

  return pty;
}
#else
VtePty *vte_terminal_pty_new(VteTerminal *terminal, VtePtyFlags flags, GError **error) {
  VtePty *pty;

  if (error) {
    *error = NULL;
  }

  if (PVT(terminal)->pty) {
    return PVT(terminal)->pty;
  }

  if (!(pty = vte_pty_new(flags, error))) {
    return NULL;
  }

  vte_terminal_set_pty_object(terminal, pty);

  return pty;
}
#endif

#if !VTE_CHECK_VERSION(0, 38, 0)
void vte_pty_set_term(VtePty *pty, const char *emulation) {
  if (pty->terminal) {
    vte_terminal_set_emulation(pty->terminal, emulation);
  }
}
#endif

#if VTE_CHECK_VERSION(0, 38, 0)
VtePty *vte_terminal_get_pty(VteTerminal *terminal)
#else
VtePty *vte_terminal_get_pty_object(VteTerminal *terminal)
#endif
{
  return PVT(terminal)->pty;
}

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_pty(VteTerminal *terminal, VtePty *pty)
#else
void vte_terminal_set_pty_object(VteTerminal *terminal, VtePty *pty)
#endif
{
  pid_t pid;

  if (PVT(terminal)->pty || !pty) {
    return;
  }

  pty->terminal = terminal;
  PVT(terminal)->pty = g_object_ref(pty);

#if !VTE_CHECK_VERSION(0, 38, 0)
  vte_pty_set_term(pty, vte_terminal_get_emulation(terminal));
#endif

  pid = vte_terminal_forkpty(terminal, NULL, NULL, (pty->flags & VTE_PTY_NO_LASTLOG) ? FALSE : TRUE,
                             (pty->flags & VTE_PTY_NO_UTMP) ? FALSE : TRUE,
                             (pty->flags & VTE_PTY_NO_WTMP) ? FALSE : TRUE);

  if (pid == 0) {
    /* Child process exits, but master and slave fds survives. */
    exit(0);
  }

#if 1
#ifndef USE_WIN32API
  if (pid > 0) {
    waitpid(pid, NULL, WNOHANG);
  }
#endif
#else
  if (!vt_term_is_zombie(PVT(terminal)->term)) {
    /* Don't catch exit(0) above. */
    PVT(terminal)->term->pty->child_pid = -1;
  }
#endif
}

VtePty *
#if VTE_CHECK_VERSION(0, 38, 0)
vte_pty_new_sync(VtePtyFlags flags, GCancellable *cancellable, GError **error)
#else
vte_pty_new(VtePtyFlags flags, GError **error)
#endif
{
  VtePty *pty;

  if ((pty = g_object_new(VTE_TYPE_PTY, NULL))) {
    pty->flags = flags;
    pty->terminal = NULL;
  }

  if (error) {
    *error = NULL;
  }

  return pty;
}

VtePty *
#if VTE_CHECK_VERSION(0, 38, 0)
vte_pty_new_foreign_sync(int fd, GCancellable *cancellable, GError **error)
#else
vte_pty_new_foreign(int fd, GError **error)
#endif
{
  if (error) {
    error = NULL;
  }

  return NULL;
}

void vte_pty_close(VtePty *pty) {}

#if VTE_CHECK_VERSION(0, 48, 0)
void vte_pty_spawn_async(VtePty *pty, const char *working_directory, char **argv,
                         char **envv, GSpawnFlags spawn_flags,
                         GSpawnChildSetupFunc child_setup, gpointer child_setup_data,
                         GDestroyNotify child_setup_data_destroy, int timeout,
                         GCancellable *cancellable, GAsyncReadyCallback callback,
                         gpointer user_data) {}

gboolean vte_pty_spawn_finish(VtePty *pty, GAsyncResult *result, GPid *child_pid, GError **error) {
  if (error) {
    *error = NULL;
  }

  return FALSE;
}

#endif

#if (!defined(HAVE_SETSID) && defined(TIOCNOTTY)) || !defined(TIOCSCTTY)
#include <fcntl.h>
#endif

/* Child process (before exec()) */
void vte_pty_child_setup(VtePty *pty) {
#ifndef USE_WIN32API
  int slave;
  int master;
#if (!defined(HAVE_SETSID) && defined(TIOCNOTTY)) || !defined(TIOCSCTTY)
  int fd;
#endif

  if (!pty->terminal) {
    return;
  }

#ifdef HAVE_SETSID
  setsid();
#else
#ifdef TIOCNOTTY
  if ((fd = open("/dev/tty", O_RDWR | O_NOCTTY)) >= 0) {
    ioctl(fd, TIOCNOTTY, NULL);
    close(fd);
  }
#endif
#endif

  master = vt_term_get_master_fd(PVT(pty->terminal)->term);
  slave = vt_term_get_slave_fd(PVT(pty->terminal)->term);

#ifdef TIOCSCTTY
  while (ioctl(slave, TIOCSCTTY, NULL) == -1) {
    /*
     * Child process which exits in vte_terminal_set_pty() may still have
     * controll terminal.
     */
    bl_usleep(100);
  }
#else
  if ((fd = open("/dev/tty", O_RDWR | O_NOCTTY)) >= 0) {
    close(fd);
  }
  if ((fd = open(ptsname(master), O_RDWR)) >= 0) {
    close(fd);
  }
  if ((fd = open("/dev/tty", O_WRONLY)) >= 0) {
    close(fd);
  }
#endif

  dup2(slave, 0);
  dup2(slave, 1);
  dup2(slave, 2);

  if (slave > STDERR_FILENO) {
    close(slave);
  }

  close(master);
#endif /* USE_WIN32API */
}

int vte_pty_get_fd(VtePty *pty) {
  /*
   * XXX
   * sakura 3.6.0 calls vte_pty_get_fd(NULL) if "Close tab" of right-click menu is selected.
   * (This is because vte_pty_get_pty() in this file can return NULL.)
   */
  if (!pty || !pty->terminal) {
    return -1;
  }

  return vt_term_get_master_fd(PVT(pty->terminal)->term);
}

gboolean vte_pty_set_size(VtePty *pty, int rows, int columns, GError **error) {
  if (error) {
    *error = NULL;
  }

  if (!pty->terminal) {
    return FALSE;
  }

  vte_terminal_set_size(pty->terminal, columns, rows);

  return TRUE;
}

gboolean vte_pty_get_size(VtePty *pty, int *rows, int *columns, GError **error) {
  if (error) {
    *error = NULL;
  }

  if (!pty->terminal) {
    return FALSE;
  }

  *columns = COLUMN_COUNT(pty->terminal);
  *rows = ROW_COUNT(pty->terminal);

  return TRUE;
}

gboolean vte_pty_set_utf8(VtePty *pty, gboolean utf8, GError **error) {
  if (error) {
    *error = NULL;
  }

  if (!pty->terminal) {
    return FALSE;
  }

  return vt_term_change_encoding(PVT(pty->terminal)->term,
                                 utf8 ? VT_UTF8 : vt_get_char_encoding("auto"));
}

void vte_terminal_watch_child(VteTerminal *terminal, GPid child_pid) {
  vte_reaper_add_child(child_pid);

  if (!vt_term_is_zombie(PVT(terminal)->term)) {
    /* GPid in win32 is void* (HANDLE). (see glib/gmain.h) */
    PVT(terminal)->term->pty->child_pid = (pid_t)child_pid;
  }
}

#endif

#if VTE_CHECK_VERSION(0, 38, 0)
void vte_terminal_set_input_enabled(VteTerminal *terminal, gboolean enabled) {}

gboolean vte_terminal_get_input_enabled(VteTerminal *terminal) { return TRUE; }

void vte_terminal_get_geometry_hints(VteTerminal *terminal, GdkGeometry *hints, int min_rows,
                                     int min_columns) {
  hints->base_width = PVT(terminal)->screen->window.hmargin * 2;
  hints->base_height = PVT(terminal)->screen->window.vmargin * 2;
  hints->width_inc = CHAR_WIDTH(terminal);
  hints->height_inc = CHAR_HEIGHT(terminal);
  hints->min_width = hints->base_width + hints->width_inc * min_columns;
  hints->min_height = hints->base_height + hints->height_inc * min_rows;
}

void vte_terminal_set_geometry_hints_for_window(VteTerminal *terminal, GtkWindow *window) {
  GdkGeometry hints;

  vte_terminal_get_geometry_hints(terminal, &hints, 1, 1);
  gtk_window_set_geometry_hints(window,
#if GTK_CHECK_VERSION(3, 19, 5)
                                NULL,
#else
                                GTK_WIDGET(terminal),
#endif
                                &hints,
                                (GdkWindowHints)(GDK_HINT_RESIZE_INC |
                                                 GDK_HINT_MIN_SIZE |
                                                 GDK_HINT_BASE_SIZE));
}
#endif

#if VTE_CHECK_VERSION(0, 62, 0)
VteFeatureFlags vte_get_feature_flags(void) {
  return
#if 1
    VTE_FEATURE_FLAG_BIDI |
#endif
#if 0
    VTE_FEATURE_FLAG_ICU |
#endif
#if 0
    VTE_FEATURE_FLAG_SYSTEMD
#endif
    0;
}
#endif

#if VTE_CHECK_VERSION(0, 39, 0)
guint vte_get_major_version(void) { return VTE_MAJOR_VERSION; }

guint vte_get_minor_version(void) { return VTE_MINOR_VERSION; }

guint vte_get_micro_version(void) { return VTE_MICRO_VERSION; }
#endif

#if VTE_CHECK_VERSION(0, 34, 0)
const char *vte_terminal_get_current_directory_uri(VteTerminal *terminal) { return NULL; }

const char *vte_terminal_get_current_file_uri(VteTerminal *terminal) { return NULL; }
#endif

/* Ubuntu original function ? */
void vte_terminal_set_alternate_screen_scroll(VteTerminal *terminal, gboolean scroll) {}

/* Hack for input method module */

static GIOChannel *gio;
static guint src_id;

int ui_event_source_add_fd(int fd, void (*handler)(void)) {
  if (gio) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " ui_event_source_add_fd failed\n");
#endif

    return 0;
  }

  gio = g_io_channel_unix_new(fd);
  src_id = g_io_add_watch(gio, G_IO_IN, (GIOFunc)handler, NULL);

  return 1;
}

int ui_event_source_remove_fd(int fd) {
  if (gio && g_io_channel_unix_get_fd(gio) == fd) {
    g_source_destroy(g_main_context_find_source_by_id(NULL, src_id));
    g_io_channel_unref(gio);

    gio = NULL;
  }

  return 1;
}

/*
 * GTK+-3.0 supports XInput2 which enables multi device.
 * But XInput2 disables popup menu and shortcut key which applications
 * using libvte show, because mlterm doesn't support it.
 * So multi device is disabled for now.
 *
 * __attribute__((constructor)) hack is necessary because
 * gdk_disable_multidevice() must be called before gtk_init().
 */
#if GTK_CHECK_VERSION(2, 90, 0) && __GNUC__
static void __attribute__((constructor)) init_vte(void) {
#if 0
  setenv("GDK_CORE_DEVICE_EVENTS", "1", 1);
#else
  gdk_disable_multidevice();
#endif
}
#endif

#if VTE_CHECK_VERSION(0, 46, 0)
VteRegex *vte_regex_ref(VteRegex *regex) {
  g_return_val_if_fail(regex, NULL);

  g_atomic_int_inc(&regex->ref_count);

  return regex;
}

VteRegex *vte_regex_unref(VteRegex *regex) {
  g_return_val_if_fail(regex, NULL);

  if (g_atomic_int_dec_and_test(&regex->ref_count)) {
    g_regex_unref(regex->gregex);
    g_slice_free(VteRegex, regex);
  }

  return NULL;
}

VteRegex *vte_regex_new_for_match(const char *pattern, gssize pattern_length,
                                  guint32 flags, GError **error) {
  VteRegex *regex = g_slice_new(VteRegex);
  regex->ref_count = 1;
  regex->gregex = g_regex_new(pattern, 0, 0, error);

  return regex;
}

#if VTE_CHECK_VERSION(0, 76, 0)
VteRegex *vte_regex_new_for_match_full(char const *pattern, gssize pattern_length,
                                       uint32_t flags, uint32_t extra_flags,
                                       gsize *error_offset, GError **error) {
  return vte_regex_new_for_match(pattern, pattern_length, flags, error);
}
#endif

VteRegex *vte_regex_new_for_search(const char *pattern, gssize pattern_length,
                                   guint32 flags, GError **error) {
  VteRegex *regex = g_slice_new(VteRegex);
  regex->ref_count = 1;
  regex->gregex = g_regex_new(pattern, 0, 0, error);

  return regex;
}

#if VTE_CHECK_VERSION(0, 76, 0)
VteRegex *vte_regex_new_for_search_full(char const *pattern, gssize pattern_length,
                                        uint32_t flags, uint32_t extra_flags,
                                        gsize *error_offset, GError **error) {
  return vte_regex_new_for_search(pattern, pattern_length, flags, error);
}
#endif

gboolean vte_regex_jit(VteRegex *regex, guint32 flags, GError **error) {
  if (error) {
    *error = NULL;
  }

  return TRUE;
}

int vte_terminal_match_add_regex(VteTerminal *terminal, VteRegex *regex, guint32 flags) {
  /* XXX */
  void *p;

  if ((p = realloc(PVT(terminal)->match_vregexes,
                   sizeof(VteRegex*) * (PVT(terminal)->num_match_vregexes + 1))) == NULL) {
    return 0;
  }
  PVT(terminal)->match_vregexes = p;
  PVT(terminal)->match_vregexes[PVT(terminal)->num_match_vregexes++] = regex;
  vte_regex_ref(regex);

  if (strstr(g_regex_get_pattern(regex->gregex), "http")) {
    /* tag == 1 */
    return 1;
  } else {
    /* tag == 0 */
    return 0;
  }
}

gboolean vte_terminal_event_check_regex_simple(VteTerminal *terminal, GdkEvent *event,
                                               VteRegex **regexes, gsize n_regexes,
                                               guint32 match_flags, char **matches) {
  return FALSE;
}

void vte_terminal_search_set_regex(VteTerminal *terminal, VteRegex *regex, guint32 flags) {
  if (regex) {
    if (!PVT(terminal)->vregex &&
        !vt_term_search_init(PVT(terminal)->term, -1, -1, match_vteregex)) {
        regex = NULL;
    } else {
      vte_regex_ref(regex);
    }
  } else {
    if (IS_VTE_SEARCH(terminal)) {
      vt_term_search_final(PVT(terminal)->term);
    }
  }

  if (PVT(terminal)->vregex) {
    vte_regex_unref(PVT(terminal)->vregex);
  }

  PVT(terminal)->vregex = regex;
}

VteRegex *vte_terminal_search_get_regex(VteTerminal *terminal) { return PVT(terminal)->vregex; }
#endif

#if VTE_CHECK_VERSION(0, 50, 0)
gboolean vte_terminal_get_allow_hyperlink(VteTerminal *terminal) {
  return FALSE;
}

void vte_terminal_set_allow_hyperlink(VteTerminal *terminal, gboolean allow_hyperlink) {}

char *vte_terminal_hyperlink_check_event(VteTerminal *terminal, GdkEvent *event) { return NULL; }
#endif

#if VTE_CHECK_VERSION(0, 62, 0)
char **vte_terminal_event_check_regex_array(VteTerminal *terminal,
                                            GdkEvent *event,
                                            VteRegex **regexes,
                                            gsize n_regexes,
                                            guint32 match_flags,
                                            gsize *n_matches) {
  return NULL;
}
#endif

#if VTE_CHECK_VERSION(0, 52, 0)
void vte_terminal_set_bold_is_bright(VteTerminal *terminal, gboolean bold_is_bright) {};

gboolean vte_terminal_get_bold_is_bright(VteTerminal *terminal) { return TRUE; };

void vte_terminal_set_cell_width_scale(VteTerminal *terminal, double scale) {};

double vte_terminal_get_cell_width_scale(VteTerminal *terminal) {
  return ui_col_width(PVT(terminal)->screen);
}

void vte_terminal_set_cell_height_scale(VteTerminal *terminal, double scale) {};

double vte_terminal_get_cell_height_scale(VteTerminal *terminal) {
  return ui_line_height(PVT(terminal)->screen);
}

void vte_terminal_set_text_blink_mode(VteTerminal *terminal,
                                      VteTextBlinkMode text_blink_mode) {}

VteTextBlinkMode vte_terminal_get_text_blink_mode(VteTerminal *terminal) {
  return VTE_TEXT_BLINK_FOCUSED;
}
#endif

#if VTE_CHECK_VERSION(0, 54, 0)
void vte_set_test_flags(guint64 flags) {}

void vte_terminal_get_color_background_for_draw(VteTerminal *terminal, GdkRGBA *color) {}
#endif

#if VTE_CHECK_VERSION(0, 62, 0)
void vte_terminal_set_enable_sixel(VteTerminal *terminal, gboolean enabled) {}

gboolean vte_terminal_get_enable_sixel(VteTerminal *terminal) {
  return TRUE;
}
#endif

#if VTE_CHECK_VERSION(0, 76, 0)
void vte_terminal_set_xalign(VteTerminal *terminal, VteAlign align) {}

VteAlign vte_terminal_get_xalign(VteTerminal *terminal) {
  return VTE_ALIGN_START;
}

void vte_terminal_set_yalign(VteTerminal *terminal, VteAlign align) {}

VteAlign vte_terminal_get_yalign(VteTerminal *terminal) {
  return VTE_ALIGN_START;
}

void vte_terminal_set_xfill(VteTerminal *terminal, gboolean fill) {}

gboolean vte_terminal_get_xfill(VteTerminal *terminal) {
  return TRUE;
}

void vte_terminal_set_yfill(VteTerminal *terminal, gboolean fill) {}

gboolean vte_terminal_get_yfill(VteTerminal *terminal) {
  return TRUE;
}

void vte_terminal_set_context_menu_model(VteTerminal *terminal, GMenuModel *model) {}

GMenuModel *vte_terminal_get_context_menu_model(VteTerminal *terminal) {
  return NULL;
}

void vte_terminal_set_context_menu(VteTerminal *terminal, GtkWidget *menu) {}

GtkWidget *vte_terminal_get_context_menu(VteTerminal* terminal) {
  return NULL;
}

#if _VTE_GTK == 3
GdkEvent *vte_event_context_get_event(VteEventContext const *context) {
  return NULL;
}
#elif _VTE_GTK == 4
gboolean vte_event_context_get_coordinates(VteEventContext const *context,
                                           double *x, double *y) {
  return FALSE;
}
#endif

#endif

#if VTE_CHECK_VERSION(0, 78, 0)
int vte_install_termprop(char const *name, VtePropertyType type, VtePropertyFlags flags) {
  return 0;
}

int vte_install_termprop_alias(char const *name, char const *target_name) {
  return 0;
}

char const **vte_get_termprops(gsize *length) {
  *length = 0;

  return NULL;
}

gboolean vte_query_termprop(char const *name, char const **resolved_name, int *prop,
                            VtePropertyType *type, VtePropertyFlags *flags) {
  return FALSE;
}

gboolean vte_query_termprop_by_id(int prop, char const **name, VtePropertyType *type,
                                  VtePropertyFlags *flags) {
  return FALSE;
}

guint64 vte_get_test_flags(void) {
  return 0;
}

gboolean vte_terminal_get_enable_a11y(VteTerminal *terminal) {
  return FALSE;
}

void vte_terminal_set_enable_a11y(VteTerminal *terminal, gboolean enable_a11y) {}

void vte_terminal_set_suppress_legacy_signals(VteTerminal *terminal) {}

void vte_terminal_set_enable_legacy_osc777(VteTerminal *terminal, gboolean enable) {}

gboolean vte_terminal_get_enable_legacy_osc777(VteTerminal *terminal) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_bool(VteTerminal *terminal,
                                        char const *prop, gboolean *valuep)  {
  return FALSE;
}

gboolean vte_terminal_get_termprop_bool_by_id(VteTerminal *terminal, int prop, gboolean *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_int(VteTerminal *terminal,
                                       char const *prop, int64_t *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_int_by_id(VteTerminal *terminal, int prop, int64_t *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_uint(VteTerminal *terminal, char const *prop, uint64_t *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_uint_by_id(VteTerminal *terminal,
                                              int prop, uint64_t *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_double(VteTerminal *terminal, char const *prop, double *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_double_by_id(VteTerminal *terminal, int prop, double *valuep) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_rgba(VteTerminal *terminal, char const *prop, GdkRGBA *color) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_rgba_by_id(VteTerminal *terminal, int prop, GdkRGBA *color) {
  return FALSE;
}

char const *vte_terminal_get_termprop_string(VteTerminal *terminal,
                                             char const *prop, size_t *size) {
  return NULL;
}

char const *vte_terminal_get_termprop_string_by_id(VteTerminal *terminal, int prop, size_t *size) {
  return NULL;
}

char *vte_terminal_dup_termprop_string(VteTerminal *terminal, char const *prop, size_t *size) {
  return NULL;
}

char *vte_terminal_dup_termprop_string_by_id(VteTerminal *terminal, int prop, size_t *size) {
  return NULL;
}

uint8_t const *vte_terminal_get_termprop_data(VteTerminal *terminal,
                                              char const *prop, size_t *size) {
  return NULL;
}

uint8_t const *vte_terminal_get_termprop_data_by_id(VteTerminal *terminal, int prop, size_t *size) {
  return NULL;
}

GBytes *vte_terminal_ref_termprop_data_bytes(VteTerminal *terminal, char const *prop) {
  return NULL;
}

GBytes *vte_terminal_ref_termprop_data_bytes_by_id(VteTerminal *terminal, int prop) {
  return NULL;
}

VteUuid *vte_terminal_dup_termprop_uuid(VteTerminal *terminal, char const *prop) {
  return NULL;
}

VteUuid *vte_terminal_dup_termprop_uuid_by_id(VteTerminal *terminal, int prop) {
  return NULL;
}

GUri *vte_terminal_ref_termprop_uri(VteTerminal *terminal, char const *prop) {
  return NULL;
}

GUri *vte_terminal_ref_termprop_uri_by_id(VteTerminal *terminal, int prop) {
  return NULL;
}

gboolean vte_terminal_get_termprop_value(VteTerminal *terminal, char const *prop, GValue *gvalue) {
  return FALSE;
}

gboolean vte_terminal_get_termprop_value_by_id(VteTerminal *terminal, int prop, GValue *gvalue) {
  return FALSE;
}

GVariant *vte_terminal_ref_termprop_variant(VteTerminal *terminal, char const *prop) {
  return NULL;
}

GVariant *vte_terminal_ref_termprop_variant_by_id(VteTerminal *terminal, int prop) {
  return NULL;
}
#endif

#if VTE_CHECK_VERSION(0, 80, 0)
cairo_surface_t *vte_terminal_ref_termprop_image_surface_by_id(VteTerminal* terminal,
                                                               int prop) {
  return NULL;
}

cairo_surface_t *vte_terminal_ref_termprop_image_surface(VteTerminal* terminal,
                                                         char const* prop) {
  return NULL;
}

#if _VTE_GTK == 3
GdkPixbuf *vte_terminal_ref_termprop_image_pixbuf_by_id(VteTerminal* terminal,
                                                        int prop) {
  return NULL;
}

GdkPixbuf *vte_terminal_ref_termprop_image_pixbuf(VteTerminal* terminal,
                                                  char const* prop) {
  return NULL;
}
#elif _VTE_GTK == 4
GdkTexture *vte_terminal_ref_termprop_image_texture_by_id(VteTerminal* terminal,
                                                          int prop) {
  return NULL;
}

GdkTexture *vte_terminal_ref_termprop_image_texture(VteTerminal* terminal,
                                                    char const* prop) {
  return NULL;
}
#endif
#endif

#if VTE_CHECK_VERSION(0, 56, 0)
char *vte_regex_substitute(VteRegex *regex, const char *subject, const char *replacement,
                           guint32 flags, GError **error) {
  return NULL;
}
#endif

#if VTE_CHECK_VERSION(0, 58, 0)
void vte_terminal_set_enable_bidi(VteTerminal *terminal, gboolean enable_bidi) {
  /* Prefer setting in ~/.mlterm/main. */
#if 0
  if (PVT(terminal)->screen->term) {
    ui_screen_set_config(PVT(terminal)->screen, NULL, "use_ctl", enable_bidi ? "true" : "false");
  }
#endif
}

gboolean vte_terminal_get_enable_bidi(VteTerminal *terminal) {
  if (vt_term_is_using_ctl(PVT(terminal)->screen->term)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

void vte_terminal_set_enable_shaping(VteTerminal *terminal, gboolean enable_shaping) {}

gboolean vte_terminal_get_enable_shaping(VteTerminal *terminal) {
  return vte_terminal_get_enable_bidi(terminal);
}
#endif

#if VTE_CHECK_VERSION(0, 60, 0)
char **vte_get_encodings(gboolean include_aliases) {
  return calloc(1, sizeof(char*));
}

gboolean vte_get_encoding_supported(const char *encoding) {
  return FALSE;
}
#endif

#if VTE_CHECK_VERSION(0, 64, 0)
void vte_terminal_set_enable_fallback_scrolling(VteTerminal *terminal, gboolean enable) {
}

gboolean vte_terminal_get_enable_fallback_scrolling(VteTerminal *terminal) {
  return FALSE;
}
#endif

#if VTE_CHECK_VERSION(0, 66, 0)
void vte_terminal_set_scroll_unit_is_pixels(VteTerminal *terminal, gboolean enable) {}

gboolean vte_terminal_get_scroll_unit_is_pixels(VteTerminal *terminal) {
  return FALSE;
}
#endif


#if VTE_CHECK_VERSION(0, 78, 0)
#include <vte/vteuuid.h>

G_DEFINE_BOXED_TYPE(VteUuid, vte_uuid, vte_uuid_dup,
                    (GBoxedFreeFunc)vte_uuid_free)

VteUuid *vte_uuid_new_v4(void) { return NULL; }

VteUuid *vte_uuid_new_v5(VteUuid const *ns, char const *data, gssize len) { return NULL; }

VteUuid *vte_uuid_new_from_string(char const *str, gssize len, VteUuidFormat fmt) { return NULL; }

VteUuid *vte_uuid_dup(VteUuid const *uuid) { return NULL; }

void vte_uuid_free(VteUuid *uuid) {}

char *vte_uuid_free_to_string(VteUuid *uuid, VteUuidFormat fmt, gsize *len) { return NULL; }

char *vte_uuid_to_string(VteUuid const *uuid, VteUuidFormat fmt, gsize *len) { return NULL; }

gboolean vte_uuid_equal(VteUuid const *uuid, VteUuid const *other) { return FALSE; }

gboolean vte_uuid_validate_string(char const *str, gssize len, VteUuidFormat fmt) { return FALSE; }
#endif
