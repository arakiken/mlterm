/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_display.h"

#include <stdio.h>       /* sprintf */
#include <string.h>      /* memset/memcpy */
#include <stdlib.h>      /* getenv */
#include <pobl/bl_def.h> /* USE_WIN32API */
#include <pobl/bl_file.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_dialog.h>

#include "ui.h"
#include "../ui_window.h"
#include "../ui_picture.h"
#include "cocoa.h"

#define DISP_IS_INITED (_disp.display)

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static ui_display_t _disp; /* Singleton */
static Display _display;
static ui_display_t *opened_disp = &_disp;

/* --- static functions --- */

static int dialog_cb(bl_dialog_style_t style, const char *msg) {
  if (style == BL_DIALOG_OKCANCEL) {
    return cocoa_dialog_okcancel(msg);
  } else if (style == BL_DIALOG_ALERT) {
    return cocoa_dialog_alert(msg);
  } else {
    return -1;
  }
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, /* Ignored */
                              u_int depth      /* Ignored */
                              ) {
  if (DISP_IS_INITED) {
    /* Already opened. */
    return &_disp;
  }

  /* Callback should be set before bl_dialog() is called. */
  bl_dialog_set_callback(dialog_cb);

  /* _disp.width and _disp.height are set in viewDidMoveToWindow */

  _disp.depth = 32;

  if (!(_disp.name = getenv("DISPLAY"))) {
    _disp.name = ":0.0";
  }

  /* _disp is initialized successfully. */
  _display.fd = -1;
  _disp.display = &_display;

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

  _disp.display = NULL;
}

ui_display_t **ui_get_opened_displays(u_int *num) {
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

  if ((p = realloc(disp->roots, sizeof(ui_window_t*) * (disp->num_roots + 1))) == NULL) {
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

  disp->roots[disp->num_roots++] = root;

  if (ui_window_show(root, hint)) {
    if (disp->num_roots > 1) {
      window_alloc(root);
    }

    return 1;
  } else {
    return 0;
  }
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

  for (count = 0; count < _disp.num_roots; count++) {
    ui_window_idling(_disp.roots[count]);
  }
}

int ui_display_receive_next_event(ui_display_t *disp) { return 1; }

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

  if (disp->selection_owner->selection_cleared) {
    (*disp->selection_owner->selection_cleared)(disp->selection_owner);
  }

  disp->selection_owner = NULL;

  return 1;
}

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp) { return disp->modmap.map; }

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) { /* dummy */ }

Cursor ui_display_get_cursor(ui_display_t *disp, u_int shape) {
  return None;
}

XID ui_display_get_group_leader(ui_display_t *disp) { return None; }
