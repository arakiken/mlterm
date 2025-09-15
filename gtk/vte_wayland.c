/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#if _VTE_GTK == 3
#include <gdk/gdkwayland.h>
#else
#include <gdk/wayland/gdkwayland.h>
#endif
#include <locale.h>

#if _VTE_GTK >= 4
#define FocusOut 10 /* see wayland/ui_display.h */

#define gdk_wayland_window_get_wl_surface(window) gdk_wayland_surface_get_wl_surface(window)
typedef GdkSurface GdkWindow;
#endif

/* wayland/ui_display.h */
int ui_display_resize(ui_display_t *disp, u_int width, u_int height);
void ui_display_move(ui_display_t *disp, int x, int y);

/* not declared in header files. */
void ui_display_init_wlserv(ui_wlserv_t *wlserv);
void ui_display_map(ui_display_t *disp, int dummy);
void ui_display_unmap(ui_display_t *disp);

/* --- static functions --- */

static void display_move(VteTerminal *terminal, int x, int y) {
  GtkAllocation alloc;

  if (x == 0 && y == 0) {
    /*
     * (roxterm-3.11.1)
     * x and y of VteTerminal's GtkAllocation is 0.
     * It is necessary to check GtkAllocation of the parent widget.
     */
    GtkWidget *parent;

    if ((parent = gtk_widget_get_parent(GTK_WIDGET(terminal))) == NULL) {
      return;
    }

    gtk_widget_get_allocation(parent, &alloc);
    x = alloc.x;
    y = alloc.y;
  }

  ui_display_move(PVT(terminal)->screen->window.disp, x, y);
}

static void show_root(ui_display_t *disp, GtkWidget *widget) {
  GdkWindow *window = gtk_widget_get_window(widget);
  ui_window_t *win = &PVT(VTE_TERMINAL(widget))->screen->window;
  const char *class;
  static GdkCursor *cursor;

  /*
   * Don't call gdk_wayland_window_set_use_custom_surface(), which disables VteTerminal
   * to be mapped.
   */

#if 1
  class = g_get_application_name(); /* returns "Terminal" */
#if 0
  setlocale(LC_MESSAGES, "");
  bind_textdomain_codeset("gnome-terminal", "UTF-8");
  class = dgettext("gnome-terminal", class);
#endif
#else
  class = gdk_get_program_class(); /* returns "Gnome-terminal" */
#endif

  /* Internally calls create_shm_buffer() and *set_listener */
  ui_display_show_root(disp, win, 0, 0, 0, class, NULL, gdk_wayland_window_get_wl_surface(window));

  /*
   * XXX
   * pointer_handle_enter() in gdk/gdkdevice-wayland.c uses wl_surface_get_user_data(surface)
   * for GdkWaylandSeat::pointer_info::focus which is used in
   * gdk_wayland_device_window_at_position() which is called from
   * gdk_device_get_window_at_position() in gdk_wayland_window_map()
   * when you want to show popup menu by clicking right button.
   * Then, pointer_handle_button() in gdkdevice-wayland.c works.
   */
  wl_surface_set_user_data(win->disp->display->surface, window);

  if (cursor == NULL) {
#if _VTE_GTK == 3
    cursor = gdk_cursor_new_for_display(gdk_window_get_display(window), GDK_XTERM);
#else
    cursor = gdk_cursor_new_from_name("text", NULL);
#endif
  }

  /* wl_surface_set_user_data() above disables cursor settings in wayland/ui_display.c */
#if _VTE_GTK == 3
  gdk_window_set_cursor(window, cursor);
#else
  gtk_widget_set_cursor(widget, cursor);
#endif
}

static int is_initial_allocation(GtkAllocation *allocation);

static void vte_terminal_map(GtkWidget *widget) {
  GtkAllocation alloc;

  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->map)(widget);

  gtk_widget_get_allocation(widget, &alloc);
  ui_display_map(PVT(VTE_TERMINAL(widget))->screen->window.disp,
                 is_initial_allocation(&alloc));
  display_move(VTE_TERMINAL(widget), alloc.x, alloc.y);
}

static void vte_terminal_unmap(GtkWidget *widget) {
  /* Multiple displays can coexist on wayland, so '&disp' isn't used. */
  ui_display_unmap(PVT(VTE_TERMINAL(widget))->screen->window.disp);

  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->unmap)(widget);
}

static void init_display(ui_display_t *disp, VteTerminalClass *vclass) {
  GdkDisplay *gdkdisp = gdk_display_get_default();
  static Display display;
  static ui_wlserv_t wlserv;
  static struct ui_xkb xkb;
  struct rgb_info rgbinfo = {0, 0, 0, 16, 8, 0};

  disp->display = &display;
  disp->name = strdup(gdk_display_get_name(gdkdisp));
  if (!strstr(disp->name, "wayland")) {
#if _VTE_GTK == 3
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s is not supported on wayland", disp->name);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
#else
    char *msg = alloca(strlen(disp->name) + 41);
    sprintf(msg, "  %s display is not supported on wayland  ", disp->name);
    gtk_dialog_run(msg);
#endif
    exit(1);
  }

  disp->depth = 32;

  display.wlserv = &wlserv;
  display.rgbinfo = rgbinfo;
  display.bytes_per_pixel = 4;

  wlserv.xkb = &xkb;
  wlserv.display = gdk_wayland_display_get_wl_display(gdkdisp);
  bl_file_set_cloexec(wl_display_get_fd(wlserv.display));

  wlserv.registry = wl_display_get_registry(wlserv.display);
#if 0
  wlserv.xdg_shell = gdk_wayland_display_get_xdg_shell(gdkdisp);
  wlserv.compositor = gdk_wayland_display_get_wl_compositor(gdkdisp);
  {
    GdkSeat *gdkseat = gdk_display_get_default_seat(gdkdisp);
    wlserv.seat = gdk_wayland_device_get_wl_seat(gdk_seat_get_keyboard(gdkseat));
    wlserv.keyboard = gdk_wayland_device_get_wl_keyboard(gdk_seat_get_keyboard(gdkseat));
    wlserv.pointer = gdk_wayland_device_get_wl_pointer(gdk_seat_get_pointer(gdkseat));
  }
#endif

  ui_display_init_wlserv(&wlserv);

  GTK_WIDGET_CLASS(vclass)->map = vte_terminal_map;
  GTK_WIDGET_CLASS(vclass)->unmap = vte_terminal_unmap;
}

#if _VTE_GTK >= 4
/*
 * keyboard_key() receives keys without this on wayland, but menus on the titlebar
 * also use them for shortcut keys.
 * (e.g. Up key focuses "New Tab" menu in kgx.)
 */
static gboolean evctl_key_pressed(GtkEventControllerKey *controller,
                                  guint keyval, guint keycode, GdkModifierType state,
                                  gpointer user_data) {}

static void evctl_enter(GtkEventControllerFocus *controller, gpointer user_data) {
  VteTerminal *terminal = user_data;

  if (GTK_WIDGET_MAPPED(GTK_WIDGET(terminal))) {
    ui_window_set_input_focus(&PVT(terminal)->screen->window);
  } else {
    PVT(terminal)->screen->window.is_focused = 1;
  }
}

static void evctl_leave(GtkEventControllerFocus *controller, gpointer user_data) {
  VteTerminal *terminal = user_data;

  if (GTK_WIDGET_MAPPED(GTK_WIDGET(terminal))) {
    XEvent xev;

    xev.type = FocusOut;
    ui_window_receive_event(&PVT(terminal)->screen->window, (XEvent*)&xev);
  } else {
    PVT(terminal)->screen->window.is_focused = 0;
  }
}
#endif

/* --- global functions --- */

void focus_gtk_window(ui_window_t *win, uint32_t time) {
  VteTerminal *terminal = VTE_WIDGET((ui_screen_t*)win);

  if (!gtk_widget_has_focus(GTK_WIDGET(terminal))) {
#if _VTE_GTK == 3
    gdk_window_focus(gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(terminal))),
                     time /* gtk_window_focus() does nothing if GDK_CURRENT_TIME */);
#else
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(terminal))),
                         GTK_WIDGET(terminal));
    gtk_widget_grab_focus(GTK_WIDGET(terminal));
#endif
  }
}
