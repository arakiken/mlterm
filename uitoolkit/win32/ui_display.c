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
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

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
#ifdef USE_WIN32API
static DWORD main_tid; /* XXX set in main(). */
#endif

/* --- static functions --- */

#ifdef USE_WIN32API
static VOID CALLBACK timer_proc(HWND hwnd, UINT msg, UINT timerid, DWORD time) {
  ui_display_idling(&_disp);
}
#endif

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  XEvent event;
  int count;

  event.window = hwnd;
  event.msg = msg;
  event.wparam = wparam;
  event.lparam = lparam;

  for (count = 0; count < _disp.num_roots; count++) {
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

static int dialog_cb(bl_dialog_style_t style, const char *msg) {
  if (style == BL_DIALOG_OKCANCEL) {
    if (MessageBoxA(NULL, msg, "", MB_OKCANCEL) == IDOK) {
      return 1;
    } else {
      return 0;
    }
  } else if (style == BL_DIALOG_ALERT) {
    MessageBoxA(NULL, msg, "", MB_ICONSTOP);

    return 1;
  } else {
    return -1;
  }
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

#if 0
  SetDllDirectory("");

  {
    HMODULE module;

#if 1
    if ((module = LoadLibrary("Shcore"))) {
      typedef HRESULT (*func)(int);
      func set_process_dpi_awareness;

      if ((set_process_dpi_awareness = (func)GetProcAddress(module, "SetProcessDpiAwareness"))) {
        (*set_process_dpi_awareness)(1 /*PROCESS_SYSTEM_DPI_AWARE*/);
      }

      FreeLibrary(module);
    }
#else
    if ((module = LoadLibrary("User32"))) {
      typedef HRESULT (*func)(int);
      func set_process_dpi_awareness;

      if ((set_process_dpi_awareness = (func)GetProcAddress(module,
                                                            "SetProcessDpiAwarenessContext"))) {
        (*set_process_dpi_awareness)(-2 /*DPI_AWARENESS_CONTEXT_SYSTEM_AWARE*/);
      }

      FreeLibrary(module);
    }
#endif
  }
#endif

#ifdef USE_WIN32API
  /*
   * XXX
   * vt_pty_ssh_new() isn't called from the main thread, so main_tid
   * must be set here, not in vt_pty_ssh_new().
   */
  main_tid = GetCurrentThreadId();
#endif

  /* Callback should be set before bl_dialog() is called. */
  bl_dialog_set_callback(dialog_cb);

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
    ui_gc_destroy(_disp.gc);

    return NULL;
  }

  bl_file_set_cloexec(fd);
#endif

  ui_gdiobj_pool_init();

  /* _disp is initialized successfully. */
  _display.fd = fd;
  _disp.display = &_display;

#ifdef USE_WIN32API
  /* ui_window_manager_idling() called in 0.1sec. */
  SetTimer(NULL, 0, 100, timer_proc);
#endif

  return &_disp;
}

void ui_display_close(ui_display_t *disp) {
  if (disp == &_disp) {
    ui_display_close_all();
  }
}

void ui_display_close_all(void) {
  u_int count;

  if (!DISP_IS_INITED) {
    return;
  }

  ui_picture_display_closed(_disp.display);

  ui_gc_destroy(_disp.gc);

  for (count = 0; count < _disp.num_roots; count++) {
    ui_window_unmap(_disp.roots[count]);
    ui_window_final(_disp.roots[count]);
  }

  free(_disp.roots);

#if 0
  for (count = 0; count < BL_ARRAY_SIZE(_disp.cursors); count++) {
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
                         char *app_name, char *wm_role, Window parent_window
                         ) {
  void *p;

  if ((p = realloc(disp->roots, sizeof(ui_window_t*) * (disp->num_roots + 1))) == NULL) {
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

  disp->roots[disp->num_roots++] = root;

  return ui_window_show(root, hint);
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
    if (disp->roots[count] == root) {
      ui_window_unmap(root);
      ui_window_final(root);

      disp->num_roots--;

      if (count == disp->num_roots) {
        disp->roots[count] = NULL;
      } else {
        disp->roots[count] = disp->roots[disp->num_roots];
      }

      return 1;
    }
  }

  return 0;
}

void ui_display_idling(ui_display_t *disp) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
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

#ifdef USE_WIN32API
void ui_display_trigger_pty_read(void) {
  /* Exit GetMessage() in x_display_receive_next_event(). */
  PostThreadMessage(main_tid, WM_APP, 0, 0);
}
#endif
