/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <gdk/gdkwayland.h>
#include <locale.h>

void ui_display_init_wlserv(ui_wlserv_t *wlserv);
void ui_display_map(ui_display_t *disp);
int ui_display_resize(ui_display_t *disp, u_int width, u_int height);

static void (*orig_window_unfocused)(ui_window_t *win);

/* --- static functions --- */

void focus_gtk_window(ui_window_t *win) {
  VteTerminal *terminal = VTE_WIDGET((ui_screen_t*)win);
  GtkWidget *widget = gtk_widget_get_toplevel(GTK_WIDGET(terminal));

  if (!gtk_widget_has_focus(GTK_WIDGET(terminal))) {
#ifdef __DEBUG
    bl_debug_printf("Forcibly focus gdk window.\n");
#endif
    gdk_window_focus(gtk_widget_get_window(widget),
                     /* gdk_window_focus() does nothing if GDK_CURRENT_TIME */
                     win->disp->display->wlserv->time);
  }
}

static void window_unfocused(ui_window_t *win) {
  VteTerminal *terminal = VTE_WIDGET((ui_screen_t*)win);

  if (!gtk_widget_has_focus(GTK_WIDGET(terminal))) {
    (*orig_window_unfocused)(win);
  }
}

static void show_root(ui_display_t *disp, GtkWidget *widget) {
  GdkWindow *window = gtk_widget_get_window(widget);
  ui_window_t *win = &PVT(VTE_TERMINAL(widget))->screen->window;
  char *class;

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
  ui_display_show_root(disp, win, 0, 0, 0, class, gdk_wayland_window_get_wl_surface(window));

  orig_window_unfocused = win->window_unfocused;
  win->window_unfocused = window_unfocused;
}

static gboolean toplevel_configure(gpointer data) {
  VteTerminal *terminal = data;
  GtkAllocation alloc;

  gtk_widget_get_allocation(GTK_WIDGET(terminal), &alloc);
  /* Multiple displays can coexist on wayland, so '&disp' isn't used. */
  ui_display_move(PVT(terminal)->screen->window.disp, alloc.x, alloc.y);

  return FALSE;
}

static void vte_terminal_map(GtkWidget *widget) {
  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->map)(widget);

#ifdef __DEBUG
  bl_debug_printf("mapped.\n");
#endif

  /* Multiple displays can coexist on wayland, so '&disp' isn't used. */
  ui_display_map(PVT(VTE_TERMINAL(widget))->screen->window.disp);
}

static void vte_terminal_unmap(GtkWidget *widget) {
  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->unmap)(widget);

  /* Multiple displays can coexist on wayland, so '&disp' isn't used. */
  ui_display_unmap(PVT(VTE_TERMINAL(widget))->screen->window.disp);
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
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s is not supported on wayland", disp->name);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
