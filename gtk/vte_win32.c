/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <gdk/gdkwin32.h>

/* --- static variables --- */

static ui_display_t *_disp;

/* --- static functions --- */

#include <imagehlp.h>
#include <cairo/cairo.h>

static cairo_status_t (*orig_cairo_region_intersect)(cairo_region_t*, const cairo_region_t*);

static cairo_status_t cairo_region_intersect2(cairo_region_t *dst, const cairo_region_t *other) {
  /*
   * XXX
   * cairo_region_intersect() can receive NULL as 'other' argument in win32
   * when gdk_window_process_updates_with_mode() calls it.
   */
  if (other && orig_cairo_region_intersect) {
    return (*orig_cairo_region_intersect)(dst, other);
  } else {
    return CAIRO_STATUS_SUCCESS;
  }
}

static void *replace_func(const char *module_name, const char *func_name, void *func) {
  intptr_t base_addr = (intptr_t)GetModuleHandleA(module_name);
  ULONG size;
  PIMAGE_IMPORT_DESCRIPTOR img_desc;

  if (!base_addr) {
    return NULL;
  }

  img_desc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData((HMODULE)base_addr, TRUE,
                                                                 IMAGE_DIRECTORY_ENTRY_IMPORT,
                                                                 &size);

  for (; img_desc->Name; img_desc++) {
    PIMAGE_THUNK_DATA first_thunk = (PIMAGE_THUNK_DATA)(base_addr + (intptr_t)img_desc->FirstThunk);
    PIMAGE_THUNK_DATA orig_first_thunk =
      (PIMAGE_THUNK_DATA)(base_addr + (intptr_t)img_desc->OriginalFirstThunk);

    for (; first_thunk->u1.Function; first_thunk++, orig_first_thunk++) {
      if (!IMAGE_SNAP_BY_ORDINAL(orig_first_thunk->u1.Ordinal)) {
        PIMAGE_IMPORT_BY_NAME import_name =
          (PIMAGE_IMPORT_BY_NAME)(base_addr + (intptr_t)orig_first_thunk->u1.AddressOfData);

        if (stricmp((const char*)import_name->Name, func_name) == 0) {
          DWORD orig_protect;

          if (VirtualProtect(&first_thunk->u1.Function, sizeof(first_thunk->u1.Function),
                             PAGE_READWRITE, &orig_protect)) {
            void *orig_func = (void*)(intptr_t)first_thunk->u1.Function;

            WriteProcessMemory(GetCurrentProcess(), &first_thunk->u1.Function,
                               &func, sizeof(first_thunk->u1.Function), NULL);
            first_thunk->u1.Function = (intptr_t)func;

            VirtualProtect(&first_thunk->u1.Function, sizeof(first_thunk->u1.Function),
                           orig_protect, &orig_protect);

            return orig_func;
          } else {
            return NULL;
          }
        }
      }
    }
  }

  return NULL;
}

#if 0
/*
 * Don't call vt_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because vt_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn vte_terminal_filter(GdkXEvent *xevent, GdkEvent *event, /* GDK_NOTHING */
                                           gpointer data) {
  return GDK_FILTER_CONTINUE;
}
#endif

static void show_root(ui_display_t *disp, GtkWidget *widget) {
  VteTerminal *terminal = VTE_TERMINAL(widget);
#if 1
  HANDLE win = CreateWindowExW(0, L"MLTERM", L"",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               200, 100, None, NULL, disp->display->hinst, NULL);
  if (!win) {
    bl_debug_printf("FAILED to CreateWindowEx\n");
  } else {
    DestroyWindow(win);
  }
#endif

#ifdef UTF16_IME_CHAR
  ui_display_show_root(disp, &PVT(terminal)->screen->window, 0, 0, 0, L"mlterm", NULL,
                       GDK_WINDOW_HWND(gtk_widget_get_window(widget)));
#else
  ui_display_show_root(disp, &PVT(terminal)->screen->window, 0, 0, 0, "mlterm", NULL,
                       GDK_WINDOW_HWND(gtk_widget_get_window(widget)));
#endif
}

static void trigger_pty_read(void) {
  if (_disp->num_roots > 0) {
    PostMessage(_disp->roots[0]->my_window, WM_APP, 0, 0);
  }
}

int ui_gdiobj_pool_init(void); /* win32/ui_gdiobj_pool.h */
static gboolean transfer_data(gpointer data);

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  XEvent event;
  int count;

  if (msg == WM_APP) {
    /* See vte_terminal_io() in vte.c */

    for (count = 0; count < _disp->num_roots; count++) {
      vt_term_t *term = ((ui_screen_t*)_disp->roots[count])->term;

      if (term->pty) {
        vt_term_parse_vt100_sequence(term);

        if (!is_sending_data && vt_term_is_sending_data(term)) {
#if GTK_CHECK_VERSION(2, 12, 0)
          gdk_threads_add_timeout(1, transfer_data, term);
#else
          g_timeout_add(1, transfer_data, data);
#endif
          is_sending_data = 1;
        }
      }
    }

    vt_close_dead_terms();
  }

  event.window = hwnd;
  event.msg = msg;
  event.wparam = wparam;
  event.lparam = lparam;

  for (count = 0; count < _disp->num_roots; count++) {
    int val;

    val = ui_window_receive_event(_disp->roots[count], &event);
    if (val == 1) {
      /*
       * ui_window_receive_event() processes WM_APP.
       * (WM_APP == WM_APP_PAINT (see win32/ui_window.c))
       */

      return 0;
    } else if (val == -1) {
      break;
    }
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void init_display(ui_display_t *disp, VteTerminalClass *vclass) {
  static Display _display;
#ifndef UTF16_IME_CHAR
  WNDCLASS wc;
#else
  WNDCLASSW wc;
#endif

  if (!_display.hinst) {
    orig_cairo_region_intersect = replace_func("libgdk-3-0.dll", "cairo_region_intersect",
                                               cairo_region_intersect2);
    _display.hinst = GetModuleHandle(NULL);
    _display.fd = -1;
    _disp = disp;
    ui_gdiobj_pool_init();
  }

  ZeroMemory(&wc, sizeof(WNDCLASS));
  wc.lpfnWndProc = window_proc;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hInstance = _display.hinst;
  wc.hIcon = LoadIcon(_display.hinst, "MLTERM_ICON");
  disp->cursors[2] = wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = 0;
  wc.lpszClassName = __("MLTERM");

  if (!RegisterClass(&wc)) {
    bl_debug_printf("REGCLASS FAILED\n");
  }

  disp->display = &_display;
  disp->width = GetSystemMetrics(SM_CXSCREEN);
  disp->height = GetSystemMetrics(SM_CYSCREEN);
  disp->depth = 24;
  disp->gc = ui_gc_new(&_display, None);
  disp->name = ":0.0";

#if 0
  gdk_window_add_filter(NULL, vte_terminal_filter, NULL);
#endif

#ifdef USE_LIBSSH2
  vt_pty_ssh_set_pty_read_trigger(trigger_pty_read);
#ifdef USE_MOSH
  vt_pty_mosh_set_pty_read_trigger(trigger_pty_read);
#endif
#endif
#if 1
  vt_pty_win32_set_pty_read_trigger(trigger_pty_read);
#endif
}

void ui_sb_view_set_dpr(int dpr) {}
