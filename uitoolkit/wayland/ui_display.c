/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_display.h"

#include <string.h> /* memset/memcpy */
#include <sys/mman.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>  /* strdup */
#include <pobl/bl_file.h> /* bl_file_set_cloexec */

#include "../ui_window.h"
#include "../ui_picture.h"
#include "../ui_imagelib.h"

#if 0
#define __DEBUG
#endif

#ifndef BTN_LEFT
#define BTN_LEFT 0x110
#endif
#ifndef BTN_RIGHT
#define BTN_RIGHT 0x111
#endif
#ifndef BTN_MIDDLE
#define BTN_MIDDLE 0x112
#endif

#define DECORATION_MARGIN 0 /* for decorataion */
#define RESIZE_MARGIN 4  /* for resizing area */

#define KEY_REPEAT_UNIT 25       /* msec (see ui_event_source.c) */
#define DEFAULT_KEY_REPEAT_1 400 /* msec */
#define DEFAULT_KEY_REPEAT_N 50  /* msec */

/* --- static variables --- */

static u_int num_of_wlservs;
static ui_wlserv_t **wlservs;
static u_int num_of_displays;
static ui_display_t **displays;
static int rotate_display;
int kbd_repeat_1 = DEFAULT_KEY_REPEAT_1;
int kbd_repeat_N = DEFAULT_KEY_REPEAT_N;

/* --- static functions --- */

static int create_tmpfile_cloexec(char *tmpname) {
	int fd;

#ifdef HAVE_MKOSTEMP
	if ((fd = mkostemp(tmpname, O_CLOEXEC)) >= 0) {
		unlink(tmpname);
  }
#else
	if ((fd = mkstemp(tmpname)) >= 0) {
    bl_file_set_cloexec(fd);
		unlink(tmpname);
	}
#endif

	return fd;
}

static int create_anonymous_file(off_t size) {
	static const char template[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;
	int ret;

	if (!(path = getenv("XDG_RUNTIME_DIR"))) {
		return -1;
	}

	if (!(name = malloc(strlen(path) + sizeof(template)))) {
		return -1;
  }

	strcpy(name, path);
	strcat(name, template);

	fd = create_tmpfile_cloexec(name);
	free(name);

	if (fd < 0) {
		return -1;
  }

	if ((ret = posix_fallocate(fd, 0, size)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static ui_window_t *search_focused_window(ui_window_t *win) {
  u_int count;
  ui_window_t *focused;

  /*
   * *parent* - *child*
   *            ^^^^^^^ => Hit this window instead of the parent window.
   *          - child
   *          - child
   * (**: is_focused == 1)
   */
  for (count = 0; count < win->num_of_children; count++) {
    if ((focused = search_focused_window(win->children[count]))) {
      return focused;
    }
  }

  if (win->is_focused) {
    return win;
  }

  return NULL;
}

static ui_window_t *search_inputtable_window(ui_window_t *candidate, ui_window_t *win) {
  u_int count;

  if (win->inputtable) {
    if (candidate == NULL || win->inputtable > candidate->inputtable) {
      candidate = win;
    }
  }

  for (count = 0; count < win->num_of_children; count++) {
    candidate = search_inputtable_window(candidate, win->children[count]);
  }

  return candidate;
}

/*
 * x and y are rotated values.
 */
static inline ui_window_t *get_window(ui_window_t *win, int x, int y) {
  u_int count;

  for (count = 0; count < win->num_of_children; count++) {
    ui_window_t *child;

    if ((child = win->children[count])->is_mapped) {
      if (child->x <= x && x < child->x + ACTUAL_WIDTH(child) && child->y <= y &&
          y < child->y + ACTUAL_HEIGHT(child)) {
        return get_window(child, x - child->x, y - child->y);
      }
    }
  }

  return win;
}

static inline u_char *get_fb(Display *display, int x, int y) {
  return ((u_char*)display->fb) + (y + DECORATION_MARGIN) * display->line_length +
    x * display->bytes_per_pixel;
}

static ui_display_t *surface_to_display(struct wl_surface *surface) {
  u_int count;

  for (count = 0; count < num_of_displays; count++) {
    if (surface == displays[count]->display->surface) {
      return displays[count];
    }
  }

  return NULL;
}

#ifdef COMPAT_LIBVTE
static ui_display_t *parent_surface_to_display(struct wl_surface *surface) {
  u_int count;

  for (count = 0; count < num_of_displays; count++) {
    if (surface == displays[count]->display->parent_surface) {
      return displays[count];
    }
  }

  return NULL;
}

static ui_display_t *add_root_to_display(ui_display_t *disp, ui_window_t *root,
                                         struct wl_surface *parent_surface) {
  ui_wlserv_t *wlserv = disp->display->wlserv;
  ui_display_t *new;
  void *p;

  if (!(p = realloc(displays, sizeof(ui_display_t*) * (num_of_displays + 1)))) {
    return NULL;
  }
  displays = p;

  if (wlservs == NULL) {
    if (!(wlservs = malloc(sizeof(ui_wlserv_t*)))) {
      return NULL;
    }
    wlservs[0] = wlserv;
    num_of_wlservs = 1;
  }

  if (!(new = calloc(1, sizeof(ui_display_t) + sizeof(Display)))) {
    return NULL;
  }

  memcpy(new, disp, sizeof(ui_display_t));
  new->display = new + 1;
  new->name = strdup(disp->name);
  new->roots = NULL;
  new->num_of_roots = 0;

  memcpy(new->display, disp->display, sizeof(Display));

  new->display->parent_surface = parent_surface;

  displays[num_of_displays++] = new;

  if ((p = realloc(disp->roots, sizeof(ui_window_t*) * (disp->num_of_roots + 1)))) {
    disp->roots = p;
    disp->roots[disp->num_of_roots++] = root;
  }

  wlserv->ref_count++;

  return new;
}

static ui_display_t *remove_root_from_display(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_of_roots; count++) {
    if (disp->roots[count] == root) {
      disp->roots[count] = disp->roots[--disp->num_of_roots];
      break;
    }
  }

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count]->roots[0] == root) {
      return displays[count];
    }
  }

  return NULL;
}
#endif

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version) {
  ui_wlserv_t *wlserv = data;
  if(strcmp(interface, "wl_compositor") == 0) {
    wlserv->compositor = wl_registry_bind(registry, name,
                                          &wl_compositor_interface, 1);
  }
#ifdef COMPAT_LIBVTE
  else if (strcmp(interface, "wl_subcompositor") == 0) {
    wlserv->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
  }
#else
  else if(strcmp(interface, "wl_shell") == 0) {
    wlserv->shell = wl_registry_bind(registry, name,
                                     &wl_shell_interface, 1);
  }
#endif
  else if(strcmp(interface, "wl_shm") == 0) {
    wlserv->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if(strcmp(interface, "wl_seat") == 0) {
    wlserv->seat = wl_registry_bind(registry, name,
                                    &wl_seat_interface, 4); /* 4 is for keyboard_repeat_info */
  } else if(strcmp(interface, "wl_output") == 0) {
    wlserv->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    wlserv->data_device_manager = wl_registry_bind(registry, name,
                                                   &wl_data_device_manager_interface, 3);
  }
#ifdef __DEBUG
  else {
    bl_debug_printf("Unknown interface: %s\n", interface);
  }
#endif
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
  registry_global,
  registry_global_remove
};

static void receive_key_event(ui_wlserv_t *wlserv, XKeyEvent *ev) {
  if (wlserv->current_kbd_surface) {
    ui_display_t *disp = surface_to_display(wlserv->current_kbd_surface);
    ui_window_t *win;

#ifdef COMPAT_LIBVTE
    if (!disp) {
      disp = parent_surface_to_display(wlserv->current_kbd_surface);
    }
#endif

    /* Key event for dead surface may be received. */
    if (disp && (win = search_focused_window(disp->roots[0]))) {
      ui_window_receive_event(win, ev);
    }
  }
}

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                            uint32_t format, int fd, uint32_t size){
  ui_wlserv_t *wlserv = data;
  char *string;
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }

  if ((string = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
    close(fd);
    return;
  }

  wlserv->xkb->keymap = xkb_keymap_new_from_string(wlserv->xkb->ctx, string,
                                                   XKB_KEYMAP_FORMAT_TEXT_V1, 0);
  munmap(string, size);
  close(fd);

  wlserv->xkb->state = xkb_state_new(wlserv->xkb->keymap);

  wlserv->xkb->ctrl = xkb_keymap_mod_get_index(wlserv->xkb->keymap, XKB_MOD_NAME_CTRL);
  wlserv->xkb->alt = xkb_keymap_mod_get_index(wlserv->xkb->keymap, XKB_MOD_NAME_ALT);
  wlserv->xkb->shift = xkb_keymap_mod_get_index(wlserv->xkb->keymap, XKB_MOD_NAME_SHIFT);
  wlserv->xkb->logo = xkb_keymap_mod_get_index(wlserv->xkb->keymap, XKB_MOD_NAME_LOGO);
}

static void auto_repeat(void) {
  u_int count;

  for (count = 0; count < num_of_wlservs; count++) {
    if (wlservs[count]->prev_kev.type && --wlservs[count]->kbd_repeat_wait == 0) {
      if (wlservs[count]->kbd_repeat_count++ == 2) {
        /*
         * Selecting next candidate of ibus lookup table by repeating space key
         * freezes without this.
         */
        wl_display_roundtrip(wlservs[count]->display);
        wlservs[count]->kbd_repeat_count = 0;
      }

      wlservs[count]->prev_kev.time += kbd_repeat_N;
      receive_key_event(wlservs[count], &wlservs[count]->prev_kev);
      wlservs[count]->kbd_repeat_wait = (kbd_repeat_N + KEY_REPEAT_UNIT / 2) / KEY_REPEAT_UNIT;
    }
  }
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys) {
  ui_display_t *disp = surface_to_display(surface);
  ui_wlserv_t *wlserv = data;
  ui_window_t *win;

  wlserv->current_kbd_surface = surface;

#ifdef COMPAT_LIBVTE
  if (!disp || disp->num_of_roots == 0) {
    return;
  }
#endif

  if (disp->display->parent) {
    /* XXX input method */
    disp = disp->display->parent;
    surface = disp->display->surface;
  }

#ifdef __DEBUG
  bl_debug_printf("KBD ENTER %p\n", surface);
#endif

  if ((win = search_inputtable_window(NULL, disp->roots[0]))) {
#ifdef __DEBUG
    bl_debug_printf("FOCUSED %p\n", win);
#endif
    ui_window_set_input_focus(win);
  }

  /* During resizing keyboard leaves. keyboard_enter() means that resizing has finished. */
  disp->display->is_resizing = 0;
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp = surface_to_display(surface);
  ui_window_t *win;

#ifdef __DEBUG
  bl_debug_printf("KBD LEAVE %p\n", surface);
#endif

#if 0
  wlserv->prev_kev.type = 0;
#endif

  if (wlserv->current_kbd_surface == surface) {
    wlserv->current_kbd_surface = NULL;
  }

  /* surface may have been already destroyed. So null check of disp is necessary. */
  if (disp && (win = search_focused_window(disp->roots[0]))) {
#ifdef __DEBUG
    bl_debug_printf("UNFOCUSED %p\n", win);
#endif
    XEvent ev;
    ev.type = FocusOut;
    ui_window_receive_event(ui_get_root_window(win), &ev);
  }
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state_w) {
  ui_wlserv_t *wlserv = data;
  XKeyEvent ev;

  if (state_w == WL_KEYBOARD_KEY_STATE_PRESSED) {
    ev.ksym = xkb_state_key_get_one_sym(wlserv->xkb->state, key + 8);
    ev.type = KeyPress;
    ev.keycode = 0;
    ev.state = wlserv->xkb->mods;
    ev.time = time;

    wlserv->kbd_repeat_wait = (kbd_repeat_1 + KEY_REPEAT_UNIT - 1) / KEY_REPEAT_UNIT;
    wlserv->prev_kev = ev;

#ifdef __DEBUG
    bl_debug_printf("Receive key %x %x at %d\n", ev.ksym, wlserv->xkb->mods, time);
#endif
  } else if (state_w == WL_KEYBOARD_KEY_STATE_RELEASED) {
#if 0
    ev.type = KeyRelease;
    ev.time = time;
    ev.ksym = ev.keycode = ev.state = 0;
#endif
    wlserv->prev_kev.type = 0;
#ifdef __DEBUG
    bl_debug_printf("key released\n");
#endif
    return;
  }
  else {
    return;
  }

  wlserv->serial = serial;

  receive_key_event(wlserv, &ev);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
  ui_wlserv_t *wlserv = data;
  xkb_mod_mask_t mod_mask;

  xkb_state_update_mask(wlserv->xkb->state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  mod_mask = xkb_state_serialize_mods(wlserv->xkb->state, XKB_STATE_MODS_EFFECTIVE);
  wlserv->xkb->mods = 0;

  if (mod_mask & (1 << wlserv->xkb->ctrl))
    wlserv->xkb->mods |= ControlMask;
  if (mod_mask & (1 << wlserv->xkb->alt))
    wlserv->xkb->mods |= ModMask;
  if (mod_mask & (1 << wlserv->xkb->shift))
    wlserv->xkb->mods |= ShiftMask;
#if 0
  if (mod_mask & (1 << wlserv->xkb->logo))
    wlserv->xkb->mods |= MOD_MASK_LOGO;
#endif

#ifdef __DEBUG
  bl_debug_printf("MODIFIER MASK %x\n", wlserv->xkb->mods);
#endif
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                 int32_t rate, int32_t delay) {
#ifdef __DEBUG
  bl_debug_printf("Repeat info rate %d delay %d.\n", rate, delay);
#endif
  kbd_repeat_1 = delay;
#ifdef COMPAT_LIBVTE
  kbd_repeat_N = (1000 / rate); /* XXX 1000 / rate is too late. */
#else
  kbd_repeat_N = (500 / rate); /* XXX 1000 / rate is too late. */
#endif
}

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_keymap,
  keyboard_enter,
  keyboard_leave,
  keyboard_key,
  keyboard_modifiers,
  keyboard_repeat_info,
};

static void change_cursor(struct wl_pointer *pointer, uint32_t serial,
                          struct wl_surface *surface, struct wl_cursor *cursor) {
  struct wl_buffer *buffer;
  struct wl_cursor_image *image;

  image = cursor->images[0];
  if (!(buffer = wl_cursor_image_get_buffer(image))) {
    return;
  }
  wl_pointer_set_cursor(pointer, serial, surface, image->hotspot_x, image->hotspot_y);
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, image->width, image->height);
  wl_surface_commit(surface);
}

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx_w, wl_fixed_t sy_w) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("POINTER ENTER %p\n", surface);
#endif

  wlserv->current_pointer_surface = surface;

#ifdef COMPAT_LIBVTE
  if (parent_surface_to_display(surface)) {
    return;
  }
#endif

  if (wlserv->cursor[0]) {
    change_cursor(pointer, serial, wlserv->cursor_surface, wlserv->cursor[0]);
    wlserv->current_cursor = 0;
  }
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface) {
  ui_wlserv_t *wlserv = data;

  if (wlserv->current_pointer_surface == surface) {
    wlserv->current_pointer_surface = NULL;
  }

#ifdef __DEBUG
  bl_debug_printf("POINTER LEAVE %p\n", surface);
#endif
}

static int get_edge_state(u_int width, u_int height, int x, int y) {
  if (x < RESIZE_MARGIN) {
    if (y < height / 8) {
      return WL_SHELL_SURFACE_RESIZE_TOP_LEFT;
    } else if (y > height - height / 8) {
      return WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT;
    }
    return WL_SHELL_SURFACE_RESIZE_LEFT;
  } else if (x > width - RESIZE_MARGIN * 2) {
    if (y < height / 8) {
      return WL_SHELL_SURFACE_RESIZE_TOP_RIGHT;
    } else if (y > height - height / 8) {
      return WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
    }
    return WL_SHELL_SURFACE_RESIZE_RIGHT;
  } else if (y < RESIZE_MARGIN) {
    if (x < width / 8) {
      return WL_SHELL_SURFACE_RESIZE_TOP_LEFT;
    } else if (x > width - width / 8) {
      return WL_SHELL_SURFACE_RESIZE_TOP_RIGHT;
    } else if (width * 2 / 5 < x && x < width - width * 2 / 5) {
      return 11;
    }
    return WL_SHELL_SURFACE_RESIZE_TOP;
  } else if (y > height - RESIZE_MARGIN * 2) {
    if (x < width / 8){
      return WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT;
    } else if (x > width - width / 8){
      return WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
    } else if (width * 2 / 5 < x && x < width - width * 2 / 5) {
      return 11;
    }
    return WL_SHELL_SURFACE_RESIZE_BOTTOM;
  }
  return WL_SHELL_SURFACE_RESIZE_NONE;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time /* milisec */, wl_fixed_t sx_w, wl_fixed_t sy_w) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef ____DEBUG
  bl_debug_printf("Pointer motion at %d\n", time);
#endif
  disp = surface_to_display(wlserv->current_pointer_surface);

  if (disp) {
    XMotionEvent ev;
    ui_window_t *win;
    int resize_state;

    ev.type = MotionNotify;
    ev.time = time;
    ev.state = wlserv->xkb->mods;
    if (wlserv->pointer_button) {
      ev.state |= (Button1Mask << (wlserv->pointer_button - 1));
    }
    wlserv->pointer_x = ev.x = wl_fixed_to_int(sx_w);
    wlserv->pointer_y = ev.y = wl_fixed_to_int(sy_w);

    if ((resize_state = get_edge_state(disp->display->width, disp->display->height,
                                       ev.x, ev.y))) {
      if (resize_state > 3) {
        /* 3 and 7 are missing numbers in wl_shell_surface_resize */
        if (resize_state > 7) {
          resize_state -= 2;
        } else {
          resize_state --;
        }
      }

      if (!wlserv->cursor[resize_state]) {
        resize_state = 0;
      }
    }

    if (resize_state != wlserv->current_cursor) {
      change_cursor(pointer, 0, wlserv->cursor_surface, wlserv->cursor[resize_state]);
      wlserv->current_cursor = resize_state;
    }

    if (rotate_display) {
      int tmp = ev.x;
      if (rotate_display > 0) {
        ev.x = ev.y;
        ev.y = disp->display->width - tmp - 1;
      } else {
        ev.x = disp->display->height - ev.y - 1;
        ev.y = tmp;
      }
    }

    win = get_window(disp->roots[0], ev.x, ev.y);
    ev.x -= win->x;
    ev.y -= win->y;

#ifdef ____DEBUG
    bl_debug_printf("Motion event state %x x %d y %d in %p window.\n", ev.state, ev.x, ev.y, win);
#endif

    ui_window_receive_event(win, &ev);
  }
}

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state_w) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef __DEBUG
  bl_debug_printf("Pointer button at %d\n", time);
#endif
  disp = surface_to_display(wlserv->current_pointer_surface);

  if (disp) {
    XButtonEvent ev;
    ui_window_t *win;

    ev.x = wlserv->pointer_x;
    ev.y = wlserv->pointer_y;

    if (state_w == WL_POINTER_BUTTON_STATE_PRESSED) {
      ev.type = ButtonPress;
      if (button == BTN_LEFT) {
#ifndef COMPAT_LIBVTE
        int state = get_edge_state(disp->display->width, disp->display->height, ev.x, ev.y);
        if (state > 0) {
          if (state == 11) {
            wl_shell_surface_move(disp->display->shell_surface, wlserv->seat, serial);
          } else {
            disp->display->is_resizing = 1;
            wl_shell_surface_resize(disp->display->shell_surface, wlserv->seat, serial, state);
          }

          return;
        }
#endif
        ev.button = 1;
      } else if (button == BTN_MIDDLE) {
        ev.button = 2;
      } else if (button == BTN_RIGHT) {
        ev.button = 3;
      } else {
        return;
      }
    } else {
      ev.type = ButtonRelease;
      ev.button = 0;
    }
    wlserv->pointer_button = ev.button;
    ev.time = time;
    ev.state = wlserv->xkb->mods;

    if (rotate_display) {
      int tmp = ev.x;
      if (rotate_display > 0) {
        ev.x = ev.y;
        ev.y = disp->display->width - tmp - 1;
      } else {
        ev.x = disp->display->height - ev.y - 1;
        ev.y = tmp;
      }
    }

    win = get_window(disp->roots[0], ev.x, ev.y);
    ev.x -= win->x;
    ev.y -= win->y;

#ifdef __DEBUG
    bl_debug_printf("Button event type %d button %d state %x x %d y %d in %p window.\n",
                    ev.type, ev.button, ev.state, ev.x, ev.y, win);
#endif

    wlserv->serial = serial;

    ui_window_receive_event(win, &ev);
  }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef __DEBUG
  bl_debug_printf("Pointer axis at %d\n", time);
#endif
  disp = surface_to_display(wlserv->current_pointer_surface);

  if (disp) {
    XButtonEvent ev;
    ui_window_t *win;

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
      /* Vertical scroll */
      if (value < 0) {
        ev.button = 4;
      } else {
        ev.button = 5;
      }
    } else /* if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) */ {
      /* Horizontal scroll */
      return;
    }

    ev.time = time;
    ev.state = wlserv->xkb->mods;

    ev.x = wlserv->pointer_x;
    ev.y = wlserv->pointer_y;

    if (rotate_display) {
      int tmp = ev.x;
      if (rotate_display > 0) {
        ev.x = ev.y;
        ev.y = disp->display->width - tmp - 1;
      } else {
        ev.x = disp->display->height - ev.y - 1;
        ev.y = tmp;
      }
    }

    win = get_window(disp->roots[0], ev.x, ev.y);
    ev.x -= win->x;
    ev.y -= win->y;

#ifdef __DEBUG
    bl_debug_printf("Wheel event button %d state %x x %d y %d in %p window.\n",
                    ev.button, ev.state, ev.x, ev.y, win);
#endif

    ev.type = ButtonPress;
    ui_window_receive_event(win, &ev);

    ev.type = ButtonRelease;
    ui_window_receive_event(win, &ev);
  }
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_enter,
  pointer_leave,
  pointer_motion,
  pointer_button,
  pointer_axis,
};

static void surface_enter(void *data, struct wl_surface *surface, struct wl_output *output) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("SURFACE ENTER %p\n", surface);
#endif

  wlserv->current_kbd_surface = surface;
}

static void surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("SURFACE LEAVE %p\n", surface);
#endif

  if (wlserv->current_kbd_surface == surface) {
    wlserv->current_kbd_surface = NULL;
  }
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

/* Call this after display->surface was created. */
static int create_shm_buffer(Display *display) {
  struct wl_shm_pool *pool;
  int fd;
  int size;

#ifdef __DEBUG
  bl_debug_printf("Buffer w %d h %d bpp %d\n",
                  display->width, display->height, display->bytes_per_pixel);
#endif

  display->line_length = display->width * display->bytes_per_pixel;
  size = display->line_length * (display->height + DECORATION_MARGIN);

  if ((fd = create_anonymous_file(size)) < 0) {
    return 0;
  }

  if ((display->fb = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
    return 0;
  }

  pool = wl_shm_create_pool(display->wlserv->shm, fd, size);
  display->buffer = wl_shm_pool_create_buffer(pool, 0, display->width, display->height,
                                              display->line_length, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  wl_surface_attach(display->surface, display->buffer, 0, 0);

  return 1;
}

static void destroy_shm_buffer(Display *display) {
  wl_surface_attach(display->surface, NULL, 0, 0);
  wl_buffer_destroy(display->buffer);
  munmap(display->fb, display->line_length * (display->height + DECORATION_MARGIN));
}

static u_int total_min_width(ui_window_t *win) {
  u_int count;
  u_int min_width;

  min_width = win->min_width + win->hmargin * 2;

  for (count = 0; count < win->num_of_children; count++) {
    if (win->children[count]->is_mapped) {
      /* XXX */
      min_width += total_min_width(win->children[count]);
    }
  }

  return min_width;
}

static u_int total_min_height(ui_window_t *win) {
  u_int count;
  u_int min_height;

  min_height = win->min_height + win->vmargin * 2;

  for (count = 0; count < win->num_of_children; count++) {
    if (win->children[count]->is_mapped) {
      /* XXX */
      min_height += total_min_height(win->children[count]);
    }
  }

  return min_height;
}

static u_int total_width_inc(ui_window_t *win) {
  u_int count;
  u_int width_inc;

  width_inc = win->width_inc;

  for (count = 0; count < win->num_of_children; count++) {
    if (win->children[count]->is_mapped) {
      u_int sub_inc;

      /*
       * XXX
       * we should calculate least common multiple of width_inc and sub_inc.
       */
      if ((sub_inc = total_width_inc(win->children[count])) > width_inc) {
        width_inc = sub_inc;
      }
    }
  }

  return width_inc;
}

static u_int total_height_inc(ui_window_t *win) {
  u_int count;
  u_int height_inc;

  height_inc = win->height_inc;

  for (count = 0; count < win->num_of_children; count++) {
    if (win->children[count]->is_mapped) {
      u_int sub_inc;

      /*
       * XXX
       * we should calculate least common multiple of width_inc and sub_inc.
       */
      if ((sub_inc = total_height_inc(win->children[count])) > height_inc) {
        height_inc = sub_inc;
      }
    }
  }

  return height_inc;
}

static int resize_display(ui_display_t *disp, u_int width, u_int height, int roundtrip) {
  int do_create;

  if (disp->width == width && disp->height == height) {
    return 0;
  }

  if (roundtrip) {
    /* Process pending shell_surface_configure events in advance. */
    wl_display_roundtrip(disp->display->wlserv->display);
  }

#ifdef __DEBUG
  bl_debug_printf("Resizing display from w %d h %d to w %d h %d.\n",
                  disp->width, disp->height, width, height);
#endif

#ifdef COMPAT_LIBVTE
  /* For the case of resizing a window which contains multiple tabs. */
  if (disp->display->buffer) {
    destroy_shm_buffer(disp->display);
    do_create = 1;
  } else {
    do_create = 0;
  }
#else
  destroy_shm_buffer(disp->display);
  do_create = 1;
#endif

  disp->width = width;
  disp->height = height;

  if (rotate_display) {
    disp->display->width = height;
    disp->display->height = width;
  } else {
    disp->display->width = width;
    disp->display->height = height;
  }

  if (do_create) {
    create_shm_buffer(disp->display);
  }

  return 1;
}

#ifndef COMPAT_LIBVTE
static void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface,
                               uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

static int check_resize(u_int old_width, u_int old_height, int32_t *new_width, int32_t *new_height,
                        u_int min_width, u_int min_height, u_int width_inc, u_int height_inc,
                        int check_inc) {
  u_int diff;

  if (old_width < *new_width) {
    diff = ((*new_width - old_width) / width_inc) * width_inc;
    if (check_inc || diff < width_inc) {
      *new_width = old_width + diff;
    }
  } else if (*new_width < old_width) {
    diff = ((old_width - *new_width) / width_inc) * width_inc;
    if (!check_inc && diff >= width_inc) {
      diff = old_width - *new_width;
    }

    if (old_width < min_width + diff) {
      *new_width = min_width;
    } else {
      *new_width = old_width - diff;
    }
  }

  if (old_height < *new_height) {
    diff = ((*new_height - old_height) / height_inc) * height_inc;
    if (check_inc || diff < height_inc) {
      *new_height = old_height + diff;
    }
  } else if (*new_height < old_height) {
    diff = ((old_height - *new_height) / height_inc) * height_inc;
    if (!check_inc && diff >= height_inc) {
      diff = old_height - *new_height;
    }

    if (old_height < min_height + diff) {
      *new_height = min_height;
    } else {
      *new_height = old_height - diff;
    }
  }

  if (old_width == *new_width && old_height == *new_height) {
    return 0;
  } else {
    return 1;
  }
}

/* XXX I don't know why, but edges is always 0 even if resizing by dragging an edge. */
static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface,
                                    uint32_t edges, int32_t width, int32_t height) {
  ui_display_t *disp = data;

#ifdef __DEBUG
  bl_debug_printf("Configuring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  if (check_resize(disp->display->width, disp->display->height, &width, &height,
                   total_min_width(disp->roots[0]), total_min_height(disp->roots[0]),
                   total_width_inc(disp->roots[0]), total_height_inc(disp->roots[0]),
                   disp->display->is_resizing)) {
#ifdef __DEBUG
    bl_msg_printf("-> modified size w %d h %d\n", width, height);
#endif

    if (rotate_display) {
      resize_display(disp, height, width, 0);
    } else {
      resize_display(disp, width, height, 0);
    }

    ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height, NOTIFY_TO_MYSELF);
  }
}

static void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface) {
}

static const struct wl_shell_surface_listener shell_surface_listener = {
  shell_surface_ping,
  shell_surface_configure,
  shell_surface_popup_done
};
#endif

static void data_offer_offer(void *data, struct wl_data_offer *offer, const char *type) {
#ifdef __DEBUG
  bl_debug_printf("DATA OFFER %s\n", type);
#endif
}

static void data_offer_source_action(void *data, struct wl_data_offer *data_offer,
                                     uint32_t source_actions) {
#ifdef __DEBUG
  bl_debug_printf("SOURCE ACTION %d\n", source_actions);
#endif
}

static void data_offer_action(void *data, struct wl_data_offer *data_offer,
                              uint32_t dnd_action) {
#ifdef __DEBUG
  bl_debug_printf("DND ACTION %d\n", dnd_action);
#endif
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_source_action,
  data_offer_action
};

static void data_device_data_offer(void *data, struct wl_data_device *data_device,
                                   struct wl_data_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("DND OFFER %p\n", offer);
#endif

  wl_data_offer_add_listener(offer, &data_offer_listener, wlserv);
  wlserv->dnd_offer = offer;
}

static void data_device_enter(void *data, struct wl_data_device *data_device,
                              uint32_t serial, struct wl_surface *surface,
                              wl_fixed_t x_w, wl_fixed_t y_w,
                              struct wl_data_offer *offer) {
#ifdef __DEBUG
  bl_debug_printf("DATA DEVICE ENTER %p\n", offer);
#endif

  wl_data_offer_accept(offer, serial, "text/uri-list");

  /* wl_data_device_manager_get_version() >= 3 */
  wl_data_offer_set_actions(offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
}

static void data_device_leave(void *data, struct wl_data_device *data_device) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("DATA DEVICE LEAVE\n");
#endif

  if (wlserv->dnd_offer) {
    wl_data_offer_destroy(wlserv->dnd_offer);
    wlserv->dnd_offer = NULL;
  }
  if (wlserv->sel_offer) {
    wl_data_offer_destroy(wlserv->sel_offer);
    wlserv->sel_offer = NULL;
  }
}

static void data_device_motion(void *data, struct wl_data_device *data_device,
                               uint32_t time, wl_fixed_t x_w, wl_fixed_t y_w) {
}

static void receive_data(ui_wlserv_t *wlserv, struct wl_data_offer *offer, const char *mime) {
  int fds[2];
  if (pipe(fds) == 0) {
    u_char buf[512];
    ssize_t len;
    ui_display_t *disp = surface_to_display(wlserv->current_kbd_surface);

    wl_data_offer_receive(offer, mime, fds[1]);
    wl_display_flush(wlserv->display);
    close(fds[1]);

    while ((len = read(fds[0], buf, sizeof(buf))) > 0) {
#ifdef __DEBUG
      if (len == sizeof(buf)) {
        buf[len - 1] = '\0';
      } else {
        buf[len] = '\0';
      }
      bl_debug_printf("RECEIVE %d %s\n", len, buf);
#endif
      if (disp->roots[0]->utf_selection_notified) {
        (*disp->roots[0]->utf_selection_notified)(disp->roots[0], buf, len);
      }
    }
    close(fds[0]);
  }
}

static void data_device_drop(void *data, struct wl_data_device *data_device) {
  ui_wlserv_t *wlserv = data;

#ifdef  __DEBUG
  bl_debug_printf("DROPPED\n");
#endif

  receive_data(data, wlserv->dnd_offer, "text/uri-list");
}

static void data_device_selection(void *data, struct wl_data_device *wl_data_device,
                                  struct wl_data_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("SELECTION OFFER %p\n", offer);
#endif

  if (wlserv->sel_offer) {
    wl_data_offer_destroy(wlserv->sel_offer);
    wlserv->sel_offer = NULL;
  }
  wlserv->sel_offer = offer;
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
};

static void data_source_target(void *data, struct wl_data_source *source, const char *mime) {
}

static void data_source_send(void *data, struct wl_data_source *source, const char *mime,
                             int32_t fd) {
  ui_display_t *disp = data;
  disp->display->wlserv->sel_fd = fd;

#ifdef __DEBUG
  bl_debug_printf("SOURCE SEND %s\n", mime);
#endif

  if (disp->selection_owner->utf_selection_requested) {
    /* utf_selection_requested() calls ui_display_send_text_selection() */
    (*disp->selection_owner->utf_selection_requested)(disp->selection_owner, NULL, 0);
  }
}

static void data_source_cancelled(void *data, struct wl_data_source *source) {
  ui_display_t *disp = data;
  ui_wlserv_t *wlserv = disp->display->wlserv;

#ifdef __DEBUG
  bl_debug_printf("SOURCE CANCEL %p %p\n", source, wlserv->sel_source);
#endif

  if (source == wlserv->sel_source) {
    ui_display_clear_selection(disp, disp->selection_owner);
  }
}

void data_source_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source) {
}

void data_source_dnd_finished(void *data, struct wl_data_source *wl_data_source) {
}

void data_source_action(void *data, struct wl_data_source *wl_data_source, uint32_t dnd_action) {
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled,
  data_source_dnd_drop_performed,
  data_source_dnd_finished,
  data_source_action,
};

static ui_wlserv_t *open_wl_display(char *name) {
  ui_wlserv_t *wlserv;

  if (!(wlserv = calloc(1, sizeof(ui_wlserv_t) + sizeof(*wlserv->xkb)))) {
    return NULL;
  }

  wlserv->xkb = wlserv + 1;

  if ((wlserv->display = wl_display_connect(name)) == NULL) {
    bl_error_printf("Couldn't open display %s.\n", name);
    free(wlserv);

    return NULL;
  }

  /* set close-on-exec flag on the socket connected to X. */
  bl_file_set_cloexec(wl_display_get_fd(wlserv->display));

  wlserv->registry = wl_display_get_registry(wlserv->display);
  wl_registry_add_listener(wlserv->registry, &registry_listener, wlserv);

  wl_display_roundtrip(wlserv->display);

  wlserv->keyboard = wl_seat_get_keyboard(wlserv->seat);
  wl_keyboard_add_listener(wlserv->keyboard, &keyboard_listener, wlserv);
  wlserv->xkb->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  if ((wlserv->cursor_theme = wl_cursor_theme_load(NULL, 32, wlserv->shm))) {
    /* The order should be the one of wl_shell_surface_resize. */
    char *names[] = { "xterm", "n-resize", "s-resize", "w-resize", "nw-resize",
                      "sw-resize", "e-resize", "ne-resize", "se-resize", "move" };
    int count;
    int has_cursor = 0;

    for (count = 0; count < sizeof(names) / sizeof(names[0]); count++) {
      if ((wlserv->cursor[count] = wl_cursor_theme_get_cursor(wlserv->cursor_theme,
                                                              names[count]))) {
        has_cursor = 1;
      }
    }

    if (has_cursor) {
      wlserv->cursor_surface = wl_compositor_create_surface(wlserv->compositor);
    } else {
      wl_cursor_theme_destroy(wlserv->cursor_theme);
      wlserv->cursor_theme = NULL;
    }
  }

  wlserv->pointer = wl_seat_get_pointer(wlserv->seat);
  wl_pointer_add_listener(wlserv->pointer, &pointer_listener, wlserv);

  wlserv->data_device = wl_data_device_manager_get_data_device(wlserv->data_device_manager,
                                                               wlserv->seat);
  wl_data_device_add_listener(wlserv->data_device, &data_device_listener, wlserv);

  wlserv->sel_fd = -1;

  return wlserv;
}

static void close_wl_display(ui_wlserv_t *wlserv) {
#ifdef __DEBUG
  bl_debug_printf("Closing wldisplay.\n");
#endif

#if 0
  if (wlserv->dnd_offer) {
    wl_data_offer_destroy(wlserv->dnd_offer);
    wlserv->dnd_offer = NULL;
  }
  if (wlserv->sel_offer) {
    wl_data_offer_destroy(wlserv->sel_offer);
    wlserv->sel_offer = NULL;
  }
#endif
  if (wlserv->sel_source) {
    wl_data_source_destroy(wlserv->sel_source);
    wlserv->sel_source = NULL;
  }

#ifdef COMPAT_LIBVTE
  wl_subcompositor_destroy(wlserv->subcompositor);
#endif
  wl_output_destroy(wlserv->output);
  wl_seat_destroy(wlserv->seat);
  wl_compositor_destroy(wlserv->compositor);
#ifndef COMPAT_LIBVTE
  wl_registry_destroy(wlserv->registry);
#endif
  wl_keyboard_destroy(wlserv->keyboard);

  xkb_state_unref(wlserv->xkb->state);
  xkb_keymap_unref(wlserv->xkb->keymap);
  xkb_context_unref(wlserv->xkb->ctx);
  if (wlserv->cursor_surface) {
    wl_surface_destroy(wlserv->cursor_surface);
  }
  if (wlserv->cursor_theme) {
    wl_cursor_theme_destroy(wlserv->cursor_theme);
  }

  wl_pointer_destroy(wlserv->pointer);
  wl_data_device_destroy(wlserv->data_device);
  wl_data_device_manager_destroy(wlserv->data_device_manager);
#ifndef COMPAT_LIBVTE
  wl_display_disconnect(wlserv->display);

  free(wlserv);
#endif
}

static int close_display(ui_display_t *disp) {
  u_int count;

#ifdef __DEBUG
  bl_debug_printf("Closing ui_display_t\n");
#endif

  free(disp->name);

  for (count = 0; count < disp->num_of_roots; count++) {
    ui_window_unmap(disp->roots[count]);
    ui_window_final(disp->roots[count]);
  }

  free(disp->roots);

  ui_picture_display_closed(disp->display);

#ifdef COMPAT_LIBVTE
  if (disp->display->subsurface) {
    /* disp->display->buffer may have been destroyed in ui_display_unmap(). */
    if (disp->display->buffer) {
      destroy_shm_buffer(disp->display);
      disp->display->buffer = NULL;
    }
    wl_subsurface_destroy(disp->display->subsurface);
    wl_surface_destroy(disp->display->surface);
  }
#else
  if (disp->display->shell_surface) {
    destroy_shm_buffer(disp->display);
    disp->display->buffer = NULL;
    wl_shell_surface_destroy(disp->display->shell_surface);
    wl_surface_destroy(disp->display->surface);
  }
#endif

  if (disp->display->wlserv->ref_count == 1) {
    u_int count;

    close_wl_display(disp->display->wlserv);

    for (count = 0; count < num_of_wlservs; count++) {
      if (disp->display->wlserv == wlservs[count]) {
        if (--num_of_wlservs == 0) {
          free(wlservs);
          wlservs = NULL;
        } else {
          wlservs[count] = wlservs[--num_of_wlservs];
        }
      }
    }
  } else {
    disp->display->wlserv->ref_count--;
  }

  free(disp);

  return 1;
}

static int flush_damage(Display *display) {
  if (display->damage_height > 0) {
    wl_surface_damage(display->surface, display->damage_x, display->damage_y,
                      display->damage_width, display->damage_height);
    display->damage_x = display->damage_y = display->damage_width = display->damage_height = 0;

    return 1;
  } else {

    return 0;
  }
}

static int cache_damage_h(Display *display, int x, int y, u_int width, u_int height) {
  if (x == display->damage_x && y == display->damage_y + display->damage_height &&
      width == display->damage_width) {
    display->damage_height += height;
    return 1;
  } else {
    return 0;
  }
}

static int cache_damage_v(Display *display, int x, int y, u_int width, u_int height) {
  if (y == display->damage_y && x == display->damage_x + display->damage_width &&
      height == display->damage_height) {
    display->damage_width += width;
    return 1;
  } else {
    return 0;
  }
}

static void damage_buffer(Display *display, int x, int y, u_int width, u_int height) {
  if (rotate_display ? !cache_damage_v(display, x, y, width, height) :
                       !cache_damage_h(display, x, y, width, height)) {
    if (display->damage_height > 0) {
      wl_surface_damage(display->surface, display->damage_x, display->damage_y,
                        display->damage_width, display->damage_height);
    }
    display->damage_x = x;
    display->damage_y = y;
    display->damage_width = width;
    display->damage_height = height;
  }
}

static int flush_display(Display *display) {
  if (flush_damage(display)) {
    wl_surface_commit(display->surface);

    return 1;
  } else {
    return 0;
  }
}

static void create_surface(ui_display_t *disp, int x, int y, u_int width, u_int height,
                           char *app_name) {
  Display *display = disp->display;
  ui_wlserv_t *wlserv = display->wlserv;

  display->surface = wl_compositor_create_surface(wlserv->compositor);

#ifdef COMPAT_LIBVTE
  display->subsurface = wl_subcompositor_get_subsurface(wlserv->subcompositor, display->surface,
                                                        display->parent_surface);
#else
  display->shell_surface = wl_shell_get_shell_surface(wlserv->shell, display->surface);
  wl_shell_surface_set_class(display->shell_surface, app_name);

  if (display->parent) {
#ifdef __DEBUG
    bl_debug_printf("Move display (set transient) at %d %d\n", x, y);
#endif
    wl_shell_surface_set_transient(display->shell_surface,
                                   display->parent->display->surface, x, y,
                                   WL_SHELL_SURFACE_TRANSIENT_INACTIVE);
  } else {
    wl_surface_add_listener(display->surface, &surface_listener, wlserv);
    wl_shell_surface_add_listener(display->shell_surface, &shell_surface_listener, disp);
    wl_shell_surface_set_toplevel(display->shell_surface);
    wl_shell_surface_set_title(display->shell_surface, app_name);
  }
#endif

  disp->width = width;
  disp->height = height;
  if (rotate_display) {
    display->width = height;
    display->height = width;
  } else {
    display->width = width;
    display->height = height;
  }
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, u_int depth) {
  u_int count;
  ui_display_t *disp;
  ui_wlserv_t *wlserv = NULL;
  void *p;
  struct rgb_info rgbinfo = {0, 0, 0, 16, 8, 0};
  static int added_auto_repeat;

  if (!(disp = calloc(1, sizeof(ui_display_t) + sizeof(Display)))) {
    return NULL;
  }

  disp->display = disp + 1;

  if ((p = realloc(displays, sizeof(ui_display_t*) * (num_of_displays + 1))) == NULL) {
    free(disp);

    return NULL;
  }

  displays = p;
  displays[num_of_displays] = disp;

  for (count = 0; count < num_of_displays; count++) {
    if (strcmp(displays[count]->name, disp_name) == 0) {
      disp->selection_owner = displays[count]->selection_owner;
      wlserv = displays[count]->display->wlserv;
      break;
    }
  }

  if (wlserv == NULL) {
    if ((wlserv = open_wl_display(disp_name)) == NULL) {
      free(disp);

      return NULL;
    }

    if ((p = realloc(wlservs, sizeof(ui_wlserv_t*) * (num_of_wlservs + 1))) == NULL) {
      close_wl_display(wlserv);
      free(disp);

      return NULL;
    }

    wlservs = p;
    wlservs[num_of_wlservs++] = wlserv;
  }

  disp->display->wlserv = wlserv;
  disp->name = strdup(disp_name);
  disp->display->rgbinfo = rgbinfo;
  disp->display->bytes_per_pixel = 4;
  disp->depth = 32;

  wlserv->ref_count++;
  num_of_displays++;

  ui_picture_display_opened(disp->display);

  if (!added_auto_repeat) {
    ui_event_source_add_fd(-10, auto_repeat);
    added_auto_repeat = 1;
  }

  return disp;
}

int ui_display_close(ui_display_t *disp) {
  u_int count;

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count] == disp) {
      close_display(disp);
      if (--num_of_displays == 0) {
        free(displays);
        displays = NULL;
      } else {
        displays[count] = displays[num_of_displays];
      }

#ifdef DEBUG
      bl_debug_printf("wayland display connection closed.\n");
#endif

      return 1;
    }
  }

  return 0;
}

int ui_display_close_all(void) {
  while (num_of_displays > 0) {
    close_display(displays[0]);
  }

  return 1;
}

ui_display_t **ui_get_opened_displays(u_int *num) {
  *num = num_of_displays;

  return displays;
}

int ui_display_fd(ui_display_t *disp) { return wl_display_get_fd(disp->display->wlserv->display); }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, Window parent_window) {
  void *p;

  if (parent_window) {
#ifdef COMPAT_LIBVTE
    disp = add_root_to_display(disp, root, parent_window);
#endif
  } else {
    if (disp->num_of_roots > 0) {
      /* XXX Input Method */
      ui_display_t *parent = disp;
      disp = ui_display_open(disp->name, disp->depth);
      disp->display->parent = parent;
#ifdef COMPAT_LIBVTE
      parent_window = disp->display->parent_surface = parent->display->parent_surface;
#endif
    }
  }

  root->parent_window = parent_window;

  if ((p = realloc(disp->roots, sizeof(ui_window_t *) * (disp->num_of_roots + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  disp->roots = p;

  root->disp = disp;
  root->parent = NULL;

  root->gc = disp->gc;
  root->x = 0;
  root->y = 0;

  if (app_name) {
    root->app_name = app_name;
  }

  /*
   * root is added to disp->roots before ui_window_show() because
   * ui_display_get_group_leader() is called in ui_window_show().
   */
  disp->roots[disp->num_of_roots++] = root;

  create_surface(disp, x, y, ACTUAL_WIDTH(root), ACTUAL_HEIGHT(root), root->app_name);

#ifdef COMPAT_LIBVTE
  if (!disp->display->parent) {
    /* do nothing until ui_display_map() except input method */
  } else
#endif
  {
    create_shm_buffer(disp->display);
    ui_window_show(root, hint);
  }

  return 1;
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_of_roots; count++) {
    if (disp->roots[count] == root) {
#ifdef COMPAT_LIBVTE
      if (disp->display->parent) {
        /* XXX input method (do nothing) */
      } else {
        disp = remove_root_from_display(disp, root);
      }
      count = 0;
#endif

      /* Don't switching on or off screen in exiting. */
      ui_window_unmap(root);
      ui_window_final(root);
      disp->num_of_roots--;

      if (count == disp->num_of_roots) {
        disp->roots[count] = NULL;

        if (disp->num_of_roots == 0
#ifndef COMPAT_LIBVTE
            && disp->display->parent /* XXX input method alone */
#endif
            ) {
          ui_display_close(disp);
        }
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
#ifdef COMPAT_LIBVTE
  static int display_idling_wait = 4;

  if (--display_idling_wait > 0) {
    goto skip;
  }
  display_idling_wait = 4;
#endif

  for (count = 0; count < disp->num_of_roots; count++) {
#ifdef COMPAT_LIBVTE
    if (disp->roots[count]->disp->display->buffer)
#endif
    {
      ui_window_idling(disp->roots[count]);
    }
  }

  ui_display_sync(disp);

skip:
  auto_repeat();
}

#ifndef COMPAT_LIBVTE
int ui_display_receive_next_event(ui_display_t *disp) {
  u_int count;

  ui_display_sync(disp);

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count]->display->wlserv == disp->display->wlserv) {
      if (displays[count] == disp) {
        return (wl_display_dispatch(disp->display->wlserv->display) != -1);
      } else {
        break;
      }
    }
  }

  return 0;
}

int ui_display_receive_next_event_singly(ui_display_t *disp) {
  u_int count;

  ui_display_sync(disp);

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count]->display->wlserv == disp->display->wlserv) {
      return (wl_display_dispatch(disp->display->wlserv->display) != -1);
    } else {
      break;
    }
  }

  return 0;
}
#endif

void ui_display_sync(ui_display_t *disp) {
#ifdef COMPAT_LIBVTE
  u_int count;
  int flushed = 0;

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count]->display->buffer) {
      flushed |= flush_display(displays[count]->display);
    }
  }

  if (flushed) {
    wl_display_flush(wlservs[0]->display);
  }
#else
  if (flush_display(disp->display)) {
    wl_display_flush(disp->display->wlserv->display);
  }
#endif
}

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_t *win) {
  ui_wlserv_t *wlserv = disp->display->wlserv;
  u_int count;

  if (disp->selection_owner) {
    ui_display_clear_selection(NULL, disp->selection_owner);
  }

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count]->display->wlserv == wlserv) {
      displays[count]->selection_owner = win;
    }
  }

  wlserv->sel_source = wl_data_device_manager_create_data_source(wlserv->data_device_manager);
  wl_data_source_add_listener(wlserv->sel_source, &data_source_listener, disp);
  wl_data_source_offer(wlserv->sel_source, "UTF8_STRING");
  wl_data_device_set_selection(wlserv->data_device,
                               wlserv->sel_source, wlserv->serial);

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp, /* NULL means all selection owner windows. */
                               ui_window_t *win) {
  u_int count;

  for (count = 0; count < num_of_displays; count++) {
    if ((disp == NULL || disp->display->wlserv == displays[count]->display->wlserv) &&
        (displays[count]->selection_owner == win)) {
      displays[count]->selection_owner = NULL;
      if (displays[count]->display->wlserv->sel_source) {
        wl_data_source_destroy(displays[count]->display->wlserv->sel_source);
        displays[count]->display->wlserv->sel_source = NULL;
      }
    }
  }

  if (win->selection_cleared) {
    (*win->selection_cleared)(win);
  }

  return 1;
}

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp) { return disp->modmap.map; }

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) {}

XID ui_display_get_group_leader(ui_display_t *disp) {
  if (disp->num_of_roots > 0) {
    return disp->roots[0]->my_window;
  } else {
    return None;
  }
}

void ui_display_rotate(int rotate) { rotate_display = rotate; }

/* Don't call this internally from ui_display.c. Call resize_display(..., 0) instead. */
int ui_display_resize(ui_display_t *disp, u_int width, u_int height) {
  return resize_display(disp, width, height, 1);
}

int ui_display_move(ui_display_t *disp, int x, int y) {
  Display *display = disp->display;

#ifdef COMPAT_LIBVTE
  if (!display->buffer || (display->x == x && display->y == y)) {
    return 0;
  }

  display->x = x;
  display->y = y;

  if (display->subsurface) {
#ifdef __DEBUG
    bl_debug_printf("Move display (on libvte) at %d %d\n", x, y);
#endif

    wl_subsurface_set_position(display->subsurface, x, y);
    if (display->parent) {
      /*
       * Input method window.
       * refrect wl_subsurface_set_position() immediately.
       */
      wl_surface_commit(display->parent_surface);
    }
  }
#else
  if (display->parent) {
    /* input method window */
#ifdef __DEBUG
    bl_debug_printf("Move display (set transient) at %d %d\n", x, y);
#endif
    wl_shell_surface_set_transient(display->shell_surface,
                                   display->parent->display->surface, x, y,
                                   WL_SHELL_SURFACE_TRANSIENT_INACTIVE);
  }
#endif

  return 1;
}

u_long ui_display_get_pixel(ui_display_t *disp, int x, int y) {
  u_char *fb;
  u_long pixel;

#ifdef COMPAT_LIBVTE
  if (!disp->display->buffer) {
    return 0;
  }
#endif

  if (rotate_display) {
    int tmp;

    if (rotate_display > 0) {
      tmp = x;
      x = disp->height - y - 1;
      y = tmp;
    } else {
      tmp = x;
      x = y;
      y = disp->width - tmp - 1;
    }
  }

  fb = get_fb(disp->display, x, y);
  pixel = TOINT32(fb);

  return pixel;
}

void ui_display_put_image(ui_display_t *disp, int x, int y, u_char *image, size_t size,
                          int need_fb_pixel) {
  Display *display = disp->display;

#ifdef COMPAT_LIBVTE
  if (!display->buffer) {
    return;
  }
#endif

  if (rotate_display) {
    /* Display is rotated. */

    u_char *fb;
    int tmp;
    int line_length;
    size_t count;

    tmp = x;
    if (rotate_display > 0) {
      x = disp->height - y - 1;
      y = tmp;
      line_length = display->line_length;
    } else {
      x = y;
      y = disp->width - tmp - 1;
      line_length = -display->line_length;
    }

    fb = get_fb(display, x, y);

    if (display->bytes_per_pixel == 2) {
      size /= 2;
      for (count = 0; count < size; count++) {
        *((u_int16_t*)fb) = ((u_int16_t*)image)[count];
        fb += line_length;
      }
    } else /* if (display->bytes_per_pixel == 4) */ {
      size /= 4;
      for (count = 0; count < size; count++) {
        *((u_int32_t*)fb) = ((u_int32_t*)image)[count];
        fb += line_length;
      }
    }

    damage_buffer(display, x, rotate_display > 0 ? y : y - size + 1, 1 , size);
  } else {
    memcpy(get_fb(display, x, y), image, size);

    damage_buffer(display, x, y, size, 1);
  }
}

void ui_display_copy_lines(ui_display_t *disp, int src_x, int src_y, int dst_x, int dst_y,
                           u_int width, u_int height) {
  Display *display = disp->display;
  u_char *src;
  u_char *dst;
  u_int copy_len;
  u_int count;
  size_t line_length;

  if (rotate_display) {
    int tmp;

    if (rotate_display > 0) {
      tmp = src_x;
      src_x = disp->height - src_y - height;
      src_y = tmp;

      tmp = dst_x;
      dst_x = disp->height - dst_y - height;
      dst_y = tmp;
    } else {
      tmp = src_x;
      src_x = src_y;
      src_y = disp->width - tmp - width;

      tmp = dst_x;
      dst_x = dst_y;
      dst_y = disp->width - tmp - width;
    }

    tmp = height;
    height = width;
    width = tmp;
  }

  copy_len = width * display->bytes_per_pixel;
  line_length = display->line_length;

  if (src_y <= dst_y) {
    src = get_fb(display, src_x, src_y + height - 1);
    dst = get_fb(display, dst_x, dst_y + height - 1);

    if (src_y == dst_y) {
      for (count = 0; count < height; count++) {
        memmove(dst, src, copy_len);
        dst -= line_length;
        src -= line_length;
      }
    } else {
      for (count = 0; count < height; count++) {
        memcpy(dst, src, copy_len);
        dst -= line_length;
        src -= line_length;
      }
    }
  } else {
    src = get_fb(display, src_x, src_y);
    dst = get_fb(display, dst_x, dst_y);

    for (count = 0; count < height; count++) {
      memcpy(dst, src, copy_len);
      dst += line_length;
      src += line_length;
    }
  }

  damage_buffer(display, dst_x, dst_y, width, height);
}

void ui_display_request_text_selection(ui_display_t *disp) {
  if (disp->selection_owner) {
    XSelectionRequestEvent ev;
    ev.type = 0;
    ev.target = disp->roots[0];
    if (disp->selection_owner->utf_selection_requested) {
      /* utf_selection_requested() calls ui_display_send_text_selection() */
      (*disp->selection_owner->utf_selection_requested)(disp->selection_owner, &ev, 0);
    }
  } else if (disp->display->wlserv->sel_offer) {
    receive_data(disp->display->wlserv, disp->display->wlserv->sel_offer, "UTF8_STRING");
  }
}

void ui_display_send_text_selection(ui_display_t *disp, XSelectionRequestEvent *ev,
                                    u_char *sel_data, size_t sel_len) {
  if (disp->display->wlserv->sel_fd != -1) {
    write(disp->display->wlserv->sel_fd, sel_data, sel_len);
    close(disp->display->wlserv->sel_fd);
    disp->display->wlserv->sel_fd = -1;
  } else if (ev && ev->target->utf_selection_notified) {
    (*ev->target->utf_selection_notified)(ev->target, sel_data, sel_len);
  }
}

u_char ui_display_get_char(KeySym ksym) {
    char buf[10];
    int len;

    if ((len = xkb_keysym_to_utf8(ksym, buf, sizeof(buf))) > 0) {
      return buf[0];
    } else {
      return 0;
    }
}

void ui_display_logical_to_physical_coordinates(ui_display_t *disp, int *x, int *y) {
  if (rotate_display) {
    int tmp = *y;
    if (rotate_display > 0) {
      *y = *x;
      *x = disp->display->width - tmp - 1;
    } else {
      *y = disp->display->height - *x - 1;
      *x = tmp;
    }
  }

#ifdef COMPAT_LIBVTE
  *x += disp->display->x;
  *y += disp->display->y;
#endif
}

void ui_display_set_title(ui_display_t *disp, const u_char *name) {
#ifndef COMPAT_LIBVTE
  wl_shell_surface_set_title(disp->display->shell_surface, name);
#endif
}

#ifdef COMPAT_LIBVTE
/*
 * Only one ui_wlserv_t exists on libvte compat library.
 * disp in vte.c shares one ui_wlserv_t but doesn't have shm buffer and surface.
 * displays in ui_display.c share one ui_wlserv_t and have each shm buffer and surface.
 */
void ui_display_init_wlserv(ui_wlserv_t *wlserv) {
  wl_registry_add_listener(wlserv->registry, &registry_listener, wlserv);
  wl_display_roundtrip(wlserv->display);

  wlserv->keyboard = wl_seat_get_keyboard(wlserv->seat);
  wlserv->xkb->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  wl_keyboard_add_listener(wlserv->keyboard, &keyboard_listener, wlserv);

  wlserv->pointer = wl_seat_get_pointer(wlserv->seat);
  wl_pointer_add_listener(wlserv->pointer, &pointer_listener, wlserv);

  wlserv->data_device = wl_data_device_manager_get_data_device(wlserv->data_device_manager,
                                                               wlserv->seat);
  wl_data_device_add_listener(wlserv->data_device, &data_device_listener, wlserv);

  wlserv->sel_fd = -1;
}

void ui_display_map(ui_display_t *disp) {
  if (!disp->display->buffer) {
    ui_window_t *win;

#ifdef __DEBUG
    bl_debug_printf("Creating new shm buffer.\n");
#endif
    /*
     * wl_subsurface should be desynchronized between create_shm_buffer() and
     * destroy_shm_buffer() to draw screen correctly.
     */
    wl_subsurface_set_desync(disp->display->subsurface);

    create_shm_buffer(disp->display);

    /*
     * gnome-terminal doesn't invoke FocusIn event in switching tabs, so
     * it is necessary to set current_kbd_surface and is_focused manually.
     */
    disp->display->wlserv->current_kbd_surface = disp->display->surface;
    if ((win = search_inputtable_window(NULL, disp->roots[0]))) {
      /*
       * ui_window_set_input_focus() is not called here, because it is not necessary to
       * call win->window_focused() which is called in ui_window_set_input_focus().
       */
      win->inputtable = win->is_focused = 1;
    }

    /*
     * shell_surface_configure() might have been already received and
     * the size of ui_display_t and ui_window_t might mismatch.
     * So, ui_window_show() doesn't show anything.
     */
    disp->roots[0]->is_mapped = 0;
    ui_window_show(disp->roots[0], 0);
    disp->roots[0]->is_mapped = 1;
    if (!ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height,
                                      NOTIFY_TO_MYSELF)) {
      ui_window_update_all(disp->roots[0]);
    }
  }
}

void ui_display_unmap(ui_display_t *disp) {
  if (disp->display->buffer) {
#ifdef __DEBUG
    bl_debug_printf("Destroying shm buffer.\n");
#endif
    destroy_shm_buffer(disp->display);
    /*
     * Without calling wl_subsurface_set_sync() before wl_surface_commit(),
     * is_surface_effectively_shnchronized(surface->sub.parent) in meta-wayland-surface.c
     * (mutter-3.22.3) causes segfault because surface->sub.parent can be NULL before
     * ui_display_unmap() is called.
     */
    wl_subsurface_set_sync(disp->display->subsurface);
    wl_surface_commit(disp->display->surface);
    disp->display->buffer = NULL;
    disp->display->x = disp->display->y = 0;
  }
}
#endif /* COMPAT_LIBVTE */
