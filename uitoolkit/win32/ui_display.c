/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_display.h"

#include <stdio.h>       /* sprintf */
#include <string.h>      /* memset/memcpy */
#include <stdlib.h>      /* getenv */
#include <pobl/bl_def.h> /* USE_WIN32API */
#ifndef USE_WIN32API
#include <fcntl.h> /* open */
#endif
#include <unistd.h> /* close */
#include <pobl/bl_file.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_dialog.h>

#include "../ui_window.h"
#include "../ui_picture.h"
#include "ui_gdiobj_pool.h"

#define DISP_IS_INITED (_disp.display)

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static ui_display_t _disp; /* Singleton */
static Display _display;

/* --- static functions --- */

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  XEvent event;
  int count;

  event.window = hwnd;
  event.msg = msg;
  event.wparam = wparam;
  event.lparam = lparam;

  for (count = 0; count < _disp.num_of_roots; count++) {
    int val;

    val = ui_window_receive_event(_disp.roots[count], &event);
    if (val == 1) {
      return 0;
    } else if (val == -1) {
      break;
    }
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

static int dialog(bl_dialog_style_t style, char *msg) {
  if (style == BL_DIALOG_OKCANCEL) {
    if (MessageBoxA(NULL, msg, "", MB_OKCANCEL) == IDOK) {
      return 1;
    }
  } else if (style == BL_DIALOG_ALERT) {
    MessageBoxA(NULL, msg, "", MB_ICONSTOP);
  } else {
    return -1;
  }

  return 0;
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, /* Ignored */
                              u_int depth      /* Ignored */
                              ) {
#ifndef UTF16_IME_CHAR
  WNDCLASS wc;
#else
  WNDCLASSW wc;
#endif
  int fd;

  if (DISP_IS_INITED) {
    /* Already opened. */
    return &_disp;
  }

  /* Callback should be set before bl_dialog() is called. */
  bl_dialog_set_callback(dialog);

  _display.hinst = GetModuleHandle(NULL);

  /* Prepare window class */
  ZeroMemory(&wc, sizeof(WNDCLASS));
  wc.lpfnWndProc = window_proc;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hInstance = _display.hinst;
  wc.hIcon = LoadIcon(_display.hinst, "MLTERM_ICON");
  _disp.cursors[2] = wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = 0;
  wc.lpszClassName = __("MLTERM");

  if (!RegisterClass(&wc)) {
    bl_dialog(BL_DIALOG_ALERT, "Failed to register class");

    return NULL;
  }

  _disp.width = GetSystemMetrics(SM_CXSCREEN);
  _disp.height = GetSystemMetrics(SM_CYSCREEN);
  _disp.depth = 24;

  if ((_disp.gc = ui_gc_new(&_display, None)) == NULL) {
    return NULL;
  }

  if (!(_disp.name = getenv("DISPLAY"))) {
    _disp.name = ":0.0";
  }

#ifdef USE_WIN32API
  fd = -1;
#else
  if ((fd = open("/dev/windows", O_NONBLOCK, 0)) == -1) {
    ui_gc_delete(_disp.gc);

    return NULL;
  }

  bl_file_set_cloexec(fd);
#endif

  ui_gdiobj_pool_init();

  /* _disp is initialized successfully. */
  _display.fd = fd;
  _disp.display = &_display;

  return &_disp;
}

int ui_display_close(ui_display_t *disp) {
  if (disp == &_disp) {
    return ui_display_close_all();
  } else {
    return 0;
  }
}

int ui_display_close_all(void) {
  u_int count;

  if (!DISP_IS_INITED) {
    return 0;
  }

  ui_picture_display_closed(_disp.display);

  ui_gc_delete(_disp.gc);

  for (count = 0; count < _disp.num_of_roots; count++) {
    ui_window_unmap(_disp.roots[count]);
    ui_window_final(_disp.roots[count]);
  }

  free(_disp.roots);

#if 0
  for (count = 0; count < (sizeof(_disp.cursors) / sizeof(_disp.cursors[0])); count++) {
    if (_disp.cursors[count]) {
      CloseHandle(_disp.cursors[count]);
    }
  }
#endif

  if (_display.fd != -1) {
    close(_display.fd);
  }

  _disp.display = NULL;

  ui_gdiobj_pool_final();

  return 1;
}

ui_display_t **ui_get_opened_displays(u_int *num) {
  static ui_display_t *opened_disp = &_disp;

  if (!DISP_IS_INITED) {
    *num = 0;

    return NULL;
  }

  *num = 1;

  return &opened_disp;
}

int ui_display_fd(ui_display_t *disp) { return disp->display->fd; }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, Window parent_window /* Ignored */
                         ) {
  void *p;

  if ((p = realloc(disp->roots, sizeof(ui_window_t*) * (disp->num_of_roots + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  disp->roots = p;

  root->disp = disp;
  root->parent = NULL;
  root->parent_window = disp->my_window;
  root->gc = disp->gc;
  root->x = x;
  root->y = y;

  if (app_name) {
    root->app_name = app_name;
  }

  disp->roots[disp->num_of_roots++] = root;

  return ui_window_show(root, hint);
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_of_roots; count++) {
    if (disp->roots[count] == root) {
      ui_window_unmap(root);
      ui_window_final(root);

      disp->num_of_roots--;

      if (count == disp->num_of_roots) {
        disp->roots[count] = NULL;
      } else {
        disp->roots[count] = disp->roots[disp->num_of_roots];
      }

      return 1;
    }
  }

  return 0;
}

void ui_display_idling(ui_display_t *disp) {
  u_int count;

  for (count = 0; count < disp->num_of_roots; count++) {
    ui_window_idling(disp->roots[count]);
  }
}

/*
 * <Return value>
 *  0: Receive WM_QUIT
 *  1: Receive other messages.
 */
int ui_display_receive_next_event(ui_display_t *disp) {
  MSG msg;

#ifdef USE_WIN32API
  /* 0: WM_QUIT, -1: Error */
  if (GetMessage(&msg, NULL, 0, 0) <= 0) {
    return 0;
  }

  TranslateMessage(&msg);
  DispatchMessage(&msg);
#endif

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      return 0;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 1;
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

int ui_display_clear_selection(ui_display_t *disp, ui_window_t *win) {
  if (disp->selection_owner == NULL || disp->selection_owner != win) {
    return 0;
  }

  disp->selection_owner->is_sel_owner = 0;

  if (disp->selection_owner->selection_cleared) {
    (*disp->selection_owner->selection_cleared)(disp->selection_owner);
  }

  disp->selection_owner = NULL;

  return 1;
}

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp) { return disp->modmap.map; }

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) { /* dummy */ }

Cursor ui_display_get_cursor(ui_display_t *disp, u_int shape) {
  int idx;
  LPCTSTR name;

  /*
   * XXX
   * cursor[0] == XC_xterm / cursor[1] == XC_left_ptr / cursor[2] == not used
   * Mlterm uses only these shapes.
   */

  if (shape == XC_xterm) {
    idx = 0;
    name = IDC_IBEAM;
  } else if (shape == XC_left_ptr) {
    idx = 1;
    name = IDC_ARROW; /* already loaded in ui_display_open() */
  } else {
    return None;
  }

  if (!disp->cursors[idx]) {
    disp->cursors[idx] = LoadCursor(NULL, name);
  }

  return disp->cursors[idx];
}

XID ui_display_get_group_leader(ui_display_t *disp) { return None; }
