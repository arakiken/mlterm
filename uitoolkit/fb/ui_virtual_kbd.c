/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_virtual_kbd.h"

#include <pobl/bl_debug.h>

#include "../ui_imagelib.h"

#ifndef XDATADIR
#define KBD_DIR "/usr/local/share/mlterm/kbd"
#else
#define KBD_DIR XDATADIR "/mlterm/kbd"
#endif

/* --- static variables --- */

static ui_window_t *kbd_win;

static struct kbd_key {
  int16_t left;
  int16_t right;
  u_int16_t ksym;
  u_int16_t shift_ksym;
  u_int keycode;

} kbd_keys[] = {
    {1, 35, XK_Escape, XK_Escape, 1},
    {37, 71, XK_F1, XK_F1, 59},
    {73, 107, XK_F2, XK_F2, 60},
    {109, 143, XK_F3, XK_F3, 61},
    {145, 179, XK_F4, XK_F4, 62},
    {181, 215, XK_F5, XK_F5, 63},
    {217, 251, XK_F6, XK_F6, 64},
    {253, 287, XK_F7, XK_F7, 65},
    {289, 323, XK_F8, XK_F8, 66},
    {325, 359, XK_F9, XK_F9, 67},
    {361, 395, XK_F10, XK_F10, 68},
    {397, 431, XK_F11, XK_F11, 87},
    {433, 467, XK_F11, XK_F12, 88},
    {589, 623, 0, 0, 0},

    {1, 35, '`', '~', 41},
    {37, 71, '1', '!', 2},
    {73, 107, '2', '@', 3},
    {109, 143, '3', '#', 4},
    {145, 179, '4', '$', 5},
    {181, 215, '5', '%', 6},
    {217, 251, '6', '^', 7},
    {253, 287, '7', '&', 8},
    {289, 323, '8', '*', 9},
    {325, 359, '9', '(', 10},
    {361, 395, '0', ')', 11},
    {397, 431, '-', '_', 12},
    {433, 467, '=', '+', 13},
    {469, 515, XK_BackSpace, XK_BackSpace, 14},
    {517, 551, XK_Insert, XK_Insert, 110},
    {553, 587, XK_Home, XK_Home, 102},
    {589, 623, XK_Prior, XK_Prior, 104},

    {1, 47, XK_Tab, XK_Tab, 15},
    {49, 83, 'q', 'Q', 16},
    {85, 119, 'w', 'W', 17},
    {121, 155, 'e', 'E', 18},
    {157, 191, 'r', 'R', 19},
    {193, 227, 't', 'T', 20},
    {229, 263, 'y', 'Y', 21},
    {265, 299, 'u', 'U', 22},
    {301, 335, 'i', 'I', 23},
    {337, 371, 'o', 'O', 24},
    {373, 407, 'p', 'P', 25},
    {409, 443, '[', '{', 26},
    {445, 479, ']', '}', 27},
    {481, 515, '\\', '|', 43},
    {517, 551, XK_Delete, XK_Delete, 111},
    {553, 587, XK_End, XK_End, 107},
    {589, 623, XK_Next, XK_Next, 109},

    {1, 59, XK_Caps_Lock, XK_Caps_Lock, 58},
    {61, 95, 'a', 'A', 30},
    {97, 131, 's', 'S', 31},
    {133, 167, 'd', 'D', 32},
    {169, 203, 'f', 'F', 33},
    {205, 239, 'g', 'G', 34},
    {241, 275, 'h', 'H', 35},
    {277, 311, 'j', 'J', 36},
    {313, 347, 'k', 'K', 37},
    {349, 383, 'l', 'L', 38},
    {385, 419, ';', ':', 39},
    {421, 455, '\'', '\"', 40},
    {457, 515, XK_Return, XK_Return, 28},

    {1, 71, XK_Shift_L, XK_Shift_L, 42},
    {73, 107, 'z', 'Z', 44},
    {109, 143, 'x', 'X', 45},
    {145, 179, 'c', 'C', 46},
    {181, 215, 'v', 'V', 47},
    {217, 251, 'b', 'B', 48},
    {253, 287, 'n', 'N', 49},
    {289, 323, 'm', 'M', 50},
    {325, 359, ',', '<', 51},
    {361, 395, '.', '>', 52},
    {397, 431, '/', '?', 53},
    {433, 515, XK_Shift_R, XK_Shift_R, 54},
    {553, 587, XK_Up, XK_Up, 103},

    {1, 71, XK_Control_L, XK_Control_L, 29},
    {73, 143, XK_Alt_L, XK_Alt_L, 56},
    {145, 359, ' ', ' ', 57},
    {361, 431, XK_Alt_R, XK_Alt_R, 100},
    {433, 515, XK_Control_R, XK_Control_R, 97},
    {517, 551, XK_Left, XK_Left, 105},
    {553, 587, XK_Down, XK_Down, 108},
    {589, 623, XK_Right, XK_Right, 106},
};

static struct kbd_key_group {
  int16_t top;
  int16_t bottom;
  u_int16_t num_of_keys;
  struct kbd_key *keys;

} kbd_key_groups[] = {
    {1, 35, 14, kbd_keys},
    {37, 71, 17, kbd_keys + 14},
    {73, 107, 17, kbd_keys + 31},
    {109, 143, 13, kbd_keys + 48},
    {145, 179, 13, kbd_keys + 61},
    {181, 215, 8, kbd_keys + 74},
};

static Pixmap normal_pixmap;
static Pixmap pressed_pixmap;

static int is_pressed;
static int16_t pressed_top;
static int16_t pressed_left;
static u_int16_t pressed_width;
static u_int16_t pressed_height;

static int x_off;

static int state;
static int lock_state;

/* --- static functions --- */

static int update_state(u_int ksym) {
  if (ksym == XK_Alt_L || ksym == XK_Alt_R) {
    if (state & Mod1Mask) {
      state &= ~Mod1Mask;

      return -1;
    } else {
      state |= Mod1Mask;

      return 1;
    }
  } else if (ksym == XK_Control_L || ksym == XK_Control_R) {
    if (state & ControlMask) {
      state &= ~ControlMask;

      return -1;
    } else {
      state |= ControlMask;

      return 1;
    }
  } else if (ksym == XK_Shift_L || ksym == XK_Shift_R) {
    if (state & ShiftMask) {
      state &= ~ShiftMask;

      return (lock_state & CLKED) ? 1 : -1;
    } else {
      state |= ShiftMask;

      return (lock_state & CLKED) ? -1 : 1;
    }
  } else if (ksym == XK_Caps_Lock) {
    state ^= ShiftMask;

    if (lock_state & CLKED) {
      lock_state &= ~CLKED;

      return -1;
    } else {
      lock_state |= CLKED;

      return 1;
    }
  } else {
    return 0;
  }
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  if (x < x_off) {
    ui_window_clear(win, x, y, width, height);

    if (width <= x_off - x) {
      return;
    }

    width -= (x_off - x);
    x = x_off;
  }

  if (x + width > x_off + normal_pixmap->width) {
    ui_window_clear(win, x_off + normal_pixmap->width, y, x_off + 1, height);

    if (x >= x_off + normal_pixmap->width) {
      return;
    }

    width = x_off + normal_pixmap->width - x;
  }

  ui_window_copy_area(win, normal_pixmap, None, x - x_off, y, width, height, x, y);
}

static int start_virtual_kbd(ui_display_t *disp) {
  u_int width;
  u_int height;

  if (normal_pixmap /* && pressed_pixmap */) {
    width = normal_pixmap->width;
    height = normal_pixmap->height;
  } else {
    width = 0;
    height = 0;
    if (!ui_imagelib_load_file(disp, KBD_DIR "/pressed_kbd.six", NULL, &pressed_pixmap, NULL,
                               &width, &height)) {
      /*
       * Note that pressed_pixmap can be non-NULL even if
       * ui_imagelib_load_file() fails.
       */
      pressed_pixmap = NULL;

      return 0;
    }

    width = 0;
    height = 0;
    if (!ui_imagelib_load_file(disp, KBD_DIR "/kbd.six", NULL, &normal_pixmap, NULL, &width,
                               &height)) {
      /*
       * Note that normal_pixmap can be non-NULL even if
       * ui_imagelib_load_file() fails.
       */
      normal_pixmap = NULL;

      goto error;
    }

#if 1
    if (disp->depth == 1) {
      /* XXX */
      Pixmap tmp;

      tmp = normal_pixmap;
      normal_pixmap = pressed_pixmap;
      pressed_pixmap = tmp;
    }
#endif

    /*
     * It is assumed that the width and height of kbd.png are the same
     * as those of pressed_kbg.png
     */
  }

  if (width > disp->width) {
    width = disp->width;
  }

  if (height > disp->height / 2) {
    height = disp->height / 2;
  }

  if (!(kbd_win = malloc(sizeof(ui_window_t)))) {
    goto error;
  }

  ui_window_init(kbd_win, disp->width, height, disp->width, height, disp->width, height, 0, 0, 0,
                 0);
  kbd_win->window_exposed = window_exposed;

  kbd_win->disp = disp;
  kbd_win->x = 0;
  kbd_win->y = disp->height - height;

  ui_window_show(kbd_win, 0);
  ui_window_clear_all(kbd_win);

  x_off = (disp->width - width) / 2;
  ui_window_copy_area(kbd_win, normal_pixmap, None, 0, 0, width, height, x_off, 0);

  if (disp->num_of_roots > 0) {
    ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height - height,
                                 NOTIFY_TO_MYSELF);
  }

  return 1;

error:
  if (normal_pixmap) {
    ui_delete_image(disp->display, normal_pixmap);
    normal_pixmap = NULL;
  }

  if (pressed_pixmap) {
    ui_delete_image(disp->display, pressed_pixmap);
    pressed_pixmap = NULL;
  }

  return 0;
}

/* --- global functions --- */

int ui_virtual_kbd_hide(void) {
  if (!kbd_win) {
    return 0;
  }

#if 0
  ui_delete_image(kbd_win->disp->display, normal_pixmap);
  normal_pixmap = NULL;

  ui_delete_image(kbd_win->disp->display, pressed_pixmap);
  pressed_pixmap = NULL;
#endif

  if (kbd_win->disp->num_of_roots > 0) {
    ui_window_resize_with_margin(kbd_win->disp->roots[0], kbd_win->disp->width,
                                 kbd_win->disp->height, NOTIFY_TO_MYSELF);
  }

  ui_window_final(kbd_win);
  kbd_win = NULL;

  return 1;
}

/*
 * Return value
 *  0: is not virtual kbd event.
 *  1: is virtual kbd event.
 * -1: is inside the virtual kbd area but not virtual kbd event.
 */
int ui_is_virtual_kbd_event(ui_display_t *disp, XButtonEvent *bev) {
  while (!kbd_win) {
    static int click_num;

    if (bev->type == ButtonPress) {
      if (bev->x + bev->y + 20 >= disp->width + disp->height) {
        if (click_num == 0) {
          click_num = 1;
        } else /* if( click_num == 1) */
        {
          click_num = 0;

          if (start_virtual_kbd(disp)) {
            break;
          }
        }
      } else {
        click_num = 0;
      }
    }

    return 0;
  }

  if (bev->y < kbd_win->y) {
    return 0;
  }

  if (kbd_win->disp->num_of_roots > 0 &&
      kbd_win->disp->roots[0]->y + kbd_win->disp->roots[0]->height > kbd_win->y) {
    /* disp->roots[0] seems to be resized. */

    ui_virtual_kbd_hide();

    return 0;
  }

  if (bev->type == ButtonRelease || bev->type == ButtonPress) {
    return 1;
  } else {
    is_pressed = 0;

    return -1;
  }
}

/*
 * Call this function after checking if ui_is_virtual_kbd_event() returns 1.
 * Return value
 *  1: keytop image is redrawn and kev is set.
 *  2: keytop image is redrawn but kev is not set.
 *  0: keytop image is not redrawn and kev is not set.
 */
int ui_virtual_kbd_read(XKeyEvent *kev, XButtonEvent *bev) {
  int x;
  int y;

  if (bev->type == ButtonRelease) {
    if (is_pressed) {
      ui_window_copy_area(kbd_win, normal_pixmap, None, pressed_left, pressed_top, pressed_width,
                          pressed_height, pressed_left + x_off, pressed_top);

      is_pressed = 0;

      return 2;
    }
  } else /* if( bev->type == ButtonPress) */
  {
    struct kbd_key_group *key_group;
    u_int count;

    y = bev->y - kbd_win->y;
    x = bev->x - x_off;

    for (count = 0, key_group = kbd_key_groups;
         count < sizeof(kbd_key_groups) / sizeof(kbd_key_groups[0]); count++, key_group++) {
      if (y < key_group->top) {
        break;
      }

      if (y <= key_group->bottom) {
        u_int count2;

        for (count2 = 0; count2 < key_group->num_of_keys; count2++) {
          if (x < key_group->keys[count2].left) {
            break;
          }

          if (x <= key_group->keys[count2].right) {
            Pixmap pixmap;
            int ret;

            if (key_group->keys[count2].ksym == 0) {
              /* [X] button */

              return ui_virtual_kbd_hide();
            }

            if ((ret = update_state(key_group->keys[count2].ksym)) < 0) {
              pixmap = normal_pixmap;
            } else {
              pixmap = pressed_pixmap;
            }

            pressed_top = key_group->top;
            pressed_height = key_group->bottom - pressed_top + 1;
            pressed_left = key_group->keys[count2].left;
            pressed_width = key_group->keys[count2].right - pressed_left + 1;

            ui_window_copy_area(kbd_win, pixmap, None, pressed_left, pressed_top, pressed_width,
                                pressed_height, pressed_left + x_off, pressed_top);

            if (ret == 0) {
              is_pressed = 1;

              kev->type = KeyPress;
              kev->state = state;
              kev->ksym = (state & ShiftMask) ? key_group->keys[count2].shift_ksym
                                              : key_group->keys[count2].ksym;
              kev->keycode = key_group->keys[count2].keycode;

              return 1;
            } else {
              return 2;
            }
          }
        }
      }
    }
  }

  is_pressed = 0;

  return 0;
}

ui_window_t *ui_is_virtual_kbd_area(int y) {
  if (kbd_win && y > kbd_win->y) {
    return kbd_win;
  } else {
    return NULL;
  }
}
