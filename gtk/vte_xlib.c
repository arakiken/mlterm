/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <X11/keysym.h>
#include <xlib/ui_xim.h> /* ui_xim_display_opened */
#if _VTE_GTK == 3
#include <gdk/gdkx.h>
#else
#include <gdk/x11/gdkx.h>
#endif
#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
#include <X11/extensions/XInput2.h>
#endif

#if GTK_CHECK_VERSION(2, 90, 0)
#if _VTE_GTK == 3
#define gdk_x11_drawable_get_xid(window) gdk_x11_window_get_xid(window)
#else
#define gdk_x11_drawable_get_xid(d) gdk_x11_surface_get_xid(d)
/* #define GdkDeviceManager GdkX11DeviceManagerXI2 */
#endif
#endif

#if 0
/* Forcibly enable transparency on gnome-terminal which doesn't support it. */
#define FORCE_TRANSPARENCY
#endif

/* --- static variables --- */

#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
static int is_xinput2;
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

#if _VTE_GTK >= 4
static void vte_terminal_size_allocate(GtkWidget *widget, int width, int height, int baseline);
#else
static void vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
#endif

#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
static u_int xi2_get_state (XIModifierState *mods_state,
                            XIButtonState   *buttons_state,
                            XIGroupState    *group_state) {
  u_int state = 0;

  if (mods_state) {
    state = mods_state->effective;
  }

  if (buttons_state) {
    int count;

    /*
     * 5 is always less than buttons_state->mask_len * 8,
     * so skip to check if 5 <= buttons_state->mask_len * 8.
     */
    for (count = 1; count <= 5; count++) {
      if (XIMaskIsSet (buttons_state->mask, count)) {
        state |= (Button1Mask << (count - 1));
      }
    }
  }

  if (group_state) {
    state |= (group_state->effective) << 13;
  }

  return state;
}

static int xievent_to_xevent(XIDeviceEvent *xiev, XEvent *xev) {
  switch (xiev->evtype) {
  case XI_KeyPress:
    xev->xkey.type = KeyPress;
    xev->xkey.window = xiev->event;
    xev->xkey.root = xiev->root;
    xev->xkey.subwindow = xiev->child;
    xev->xkey.time = xiev->time;
    xev->xkey.x = xiev->event_x;
    xev->xkey.y = xiev->event_y;
    xev->xkey.x_root = xiev->root_x;
    xev->xkey.y_root = xiev->root_y;
    xev->xkey.state = xi2_get_state(&xiev->mods, &xiev->buttons, &xiev->group);
    xev->xkey.keycode = xiev->detail;
    xev->xkey.same_screen = True;
    break;

  case XI_ButtonPress:
    xev->xbutton.type = ButtonPress;
    xev->xbutton.window = xiev->event;
    xev->xbutton.root = xiev->root;
    xev->xbutton.subwindow = xiev->child;
    xev->xbutton.time = xiev->time;
    xev->xbutton.x = xiev->event_x;
    xev->xbutton.y = xiev->event_y;
    xev->xbutton.x_root = xiev->root_x;
    xev->xbutton.y_root = xiev->root_y;
    xev->xbutton.state = xi2_get_state(&xiev->mods, &xiev->buttons, &xiev->group);
    xev->xbutton.button = xiev->detail;
    xev->xbutton.same_screen = True;
    break;

  case XI_ButtonRelease:
    xev->xbutton.type = ButtonRelease;
    xev->xbutton.window = xiev->event;
    xev->xbutton.root = xiev->root;
    xev->xbutton.subwindow = xiev->child;
    xev->xbutton.time = xiev->time;
    xev->xbutton.x = xiev->event_x;
    xev->xbutton.y = xiev->event_y;
    xev->xbutton.x_root = xiev->root_x;
    xev->xbutton.y_root = xiev->root_y;
    xev->xbutton.state = xi2_get_state(&xiev->mods, &xiev->buttons, &xiev->group);
    xev->xbutton.button = xiev->detail;
    xev->xbutton.same_screen = True;
    break;

  case XI_Motion:
    xev->xmotion.type = MotionNotify;
    xev->xmotion.window = xiev->event;
    xev->xmotion.root = xiev->root;
    xev->xmotion.subwindow = xiev->child;
    xev->xmotion.time = xiev->time;
    xev->xmotion.x = xiev->event_x;
    xev->xmotion.y = xiev->event_y;
    xev->xmotion.x_root = xiev->root_x;
    xev->xmotion.y_root = xiev->root_y;
    xev->xmotion.state = xi2_get_state(&xiev->mods, &xiev->buttons, &xiev->group);
    xev->xmotion.is_hint = 0;
    xev->xmotion.same_screen = True;
    break;

#if 0
  case XI_FocusIn:
    xev->xfocus.type = FocusIn;
    xev->xfocus.window = ((XIEnterEvent*)xiev)->event;
    xev->xfocus.mode = ((XIEnterEvent*)xiev)->mode;
    xev->xfocus.detail = ((XIEnterEvent*)xiev)->detail;
    break;

  case XI_FocusOut:
    xev->xfocus.type = FocusOut;
    xev->xfocus.window = ((XIEnterEvent*)xiev)->event;
    xev->xfocus.mode = ((XIEnterEvent*)xiev)->mode;
    xev->xfocus.detail = ((XIEnterEvent*)xiev)->detail;
    break;
#endif

  default:
    return 0;
  }

#if 0
  bl_debug_printf("%s devid %d srcid %d window %d child %d: "
                  "rootx %0.f rooty %0.f x %0.f y %0.f detail %d serial %d\n",
                  xiev->evtype == XI_KeyPress ? "KeyPress" :
                  (xiev->evtype == XI_ButtonPress ? "ButtonPress" :
                   (xiev->evtype == XI_ButtonRelease ? "ButtonRelease" :
                    (xiev->evtype == XI_Motion ? "Motion" : "Unknown"))),
                  xiev->deviceid, xiev->sourceid, xiev->event, xiev->child,
                  xiev->root_x, xiev->root_y, xiev->event_x, xiev->event_y,
                  xiev->detail, xev->xany.serial);
#endif

  return 1;
}
#endif

#if _VTE_GTK == 3
/*
 * Don't call vt_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because vt_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn vte_terminal_filter(GdkXEvent *xevent, GdkEvent *event, /* GDK_NOTHING */
                                           gpointer data) {
  u_int count;
  int is_key_event;
#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
  XEvent new_xev;
  XGenericEventCookie *cookie;
#endif

  if (XFilterEvent((XEvent *)xevent, None)) {
    return GDK_FILTER_REMOVE;
  }

#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
  if (((XEvent *)xevent)->type == GenericEvent) {
#if 0
    static int xi_opcode;

    if (xi_opcode == 0) {
      int xi_error;
      int xi_event;

      if(!XQueryExtension(disp.display, "XInputExtension", &xi_opcode, &xi_event, &xi_error)) {
        return GDK_FILTER_CONTINUE;
      }
    }
#endif

    cookie = &((XEvent*)xevent)->xcookie;

#if 0
    if (cookie->extension != xi_opcode) {
      return GDK_FILTER_CONTINUE;
    }
#endif

    memcpy(&new_xev, xevent, sizeof(XGenericEventCookie));
    if (!xievent_to_xevent(cookie->data, &new_xev)) {
      return GDK_FILTER_CONTINUE;
    }
    xevent = &new_xev;
  }
#endif

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
      if (is_key_event) {
        if (((XEvent *)xevent)->xany.window == disp.roots[count]->my_window) {
          if (IS_VTE_SEARCH(terminal)) {
            /* reset searching state */
            vt_term_search_reset_position(PVT(terminal)->term);
          }

          if (!disp.roots[count]->is_focused) {
#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
            if (xevent == &new_xev) {
              ((XIDeviceEvent*)cookie->data)->event =
                gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));
            } else
#endif
            {
              ((XEvent *)xevent)->xany.window =
                gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));
            }

            return GDK_FILTER_CONTINUE;
          }
        }
      } else if (PVT(terminal)->screen->window.is_transparent &&
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
      if (!vt_term_is_zombie(PVT(terminal)->term) &&
          config_menu_pid != PVT(terminal)->term->pty->config_menu.pid) {
        if ((config_menu_pid = PVT(terminal)->term->pty->config_menu.pid)) {
          vte_reaper_add_child(config_menu_pid);
        }
      }

      if (is_key_event || ((XEvent *)xevent)->type == ButtonPress ||
          ((XEvent *)xevent)->type == ButtonRelease) {
        /* Hook key and button events for popup menu. */
#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
        if (xevent == &new_xev) {
          ((XIDeviceEvent*)cookie->data)->event =
            gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));
        } else
#endif
        {
          ((XEvent *)xevent)->xany.window =
            gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));
        }

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

#if _VTE_GTK >= 4
        vte_terminal_size_allocate(GTK_WIDGET(terminal), alloc.width, alloc.height, 0);
#else
        vte_terminal_size_allocate(GTK_WIDGET(terminal), &alloc);
#endif
      }

      return GDK_FILTER_REMOVE;
    }
  }

  return GDK_FILTER_CONTINUE;
}
#endif

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

#if _VTE_GTK == 3
  g_signal_connect_swapped(toplevel, "configure-event",
                           G_CALLBACK(toplevel_configure), VTE_TERMINAL(widget));
#endif

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

#if _VTE_GTK >= 4
static gboolean evctl_key_pressed(GtkEventControllerKey *controller,
                                  guint keyval, guint keycode, GdkModifierType state,
                                  gpointer user_data)
{
  VteTerminal *terminal = user_data;
  XKeyEvent xev;

  xev.type = KeyPress;
  xev.serial = 0;
  xev.send_event = False;
  xev.display = PVT(terminal)->screen->window.disp->display;
  xev.window = PVT(terminal)->screen->window.my_window;
  xev.root = DefaultRootWindow(xev.display);
  xev.subwindow = None;
  xev.time = CurrentTime;
  xev.x = xev.y = xev.x_root = xev.y_root = 0;
  xev.state = state;
  xev.keycode = keycode;
  xev.same_screen = True;

  ui_window_receive_event(&PVT(terminal)->screen->window, (XEvent*)&xev);

  return GDK_EVENT_STOP;
}

static void evctl_enter(GtkEventControllerFocus *controller, gpointer user_data) {
  VteTerminal *terminal = user_data;

  if (GTK_WIDGET_MAPPED(GTK_WIDGET(terminal))) {
    XFocusChangeEvent xev;

    xev.type = FocusIn;
    xev.serial = 0;
    xev.send_event = False;
    xev.display = PVT(terminal)->screen->window.disp->display;
    xev.window = PVT(terminal)->screen->window.my_window;
    xev.mode = NotifyNormal;
    xev.detail = NotifyDetailNone;

    ui_window_receive_event(&PVT(terminal)->screen->window, (XEvent*)&xev);
  } else {
    PVT(terminal)->screen->window.is_focused = 1;
  }
}

static void evctl_leave(GtkEventControllerFocus *controller, gpointer user_data) {
  VteTerminal *terminal = user_data;

  if (GTK_WIDGET_MAPPED(GTK_WIDGET(terminal))) {
    XFocusChangeEvent xev;

    xev.type = FocusOut;
    xev.serial = 0;
    xev.send_event = False;
    xev.display = PVT(terminal)->screen->window.disp->display;
    xev.window = PVT(terminal)->screen->window.my_window;
    xev.mode = NotifyNormal;
    xev.detail = NotifyDetailNone;

    ui_window_receive_event(&PVT(terminal)->screen->window, (XEvent*)&xev);
  } else {
    PVT(terminal)->screen->window.is_focused = 0;
  }
}

static gboolean receive_xevent(GdkDisplay *display, const XEvent *xevent, gpointer user_data) {
  VteTerminal *terminal = user_data;

  if (GTK_WIDGET_MAPPED(GTK_WIDGET(terminal))) {
    if (PVT(terminal)->screen->window.my_window == xevent->xany.window) {
      XEvent xev = *xevent;

      if (xev.type == FocusOut || xev.type == FocusIn) {
        /*
         * If pressing mlterm window, evctl_enter() is called, but FocusOut is also
         * received here. So forcibly ignore FocusOut.
         */
        return TRUE;
      }

#if 0
      if (xev.type == ButtonPress || xev.type == ButtonRelease ||
          xev.type == MotionNotify) {
        xev.xany.window = gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(terminal)));
        xev.xbutton.time = CurrentTime;

        XSendEvent(xev.xany.display, xev.xany.window, False,
                   ButtonPressMask|ButtonReleaseMask|ButtonMotionMask|PointerMotionMask, &xev);
      }
#endif

      if (ui_window_receive_event(&PVT(terminal)->screen->window, &xev)) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

gboolean stop_receive_xevent(GtkWidget *widget, gpointer user_data) {
  g_signal_handlers_disconnect_by_func(user_data, receive_xevent, widget);

  return FALSE;
}
#endif

static void show_root(ui_display_t *disp, GtkWidget *widget) {
  VteTerminal *terminal = VTE_TERMINAL(widget);
  XID xid = gdk_x11_drawable_get_xid(gtk_widget_get_window(widget));
#if _VTE_GTK >= 4
  GdkDisplay *display = gdk_surface_get_display(gtk_widget_get_window(widget));

  g_signal_connect(display, "xevent", G_CALLBACK(receive_xevent), widget);
  g_signal_connect(widget, "destroy", G_CALLBACK(stop_receive_xevent), display);
#endif

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

#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
  if (is_xinput2) {
    ui_window_remove_event_mask(&PVT(terminal)->screen->window,
                                ButtonPressMask | ButtonMotionMask | ButtonReleaseMask |
                                KeyPressMask);
  }
#endif

  ui_display_show_root(disp, &PVT(terminal)->screen->window, 0, 0,
                       HINT_CHILD_WINDOW_ATTR, "mlterm", NULL, xid);
#if 0
  PVT(terminal)->screen->window.event_mask |=
    (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
     PointerMotionMask | ExposureMask | FocusChangeMask);
  XSelectInput(PVT(terminal)->screen->window.disp->display,
               PVT(terminal)->screen->window.my_window,
               PVT(terminal)->screen->window.event_mask);
#endif

#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
  if (is_xinput2) {
    XIEventMask mask;

    /* If XIAllDevices is set and 'a' key is pressed, 'aa' can be output. */
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = calloc(mask.mask_len, 1);
    XISetMask(mask.mask, XI_Motion);
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISetMask(mask.mask, XI_KeyPress);
#if 0
    XISetMask(mask.mask, XI_KeyRelease);
    XISetMask(mask.mask, XI_FocusIn);
    XISetMask(mask.mask, XI_FocusOut);
#endif
    XISelectEvents(disp->display, PVT(terminal)->screen->window.my_window, &mask, 1);
    XSync(disp->display, False);
    free(mask.mask);
  }
#endif
}

#if _VTE_GTK >= 4
static void vte_terminal_map(GtkWidget *widget) {
  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->map)(widget);

  ui_window_map(&PVT(VTE_TERMINAL(widget))->screen->window);
}

static void vte_terminal_unmap(GtkWidget *widget) {
  ui_window_unmap(&PVT(VTE_TERMINAL(widget))->screen->window);

  (*GTK_WIDGET_CLASS(vte_terminal_parent_class)->unmap)(widget);
}
#endif

static void init_display(ui_display_t *disp, VteTerminalClass *vclass) {
  GdkDisplay *gdkdisp = gdk_display_get_default();
  const char *name = gdk_display_get_name(gdkdisp);
#if GTK_CHECK_VERSION(2, 90, 0) && defined(GDK_TYPE_X11_DEVICE_MANAGER_XI2)
#if _VTE_GTK == 3
  GdkDeviceManager *devman = gdk_display_get_device_manager(gdkdisp);

  if (G_OBJECT_TYPE(devman) == GDK_TYPE_X11_DEVICE_MANAGER_XI2) {
    is_xinput2 = 1;
  }
#endif
#endif

  /* gtk+-2.0 doesn't support wayland */
#if GTK_CHECK_VERSION(2, 90, 0)
  if (name && strstr(name, "wayland")) {
#if _VTE_GTK == 3
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s display is not supported on xlib", name);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
#else
    bl_error_printf("%s display is not supported on xlib.\n", name);
#endif
    exit(1);
  }
#endif

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

#if _VTE_GTK >= 4
  GTK_WIDGET_CLASS(vclass)->map = vte_terminal_map;
  GTK_WIDGET_CLASS(vclass)->unmap = vte_terminal_unmap;
#endif

#if _VTE_GTK == 3
  gdk_window_add_filter(NULL, vte_terminal_filter, NULL);
#endif
}
