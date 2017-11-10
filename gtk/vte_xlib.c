/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <X11/keysym.h>
#include <xlib/ui_xim.h> /* ui_xim_display_opened */
#include <gdk/gdkx.h>

#if GTK_CHECK_VERSION(2, 90, 0)
#define gdk_x11_drawable_get_xid(window) gdk_x11_window_get_xid(window)
#endif

#if 0
/* Forcibly enable transparency on gnome-terminal which doesn't support it. */
#define FORCE_TRANSPARENCY
#endif

/* --- static functions --- */

static gboolean toplevel_configure(gpointer data) {
  VteTerminal *terminal = data;

  if (PVT(terminal)->screen->window.is_transparent) {
    XEvent ev;

    if (!XCheckTypedWindowEvent(disp.display, gdk_x11_drawable_get_xid(gtk_widget_get_window(
                                                  gtk_widget_get_toplevel(GTK_WIDGET(terminal)))),
                                ConfigureNotify, &ev)) {
      ui_window_set_transparent(&PVT(terminal)->screen->window,
                                ui_screen_get_picture_modifier(PVT(terminal)->screen));
    } else {
      XPutBackEvent(disp.display, &ev);
    }
  }

  return FALSE;
}

static void vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation);

/*
 * Don't call vt_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because vt_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn vte_terminal_filter(GdkXEvent *xevent, GdkEvent *event, /* GDK_NOTHING */
                                           gpointer data) {
  u_int count;
  int is_key_event;

  if (XFilterEvent((XEvent *)xevent, None)) {
    return GDK_FILTER_REMOVE;
  }

  if ((((XEvent *)xevent)->type == KeyPress || ((XEvent *)xevent)->type == KeyRelease)) {
    is_key_event = 1;
  } else {
    is_key_event = 0;
  }

  for (count = 0; count < disp.num_roots; count++) {
    VteTerminal *terminal;

    if (IS_MLTERM_SCREEN(disp.roots[count])) {
      terminal = VTE_WIDGET((ui_screen_t *)disp.roots[count]);

      if (!PVT(terminal)->term) {
        /* pty is already closed and new pty is not attached yet. */
        continue;
      }

      /*
       * Key events are ignored if window isn't focused.
       * This processing is added for key binding of popup menu.
       */
      if (is_key_event && ((XEvent *)xevent)->xany.window == disp.roots[count]->my_window) {
        vt_term_search_reset_position(PVT(terminal)->term);

        if (!disp.roots[count]->is_focused) {
          ((XEvent *)xevent)->xany.window =
              gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));

          return GDK_FILTER_CONTINUE;
        }
      }

      if (PVT(terminal)->screen->window.is_transparent &&
          ((XEvent *)xevent)->type == ConfigureNotify &&
          ((XEvent *)xevent)->xconfigure.event ==
              gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)))) {
        /*
         * If terminal position is changed by adding menu bar or tab,
         * transparent background is reset.
         */

        gint x;
        gint y;

        gdk_window_get_position(gtk_widget_get_window(GTK_WIDGET(terminal)), &x, &y);

        /*
         * XXX
         * I don't know why but the height of menu bar has been already
         * added to the position of terminal before first
         * GdkConfigureEvent whose x and y is 0 is received.
         * But (x != xconfigure.x || y != xconfigure.y) is true eventually
         * and ui_window_set_transparent() is called expectedly.
         */
        if (x != ((XEvent *)xevent)->xconfigure.x || y != ((XEvent *)xevent)->xconfigure.y) {
          ui_window_set_transparent(&PVT(terminal)->screen->window,
                                    ui_screen_get_picture_modifier(PVT(terminal)->screen));
        }

        return GDK_FILTER_CONTINUE;
      }
    } else {
      terminal = NULL;
    }

    if (ui_window_receive_event(disp.roots[count], (XEvent *)xevent)) {
      static pid_t config_menu_pid = 0;

      if (!terminal || /* SCIM etc window */
          /* XFilterEvent in ui_window_receive_event. */
          ((XEvent *)xevent)->xany.window != disp.roots[count]->my_window) {
        return GDK_FILTER_REMOVE;
      }

      /* XXX Hack for waiting for config menu program exiting. */
      if (PVT(terminal)->term->pty &&
          config_menu_pid != PVT(terminal)->term->pty->config_menu.pid) {
        if ((config_menu_pid = PVT(terminal)->term->pty->config_menu.pid)) {
          vte_reaper_add_child(config_menu_pid);
        }
      }

      if (is_key_event || ((XEvent *)xevent)->type == ButtonPress ||
          ((XEvent *)xevent)->type == ButtonRelease) {
        /* Hook key and button events for popup menu. */
        ((XEvent *)xevent)->xany.window =
            gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));

        return GDK_FILTER_CONTINUE;
      } else {
        return GDK_FILTER_REMOVE;
      }
    }
    /*
     * xconfigure.window:  window whose size, position, border, and/or stacking
     *                     order was changed.
     *                      => processed in following.
     * xconfigure.event:   reconfigured window or to its parent.
     * (=XAnyEvent.window)  => processed in ui_window_receive_event()
     */
    else if (/* terminal && */ ((XEvent *)xevent)->type == ConfigureNotify &&
             ((XEvent *)xevent)->xconfigure.window == disp.roots[count]->my_window) {
#if 0
      /*
       * This check causes resize problem in opening tab in
       * gnome-terminal(2.29.6).
       */
      if (((XEvent *)xevent)->xconfigure.width != GTK_WIDGET(terminal)->allocation.width ||
          ((XEvent *)xevent)->xconfigure.height != GTK_WIDGET(terminal)->allocation.height)
#else
      if (CHAR_WIDTH(terminal) != ui_col_width(PVT(terminal)->screen) ||
          CHAR_HEIGHT(terminal) != ui_line_height(PVT(terminal)->screen))
#endif
      {
        /* Window was changed due to change of font size inside mlterm. */
        GtkAllocation alloc;

        gtk_widget_get_allocation(GTK_WIDGET(terminal), &alloc);
        alloc.width = ((XEvent *)xevent)->xconfigure.width;
        alloc.height = ((XEvent *)xevent)->xconfigure.height;

#ifdef __DEBUG
        bl_debug_printf(BL_DEBUG_TAG " child is resized\n");
#endif

        vte_terminal_size_allocate(GTK_WIDGET(terminal), &alloc);
      }

      return GDK_FILTER_REMOVE;
    }
  }

  return GDK_FILTER_CONTINUE;
}

#ifdef FORCE_TRANSPARENCY
#if VTE_CHECK_VERSION(0, 38, 0)
static void set_rgba_visual(GtkWidget *widget) {
  GdkScreen *screen;

  if ((screen = gtk_widget_get_screen(widget)) && gdk_screen_is_composited(screen)) {
    gtk_widget_set_visual(widget, gdk_screen_get_rgba_visual(screen));
  }
}

static gboolean toplevel_draw(GtkWidget *widget, cairo_t *cr, void *user_data) {
  GtkWidget *child = user_data;

  gint width = gtk_widget_get_allocated_width(child);
  gint height = gtk_widget_get_allocated_height(child);
  gint x;
  gint y;

  gtk_widget_translate_coordinates(child, widget, 0, 0, &x, &x);
  cairo_rectangle(cr, x, y, width, height);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
  cairo_fill(cr);

  return FALSE;
}
#endif
#endif

static void vte_terminal_hierarchy_changed(GtkWidget *widget, GtkWidget *old_toplevel,
                                           gpointer data) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);

  if (old_toplevel) {
    g_signal_handlers_disconnect_by_func(old_toplevel, toplevel_configure, widget);
  }

  g_signal_connect_swapped(toplevel, "configure-event",
                           G_CALLBACK(toplevel_configure), VTE_TERMINAL(widget));

#ifdef FORCE_TRANSPARENCY
#if VTE_CHECK_VERSION(0, 38, 0)
  /*
   * Though vte 0.38.0 or later doesn't support rgba visual,
   * this forcibly enables it.
   */
  set_rgba_visual(toplevel);

  /* roxterm calls gtk_widget_set_app_paintable(TRUE) internally. */
  if (!gtk_widget_get_app_paintable(toplevel)) {
    /*
     * XXX
     * gtk_widget_set_app_paintable(TRUE) enables transparency, but unexpectedly
     * makes a menu bar of gnome-terminal(3.24.2) transparent, sigh...
     */
    gtk_widget_set_app_paintable(toplevel, TRUE);
    g_signal_connect(toplevel, "draw", G_CALLBACK(toplevel_draw), widget);

    if (old_toplevel) {
      g_signal_handlers_disconnect_by_func(old_toplevel, toplevel_draw, widget);
    }
  }
#endif
#endif
}

static void show_root(ui_display_t *disp, GtkWidget *widget) {
  VteTerminal *terminal = VTE_TERMINAL(widget);
  XID xid = gdk_x11_drawable_get_xid(gtk_widget_get_window(widget));

  if (disp->gc->gc == DefaultGC(disp->display, disp->screen)) {
    /*
     * Replace visual, colormap, depth and gc with those inherited from parent
     * xid.
     * In some cases that those of parent xid is not DefaultVisual,
     * DefaultColormap
     * and so on (e.g. compiz), BadMatch error can happen.
     */

    XWindowAttributes attr;
    XGCValues gc_value;
    int depth_is_changed;

    XGetWindowAttributes(disp->display, xid, &attr);
    disp->visual = attr.visual;
    disp->colormap = attr.colormap;
    depth_is_changed = (disp->depth != attr.depth);
    disp->depth = attr.depth;

    /* ui_gc_t using DefaultGC is already created in vte_terminal_class_init */
    gc_value.foreground = disp->gc->fg_color;
    gc_value.background = disp->gc->bg_color;
    gc_value.graphics_exposures = True;
    disp->gc->gc =
        XCreateGC(disp->display, xid, GCForeground | GCBackground | GCGraphicsExposures, &gc_value);

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Visual %x Colormap %x Depth %d\n", disp->visual, disp->colormap,
                    disp->depth);
#endif

    if (depth_is_changed &&
        /* see ui_screen_new() */
        !PVT(terminal)->screen->window.is_transparent &&
        !PVT(terminal)->screen->pic_file_path) {
      ui_change_true_transbg_alpha(PVT(terminal)->screen->color_man, main_config.alpha);
      ui_color_manager_reload(PVT(terminal)->screen->color_man);

/* No colors are cached for now. */
#if 0
      ui_color_cache_unload_all();
#endif
    }
  }

  ui_display_show_root(disp, &PVT(terminal)->screen->window, 0, 0, 0, "mlterm", xid);
}

static void init_display(ui_display_t *disp, VteTerminalClass *vclass) {
  GdkDisplay *gdkdisp = gdk_display_get_default();
  const char *name = gdk_display_get_name(gdkdisp);

  if (name && strstr(name, "wayland")) {
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s display is not supported on xlib", name);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    exit(1);
  }

  disp->display = gdk_x11_display_get_xdisplay(gdkdisp);
  disp->screen = DefaultScreen(disp->display);
  disp->my_window = DefaultRootWindow(disp->display);
  disp->visual = DefaultVisual(disp->display, disp->screen);
  disp->colormap = DefaultColormap(disp->display, disp->screen);
  disp->depth = DefaultDepth(disp->display, disp->screen);
  disp->gc = ui_gc_new(disp->display, None);
  disp->width = DisplayWidth(disp->display, disp->screen);
  disp->height = DisplayHeight(disp->display, disp->screen);
  disp->modmap.serial = 0;
  disp->modmap.map = XGetModifierMapping(disp->display);

  ui_xim_display_opened(disp->display);

  gdk_window_add_filter(NULL, vte_terminal_filter, NULL);
}
