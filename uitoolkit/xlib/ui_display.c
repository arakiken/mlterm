/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_display.h"

#include <string.h> /* memset/memcpy */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>  /* strdup */
#include <pobl/bl_file.h> /* bl_file_set_cloexec */
#include <pobl/bl_dialog.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

#include "../ui_window.h"
#include "../ui_picture.h"
#include "../ui_imagelib.h"

#include "ui_xim.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static u_int num_displays;
static ui_display_t **displays;
static int (*default_error_handler)(Display *, XErrorEvent *);

/* --- static functions --- */

#ifdef __DEBUG

static int error_handler(Display *display, XErrorEvent *event) {
  char buffer[1024];

  XGetErrorText(display, event->error_code, buffer, 1024);

  bl_msg_printf("%s\n", buffer);

  abort();

  return 1;
}

static int ioerror_handler(Display *display) {
  bl_error_printf("X IO Error.\n");
  abort();

  return 1;
}

#else /* __DEBUG */

static int error_handler(Display *display, XErrorEvent *event) {
  /*
   * If <X11/Xproto.h> is included for 'X_OpenFont', typedef of BOOL and INT32
   * is conflicted in <X11/Xmd.h>(included from <X11/Xproto.h> and
   * <w32api/basetsd.h>(included from <windows.h>).
   */
  if (event->error_code == 2 /* BadValue */
      && event->request_code == 45 /* X_OpenFont */) {
    /*
     * XXX Hack
     * If BadValue error happens in XLoad(Query)Font function,
     * mlterm doesn't stop.
     */

    bl_msg_printf("XLoad(Query)Font failed.\n");

    /* ignored anyway */
    return 0;
  } else if (default_error_handler) {
    return (*default_error_handler)(display, event);
  } else {
    exit(1);

    return 1;
  }
}

#endif

#ifdef USE_ALERT_DIALOG
#define LINESPACE 20
#define BEGENDSPACE 20
static int dialog_cb(bl_dialog_style_t style, const char *msg) {
  Display *display;
  int screen;
  Window window;
  GC gc;
  XFontStruct *font;
  u_int width;
  u_int height;
  u_int len;
  const char title[] = "** Warning **";

  if (style != BL_DIALOG_ALERT) {
    return -1;
  }

  len = strlen(msg);

  if (!(display = XOpenDisplay(""))) {
    return -1;
  }

  screen = DefaultScreen(display);
  gc = DefaultGC(display, screen);

  if (!(font = XLoadQueryFont(display, "-*-r-normal-*-*-*-*-*-c-*-iso8859-1"))) {
    XCloseDisplay(display);

    return -1;
  }

  XSetFont(display, gc, font->fid);

  width = font->max_bounds.width * BL_MAX(len, sizeof(title) - 1) + BEGENDSPACE;
  height = (font->ascent + font->descent) * 2 + LINESPACE;

  if (!(window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                     (DisplayWidth(display, screen) - width) / 2,
                                     (DisplayHeight(display, screen) - height) / 2,
                                     width, height, 0, BlackPixel(display, screen),
                                     WhitePixel(display, screen)))) {
    XFreeFont(display, font);
    XCloseDisplay(display);

    return -1;
  }

  /*
   * Not only ButtonReleaseMask but also ButtonPressMask is necessary to
   * receive ButtonRelease event correctly.
   */
  XSelectInput(display, window,
               KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|ExposureMask|StructureNotifyMask);
  XMapWindow(display, window);

  while (1) {
    XEvent ev;

    XWindowEvent(display, window,
                 KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|ExposureMask|StructureNotifyMask,
                 &ev);

    if (ev.type == KeyRelease || ev.type == ButtonRelease || ev.type == ClientMessage) {
      break;
    } else if (ev.type == Expose) {
      XDrawString(display, window, gc, BEGENDSPACE / 2, font->ascent + LINESPACE / 2,
                  title, sizeof(title) - 1);
      XDrawString(display, window, gc, BEGENDSPACE / 2,
                  font->ascent * 2 + font->descent + LINESPACE / 2, msg, len);
    } else if (ev.type == MapNotify) {
      XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
    }
  }

  XDestroyWindow(display, window);
  XFreeFont(display, font);
  XCloseDisplay(display);

  return 1;
}
#endif

static ui_display_t *open_display(char *name, u_int depth) {
  ui_display_t *disp;
  XVisualInfo vinfo;

  if ((disp = calloc(1, sizeof(ui_display_t))) == NULL) {
    return NULL;
  }

  if ((disp->display = XOpenDisplay(name)) == NULL) {
    bl_error_printf("Couldn't open display %s.\n", name);

    goto error1;
  }

  /* set close-on-exec flag on the socket connected to X. */
  bl_file_set_cloexec(XConnectionNumber(disp->display));

  disp->name = DisplayString(disp->display);
  disp->screen = DefaultScreen(disp->display);
  disp->my_window = DefaultRootWindow(disp->display);
  disp->width = DisplayWidth(disp->display, disp->screen);
  disp->height = DisplayHeight(disp->display, disp->screen);

  if (depth && XMatchVisualInfo(disp->display, disp->screen, depth, TrueColor, &vinfo) &&
      vinfo.visual != DefaultVisual(disp->display, disp->screen)) {
    XSetWindowAttributes s_attr;
    Window win;

    disp->depth = depth;
    disp->visual = vinfo.visual;
    disp->colormap = XCreateColormap(disp->display, disp->my_window, vinfo.visual, AllocNone);

    s_attr.background_pixel = BlackPixel(disp->display, disp->screen);
    s_attr.border_pixel = BlackPixel(disp->display, disp->screen);
    s_attr.colormap = disp->colormap;
    win = XCreateWindow(disp->display, disp->my_window, 0, 0, 1, 1, 0, disp->depth, InputOutput,
                        disp->visual, CWColormap | CWBackPixel | CWBorderPixel, &s_attr);
    if ((disp->gc = ui_gc_new(disp->display, win)) == NULL) {
      goto error2;
    }
    XDestroyWindow(disp->display, win);
  } else {
    disp->depth = DefaultDepth(disp->display, disp->screen);
    disp->visual = DefaultVisual(disp->display, disp->screen);
    disp->colormap = DefaultColormap(disp->display, disp->screen);
    if ((disp->gc = ui_gc_new(disp->display, None)) == NULL) {
      goto error2;
    }
  }

  disp->modmap.map = XGetModifierMapping(disp->display);

  default_error_handler = XSetErrorHandler(error_handler);
#ifdef __DEBUG
  XSetIOErrorHandler(ioerror_handler);
  XSynchronize(disp->display, True);
#endif

  ui_xim_display_opened(disp->display);
  ui_picture_display_opened(disp->display);

#ifdef DEBUG
  bl_debug_printf("X connection opened.\n");
#endif

  return disp;

error2:
  XCloseDisplay(disp->display);

error1:
  free(disp);

  return NULL;
}

static void close_display(ui_display_t *disp) {
  u_int count;

  ui_gc_destroy(disp->gc);

  if (disp->modmap.map) {
    XFreeModifiermap(disp->modmap.map);
  }

  for (count = 0; count < BL_ARRAY_SIZE(disp->cursors); count++) {
    if (disp->cursors[count]) {
      XFreeCursor(disp->display, disp->cursors[count]);
    }
  }

  for (count = 0; count < disp->num_roots; count++) {
    ui_window_unmap(disp->roots[count]);
    ui_window_final(disp->roots[count]);
  }

  free(disp->roots);

  ui_xim_display_closed(disp->display);
  ui_picture_display_closed(disp->display);
  XCloseDisplay(disp->display);

  free(disp);
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, u_int depth) {
  int count;
  ui_display_t *disp;
  void *p;

  for (count = 0; count < num_displays; count++) {
    if (strcmp(displays[count]->name, disp_name) == 0) {
      return displays[count];
    }
  }

#ifdef USE_ALERT_DIALOG
  /* Callback should be set before bl_dialog() is called. */
  bl_dialog_set_callback(dialog_cb);
#endif

  if ((disp = open_display(disp_name, depth)) == NULL) {
    return NULL;
  }

  if ((p = realloc(displays, sizeof(ui_display_t *) * (num_displays + 1))) == NULL) {
    ui_display_close(disp);

    return NULL;
  }

  displays = p;
  displays[num_displays++] = disp;

  return disp;
}

void ui_display_close(ui_display_t *disp) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if (displays[count] == disp) {
      close_display(displays[count]);
      displays[count] = displays[--num_displays];

#ifdef DEBUG
      bl_debug_printf("X connection closed.\n");
#endif

      return;
    }
  }
}

void ui_display_close_all(void) {
  while (num_displays > 0) {
    close_display(displays[--num_displays]);
  }

  free(displays);

  displays = NULL;
}

ui_display_t **ui_get_opened_displays(u_int *num) {
  *num = num_displays;

  return displays;
}

int ui_display_fd(ui_display_t *disp) { return XConnectionNumber(disp->display); }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, char *wm_role, Window parent_window) {
  void *p;

  if ((p = realloc(disp->roots, sizeof(ui_window_t *) * (disp->num_roots + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  disp->roots = p;

  root->disp = disp;
  root->parent = NULL;

  if (parent_window) {
    root->parent_window = parent_window;
  } else {
    root->parent_window = disp->my_window;
  }

  root->gc = disp->gc;
  root->x = x;
  root->y = y;

  if (app_name) {
    root->app_name = app_name;
  }
#ifdef USE_XLIB
  if (wm_role) {
    root->wm_role = wm_role;
  }
#endif

  /*
   * root is added to disp->roots before ui_window_show() because
   * ui_display_get_group_leader() is called in ui_window_show().
   */
  disp->roots[disp->num_roots++] = root;

  ui_window_show(root, hint);

  return 1;
}

int ui_window_reset_group(ui_window_t *win); /* defined in xlib/ui_window.c */

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
    if (disp->roots[count] == root) {
      /* Don't switching on or off screen in exiting. */
      ui_window_unmap(root);

      ui_window_final(root);

      disp->num_roots--;

      if (count == disp->num_roots) {
        disp->roots[count] = NULL;
      } else {
        disp->roots[count] = disp->roots[disp->num_roots];

        if (count == 0) {
          /* Group leader is changed. */
#if 0
          bl_debug_printf(BL_DEBUG_TAG " Changing group_leader -> %x\n", disp->roots[0]->my_window);
#endif

          for (count = 0; count < disp->num_roots; count++) {
            ui_window_reset_group(disp->roots[count]);
          }
        }
      }

      return 1;
    }
  }

  return 0;
}

void ui_display_idling(ui_display_t *disp) {
  int count;

  for (count = 0; count < disp->num_roots; count++) {
    ui_window_idling(disp->roots[count]);
  }
}

int ui_display_receive_next_event(ui_display_t *disp) {
  XEvent event;
  int count;

  do {
    XNextEvent(disp->display, &event);

    if (!XFilterEvent(&event, None)) {
      for (count = 0; count < disp->num_roots; count++) {
        ui_window_receive_event(disp->roots[count], &event);
      }
    }
  } while (XEventsQueued(disp->display, QueuedAfterReading));

  return 1;
}

void ui_display_sync(ui_display_t *disp) {
  if (XEventsQueued(disp->display, QueuedAlready)) {
    ui_display_receive_next_event(disp);
  }

  XFlush(disp->display);
}

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_t *win) {
  if (disp->selection_owner) {
    ui_display_clear_selection(disp, disp->selection_owner);
  }

  disp->selection_owner = win;

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp, /* NULL means all selection owner windows. */
                               ui_window_t *win) {
  if (disp == NULL) {
    u_int count;

    for (count = 0; count < num_displays; count++) {
      ui_display_clear_selection(displays[count], displays[count]->selection_owner);
    }

    return 1;
  }

  if (disp->selection_owner == NULL || disp->selection_owner != win) {
    return 0;
  }

  if (disp->selection_owner->selection_cleared) {
    (*disp->selection_owner->selection_cleared)(disp->selection_owner);
  }

  disp->selection_owner = NULL;

  return 1;
}

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp) { return disp->modmap.map; }

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) {
  if (serial != disp->modmap.serial) {
    if (disp->modmap.map) {
      XFreeModifiermap(disp->modmap.map);
    }

    disp->modmap.map = XGetModifierMapping(disp->display);
    disp->modmap.serial = serial;
  }
}

Cursor ui_display_get_cursor(ui_display_t *disp, u_int shape) {
  int idx;

  /*
   * XXX
   * cursor[0] == XC_xterm / cursor[1] == XC_left_ptr / cursor[2] == XC_nil
   * Mlterm uses only these shapes.
   */

  if (shape == XC_xterm) {
    idx = 0;
  } else if (shape == XC_left_ptr) {
    idx = 1;
  } else if (shape == XC_nil) {
    idx = 2;
  } else {
    return None;
  }

  if (!disp->cursors[idx]) {
    if (idx == 2) {
      XFontStruct *font;
      XColor dummy;

      if (!(font = XLoadQueryFont(disp->display, "nil2")) &&
          !(font = XLoadQueryFont(disp->display, "fixed"))) {
        return None;
      }

      disp->cursors[idx] =
          XCreateGlyphCursor(disp->display, font->fid, font->fid, 'X', ' ', &dummy, &dummy);
      XFreeFont(disp->display, font);
    } else {
      disp->cursors[idx] = XCreateFontCursor(disp->display, shape);
    }
  }

  return disp->cursors[idx];
}

XVisualInfo *ui_display_get_visual_info(ui_display_t *disp) {
  XVisualInfo vinfo_template;
  int nitems;

  vinfo_template.visualid = XVisualIDFromVisual(disp->visual);

  /* Return pointer to the first element of VisualInfo list. */
  return XGetVisualInfo(disp->display, VisualIDMask, &vinfo_template, &nitems);
}

XID ui_display_get_group_leader(ui_display_t *disp) {
  if (disp->num_roots > 0) {
    return disp->roots[0]->my_window;
  } else {
    return None;
  }
}
