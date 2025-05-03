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
#include <pobl/bl_util.h> /* BL_SWAP */

#include "../ui_window.h"
#include "../ui_picture.h"
#include "../ui_imagelib.h"
#include "../ui_event_source.h"

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

static u_int num_wlservs;
static ui_wlserv_t **wlservs;
static u_int num_displays;
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

  if (posix_fallocate(fd, 0, size) != 0) {
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
  for (count = 0; count < win->num_children; count++) {
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

  for (count = 0; count < win->num_children; count++) {
    candidate = search_inputtable_window(candidate, win->children[count]);
  }

  return candidate;
}

/*
 * x and y are rotated values.
 */
static inline ui_window_t *get_window(ui_window_t *win, int x, int y) {
  u_int count;

  for (count = 0; count < win->num_children; count++) {
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

  for (count = 0; count < num_displays; count++) {
    if (surface == displays[count]->display->surface) {
      return displays[count];
    }
  }

  return NULL;
}

#ifdef COMPAT_LIBVTE
static ui_display_t *parent_surface_to_display(struct wl_surface *surface) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->display->buffer &&
        surface == displays[count]->display->parent_surface) {
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
  u_int count;

  if (!(p = realloc(displays, sizeof(ui_display_t*) * (num_displays + 1)))) {
    return NULL;
  }
  displays = p;

  if (wlservs == NULL) {
    if (!(wlservs = malloc(sizeof(ui_wlserv_t*)))) {
      return NULL;
    }
    wlservs[0] = wlserv;
    num_wlservs = 1;
  }

  if (!(new = malloc(sizeof(ui_display_t) + sizeof(Display)))) {
    return NULL;
  }

  memcpy(new, disp, sizeof(ui_display_t));
  new->display = (Display*)(new + 1);
  new->name = strdup(disp->name);

  memcpy(new->display, disp->display, sizeof(Display));

  new->display->parent_surface = parent_surface;

  for (count = 0; count < num_displays; count++) {
    if (strcmp(displays[count]->name, new->name) == 0) {
      new->selection_owner = displays[count]->selection_owner;
      new->clipboard_owner = displays[count]->clipboard_owner;
      break;
    }
  }

  displays[num_displays++] = new;

  if ((new->roots = malloc(sizeof(ui_window_t*)))) {
    new->roots[0] = root;
    new->num_roots = 1;
  } else {
    new->num_roots = 0;
  }

  wlserv->ref_count++;

  return new;
}

static ui_display_t *remove_root_from_display(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
    if (disp->roots[count] == root) {
      disp->roots[count] = disp->roots[--disp->num_roots];
      break;
    }
  }

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->roots[0] == root) {
      return displays[count];
    }
  }

  return NULL;
}
#endif

static void seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps);

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
#ifdef __DEBUG
  bl_debug_printf("seat_name %s\n", name);
#endif
}

static const struct wl_seat_listener seat_listener = {
  seat_capabilities,
  seat_name,
};

#ifdef XDG_SHELL
static void xdg_shell_ping(void *data, struct xdg_wm_base *xdg_shell, uint32_t serial) {
  xdg_wm_base_pong(xdg_shell, serial);
}

static const struct xdg_wm_base_listener xdg_shell_listener = {
  xdg_shell_ping
};
#endif

#ifdef ZXDG_SHELL_V6
static void zxdg_shell_ping(void *data, struct zxdg_shell_v6 *zxdg_shell, uint32_t serial) {
  zxdg_shell_v6_pong(zxdg_shell, serial);
}

static const struct zxdg_shell_v6_listener zxdg_shell_listener = {
  zxdg_shell_ping
};
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
  /* For sway 1.0 which doesn't support wl_surface. (supports only zxdg-shell) */
  else if (strcmp(interface, "wl_shell") == 0) {
    wlserv->shell_type = 1;
  } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
    if (wlserv->shell_type == 0 /* Not determined */) {
      wlserv->shell_type = 2;
    }
  } else if (strcmp(interface, "xdg_shell") == 0) {
    if (wlserv->shell_type == 0 /* Not determined */ || wlserv->shell_type == 2 /* zxdg_shell */) {
      /* Prefer xdg_shell to zxdg_shell */
      wlserv->shell_type = 3;
    }
  }
#else /* COMPAT_LIBVTE */
  else if (strcmp(interface, "wl_shell") == 0) {
    wlserv->shell = wl_registry_bind(registry, name, &wl_shell_interface, 1);

#ifdef ZXDG_SHELL_V6
    if (wlserv->zxdg_shell) {
      zxdg_shell_v6_destroy(wlserv->zxdg_shell);
      wlserv->zxdg_shell = NULL;
    }
#endif
#ifdef XDG_SHELL
    if (wlserv->xdg_shell) {
      xdg_wm_base_destroy(wlserv->xdg_shell);
      wlserv->xdg_shell = NULL;
    }
#endif
  }
#ifdef ZXDG_SHELL_V6
  else if (strcmp(interface, "zxdg_shell_v6") == 0) {
#ifdef XDG_SHELL
    if (wlserv->xdg_shell) {
      /* do nothing (prefer xdg_shell) */
    } else
#endif
    if (wlserv->shell == NULL) {
      wlserv->zxdg_shell = wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(wlserv->zxdg_shell, &zxdg_shell_listener, NULL);
    }
  }
#endif
#ifdef XDG_SHELL
  else if (strcmp(interface, "xdg_wm_base") == 0) {
#ifdef ZXDG_SHELL_V6
    if (wlserv->zxdg_shell) {
      zxdg_shell_v6_destroy(wlserv->zxdg_shell);
      wlserv->zxdg_shell = NULL;
    }
#endif
    if (wlserv->shell == NULL) {
      wlserv->xdg_shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
      xdg_wm_base_add_listener(wlserv->xdg_shell, &xdg_shell_listener, NULL);
    }
  }
#endif
#endif /* COMPAT_LIBVTE */
  else if (strcmp(interface, "wl_shm") == 0) {
    wlserv->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, "wl_seat") == 0) {
    wlserv->seat = wl_registry_bind(registry, name,
                                    &wl_seat_interface, 4); /* 4 is for keyboard_repeat_info */
    wl_seat_add_listener(wlserv->seat, &seat_listener, wlserv);
  } else if (strcmp(interface, "wl_output") == 0) {
    wlserv->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    wlserv->data_device_manager = wl_registry_bind(registry, name,
                                                   &wl_data_device_manager_interface, 3);
  }
#ifdef GTK_PRIMARY
  else if (strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    if (wlserv->zxsel_device_manager == NULL) {
      wlserv->gxsel_device_manager =
        wl_registry_bind(registry, name, &gtk_primary_selection_device_manager_interface, 1);
    }
  }
#endif
#ifdef ZWP_PRIMARY
  else if (strcmp(interface, "zwp_primary_selection_device_manager_v1") == 0) {
    wlserv->zxsel_device_manager =
      wl_registry_bind(registry, name, &zwp_primary_selection_device_manager_v1_interface, 1);
#ifdef GTK_PRIMARY
    if (wlserv->gxsel_device_manager) {
      gtk_primary_selection_device_manager_destroy(wlserv->gxsel_device_manager);
      wlserv->gxsel_device_manager = NULL;
    }
#endif
  }
#endif
#ifndef COMPAT_LIBVTE
  else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
    wlserv->decoration_manager =
      wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
  }
#endif
  else if (strcmp(interface, "zwp_text_input_manager_v3") == 0) {
    wlserv->text_input_manager =
      wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1);
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
      ui_window_receive_event(win, (XEvent*)ev);
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

  for (count = 0; count < num_wlservs; count++) {
    /* If prev_kev.type > 0, kbd_repeat_wait is always greater than 0. */
    if (wlservs[count]->prev_kev.type && /* wlservs[count]->kbd_repeat_wait > 0 && */
        --wlservs[count]->kbd_repeat_wait == 0) {
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
      if (kbd_repeat_N < KEY_REPEAT_UNIT / 2) {
        wlservs[count]->kbd_repeat_wait = 1;
      } else {
        wlservs[count]->kbd_repeat_wait = (kbd_repeat_N + KEY_REPEAT_UNIT / 2) / KEY_REPEAT_UNIT;
      }
    }
  }
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys) {
  ui_display_t *disp = surface_to_display(surface);
  ui_wlserv_t *wlserv = data;
  ui_window_t *win;

#ifdef COMPAT_LIBVTE
  if (!disp || disp->num_roots == 0) {
    wlserv->current_kbd_surface = surface;

    return;
  }
#endif

  if (disp->display->parent) {
    /* XXX input method */
    disp = disp->display->parent;
    surface = disp->display->surface;
  }

  wlserv->current_kbd_surface = surface;

#ifdef __DEBUG
  bl_debug_printf("keyboard_enter %p\n", surface);
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
  bl_debug_printf("keyboard_leave %p\n", surface);
#endif

#if 0
  wlserv->prev_kev.type = 0;
#endif

#ifdef COMPAT_LIBVTE
  if (!disp || disp->num_roots == 0) {
    return;
  }
#endif

  if (wlserv->current_kbd_surface == surface) {
    u_int count;

    for (count = 0; count < num_displays; count++) {
      if (displays[count]->display->parent == disp) {
        /*
         * Don't send FocusOut event to the surface which has an input method surface
         * as transient one, because the surface might receive this event by creating
         * and showing the input method surface.
         * As a secondary effect, the surface can't receive FocusOut event as long as
         * the input method surface is active.
         */
        return;
      }
    }

    wlserv->current_kbd_surface = NULL;
  }

  /* surface may have been already destroyed. So null check of disp is necessary. */
  if (disp && !disp->display->parent /* not input method window */ &&
      (win = search_focused_window(disp->roots[0]))) {
    XEvent ev;

#ifdef __DEBUG
    bl_debug_printf("UNFOCUSED %p\n", win);
#endif
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
    ev.keycode = key + 8;
    ev.state = wlserv->xkb->mods;
    ev.time = time;
    ev.utf8 = NULL;

    if (kbd_repeat_N == 0) {
      /* disable key repeating */
      wlserv->kbd_repeat_wait = 0;
      wlserv->prev_kev.type = 0;
    } else {
      if (kbd_repeat_1 == 0) {
        wlserv->kbd_repeat_wait = 1;
      } else {
        wlserv->kbd_repeat_wait = (kbd_repeat_1 + KEY_REPEAT_UNIT - 1) / KEY_REPEAT_UNIT;
      }

      wlserv->prev_kev = ev;
    }

#ifdef __DEBUG
    bl_debug_printf("key pressed %x %x at %d\n", ev.ksym, wlserv->xkb->mods, time);
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
    wlserv->xkb->mods |= Mod1Mask; /* XXX ModMask disables yourei by Ctrl+Alt+H on fcitx */
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

  /* The rate of repeating keys in characters per second. 0 disables any repeating. */
  if (rate == 0) {
    kbd_repeat_N = 0;
  } else {
    kbd_repeat_N = 1000 / rate;
  }
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
  bl_debug_printf("pointer_enter %p\n", surface);
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
  bl_debug_printf("pointer_leave %p\n", surface);
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

#ifdef __DEBUG
  bl_debug_printf("pointer_motion (time: %d) x %d y %d\n", time, wl_fixed_to_int(sx_w),
                  wl_fixed_to_int(sy_w));
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

    ui_window_receive_event(win, (XEvent*)&ev);
  }
}

#ifdef COMPAT_LIBVTE
void focus_gtk_window(ui_window_t *win, uint32_t time);
#endif

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state_w) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef __DEBUG
  bl_debug_printf("pointer_button (time: %d)\n", time);
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
#ifdef ZXDG_SHELL_V6
            if (disp->display->zxdg_toplevel) {
              zxdg_toplevel_v6_move(disp->display->zxdg_toplevel, wlserv->seat, serial);
            } else
#endif
#ifdef XDG_SHELL
            if (disp->display->xdg_toplevel) {
              xdg_toplevel_move(disp->display->xdg_toplevel, wlserv->seat, serial);
            } else
#endif
            {
              wl_shell_surface_move(disp->display->shell_surface, wlserv->seat, serial);
            }
          } else {
            disp->display->is_resizing = 1;

#ifdef ZXDG_SHELL_V6
            if (disp->display->zxdg_toplevel) {
              zxdg_toplevel_v6_resize(disp->display->zxdg_toplevel, wlserv->seat, serial, state);
            } else
#endif
#ifdef XDG_SHELL
            if (disp->display->xdg_toplevel) {
              xdg_toplevel_resize(disp->display->xdg_toplevel, wlserv->seat, serial, state);
            } else
#endif
            {
              wl_shell_surface_resize(disp->display->shell_surface, wlserv->seat, serial, state);
            }
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

      wlserv->pointer_button = ev.button;
    } else {
      ev.type = ButtonRelease;
      ev.button = wlserv->pointer_button;
      wlserv->pointer_button = 0;
    }
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

    ui_window_receive_event(win, (XEvent*)&ev);

#ifdef COMPAT_LIBVTE
    if (ev.type == ButtonPress && disp->display->parent == NULL /* Not input method */) {
      focus_gtk_window(win, time);
    }
#endif
  }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef __DEBUG
  bl_debug_printf("pointer_axis (time: %d)\n", time);
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
    ui_window_receive_event(win, (XEvent*)&ev);

    ev.type = ButtonRelease;
    ui_window_receive_event(win, (XEvent*)&ev);
  }
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_enter,
  pointer_leave,
  pointer_motion,
  pointer_button,
  pointer_axis,
};

static void seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps) {
  ui_wlserv_t *wlserv = data;

#ifdef DEBUG
  bl_debug_printf("KBD: %s, MOUSE %s\n", (caps & WL_SEAT_CAPABILITY_KEYBOARD) ? "attach" : "detach",
                  (caps & WL_SEAT_CAPABILITY_POINTER) ? "attach" : "detach");
#endif

  if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!wlserv->keyboard) {
      wlserv->keyboard = wl_seat_get_keyboard(wlserv->seat);
      wl_keyboard_add_listener(wlserv->keyboard, &keyboard_listener, wlserv);
    }
  } else {
    if (wlserv->keyboard) {
      wl_keyboard_destroy(wlserv->keyboard);
      wlserv->keyboard = NULL;
    }
  }

  if (caps & WL_SEAT_CAPABILITY_POINTER) {
    if (!wlserv->pointer) {
      wlserv->pointer = wl_seat_get_pointer(wlserv->seat);
      wl_pointer_add_listener(wlserv->pointer, &pointer_listener, wlserv);
    }
  } else {
    if (wlserv->pointer) {
      wl_pointer_destroy(wlserv->pointer);
      wlserv->pointer = NULL;
    }
  }

  wl_display_flush(wlserv->display);
}

static void surface_enter(void *data, struct wl_surface *surface, struct wl_output *output) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("surface_enter %p\n", surface);
#endif

  wlserv->current_kbd_surface = surface;
}

static void surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("surface_leave %p\n", surface);
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
                  display->width, display->height, display->bytes_per_pixel * 8);
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

  /* wl_surface_attach() must be called after (z)xdg_surface_configure() on (z)xdg-shell. */
#if defined(COMPAT_LIBVTE)
  if (display->wlserv->shell_type >= 2 /* ZXDG_SHELL, XDG_SHELL */) {
    /* do nothing */
  } else
#elif defined(XDG_SHELL)
  if (display->xdg_surface) {
    /* do nothing */
  } else
#elif defined(ZXDG_SHELL_V6)
  if (display->zxdg_surface) {
    /* do nothing */
  } else
#endif
  {
    wl_surface_attach(display->surface, display->buffer, 0, 0);
  }

  return 1;
}

static void destroy_shm_buffer(Display *display) {
#if 0
  display->damage_x = display->damage_y = display->damage_width = display->damage_height = 0;
#endif

  wl_surface_attach(display->surface, NULL, 0, 0);
  wl_buffer_destroy(display->buffer);
  munmap(display->fb, display->line_length * (display->height + DECORATION_MARGIN));
}

static u_int total_min_width(ui_window_t *win) {
  u_int count;
  u_int min_width;

  min_width = win->min_width + win->hmargin * 2;

  for (count = 0; count < win->num_children; count++) {
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

  for (count = 0; count < win->num_children; count++) {
    if (win->children[count]->is_mapped) {
      /* XXX */
      min_height += total_min_height(win->children[count]);
    }
  }

  return min_height;
}

static u_int max_width_inc(ui_window_t *win) {
  u_int count;
  u_int width_inc;

  width_inc = win->width_inc;

  for (count = 0; count < win->num_children; count++) {
    if (win->children[count]->is_mapped) {
      u_int sub_inc;

      /*
       * XXX
       * we should calculate least common multiple of width_inc and sub_inc.
       */
      if ((sub_inc = max_width_inc(win->children[count])) > width_inc) {
        width_inc = sub_inc;
      }
    }
  }

  return width_inc;
}

static u_int max_height_inc(ui_window_t *win) {
  u_int count;
  u_int height_inc;

  height_inc = win->height_inc;

  for (count = 0; count < win->num_children; count++) {
    if (win->children[count]->is_mapped) {
      u_int sub_inc;

      /*
       * XXX
       * we should calculate least common multiple of width_inc and sub_inc.
       */
      if ((sub_inc = max_height_inc(win->children[count])) > height_inc) {
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

static void close_toplevel_window(ui_display_t *disp) {
  if (disp->roots[0]->window_destroyed) {
    (*disp->roots[0]->window_destroyed)(disp->roots[0]);
  }
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

  if (rotate_display) {
    BL_SWAP(unsigned int, min_width, min_height);
    BL_SWAP(unsigned int, width_inc, height_inc);
  }

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
  bl_debug_printf("shell_surface_configure\nConfiguring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  /* mutter and gnome-shell 41.3 can invoke configure event with width == 0 and height == 0. */
  if (width != 0 && height != 0 &&
      check_resize(disp->display->width, disp->display->height, &width, &height,
                   total_min_width(disp->roots[0]), total_min_height(disp->roots[0]),
                   max_width_inc(disp->roots[0]), max_height_inc(disp->roots[0]),
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

#ifdef ZXDG_SHELL_V6
static void zxdg_surface_configure(void *data, struct zxdg_surface_v6 *zxdg_surface,
                                   uint32_t serial) {
  ui_display_t *disp = data;

#ifdef __DEBUG
  bl_debug_printf("zxdg_surface_configure\n");
#endif

  zxdg_surface_v6_ack_configure(zxdg_surface, serial);

  disp->display->zxdg_surface_configured = 1;

  /* See flush_damage() */
#if 0
  wl_surface_attach(disp->display->surface, disp->display->buffer, 0, 0);
  wl_surface_damage(disp->display->surface, 0, 0, disp->display->width, disp->display->height);
  wl_surface_commit(disp->display->surface);
#endif
}

static const struct zxdg_surface_v6_listener zxdg_surface_listener = {
  zxdg_surface_configure
};

static void zxdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *zxdg_toplevel,
                                    int32_t width, int32_t height, struct wl_array *states) {
  ui_display_t *disp = data;
#ifdef __DEBUG
  uint32_t *ps;

  bl_debug_printf("zxdg_toplevel_configure: w %d, h %d / states: ", width, height);
  wl_array_for_each(ps, states) {
  switch(*ps) {
    case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
      bl_msg_printf("MAXIMIZED ");
      break;
    case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
      bl_msg_printf("FULLSCREEN ");
      break;
    case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
      bl_msg_printf("RESIZING ");
      break;
    case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
      bl_msg_printf("ACTIVATED ");
      break;
    }
  }
  bl_msg_printf("\n");
#endif

#ifdef __DEBUG
  bl_debug_printf("Configuring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  if (width == 0) {
    width = disp->display->width;
  }
  if (height == 0) {
    height = disp->display->height;
  }

  if (check_resize(disp->display->width, disp->display->height, &width, &height,
                   total_min_width(disp->roots[0]), total_min_height(disp->roots[0]),
                   max_width_inc(disp->roots[0]), max_height_inc(disp->roots[0]),
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

static void zxdg_toplevel_close(void *data, struct zxdg_toplevel_v6 *toplevel) {
  close_toplevel_window(data);
}

const struct zxdg_toplevel_v6_listener zxdg_toplevel_listener = {
  zxdg_toplevel_configure, zxdg_toplevel_close
};

static void zxdg_popup_configure(void *data, struct zxdg_popup_v6 *zxdg_popup, int32_t x, int32_t y,
                                 int32_t width, int32_t height) {
  ui_display_t *disp = data;
  int resized;

#ifdef __DEBUG
  bl_debug_printf("Configuring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  if (width == 0) {
    width = disp->display->width;
  }
  if (height == 0) {
    height = disp->display->height;
  }

  if (rotate_display) {
    resized = resize_display(disp, height, width, 0);
  } else {
    resized = resize_display(disp, width, height, 0);
  }

  if (resized) {
    ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height, NOTIFY_TO_MYSELF);
  }
}

static void zxdg_popup_done(void *data, struct zxdg_popup_v6 *popup) {
  ui_display_t *disp = data;

  ui_window_final(disp->roots[0]);
}

const struct zxdg_popup_v6_listener zxdg_popup_listener = {
  zxdg_popup_configure, zxdg_popup_done
};
#endif

#ifdef XDG_SHELL
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  ui_display_t *disp = data;

#ifdef __DEBUG
  bl_debug_printf("xdg_surface_configure\n");
#endif

  xdg_surface_ack_configure(xdg_surface, serial);

  disp->display->xdg_surface_configured = 1;

  /* See flush_damage() */
#if 0
  wl_surface_attach(disp->display->surface, disp->display->buffer, 0, 0);
  wl_surface_damage(disp->display->surface, 0, 0, disp->display->width, disp->display->height);
  wl_surface_commit(disp->display->surface);
#endif
}

static const struct xdg_surface_listener xdg_surface_listener = {
  xdg_surface_configure
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                   int32_t width, int32_t height, struct wl_array *states) {
  ui_display_t *disp = data;
#ifdef __DEBUG
  uint32_t *ps;

  bl_debug_printf("xdg_toplevel_configure: w %d, h %d / states: ", width, height);
  wl_array_for_each(ps, states) {
  switch(*ps) {
    case XDG_TOPLEVEL_STATE_MAXIMIZED:
      bl_msg_printf("MAXIMIZED ");
      break;
    case XDG_TOPLEVEL_STATE_FULLSCREEN:
      bl_msg_printf("FULLSCREEN ");
      break;
    case XDG_TOPLEVEL_STATE_RESIZING:
      bl_msg_printf("RESIZING ");
      break;
    case XDG_TOPLEVEL_STATE_ACTIVATED:
      bl_msg_printf("ACTIVATED ");
      break;
    }
  }
  bl_msg_printf("\n");
#endif

#ifdef __DEBUG
  bl_debug_printf("Configuring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  if (width == 0) {
    width = disp->display->width;
  }
  if (height == 0) {
    height = disp->display->height;
  }

  if (check_resize(disp->display->width, disp->display->height, &width, &height,
                   total_min_width(disp->roots[0]), total_min_height(disp->roots[0]),
                   max_width_inc(disp->roots[0]), max_height_inc(disp->roots[0]),
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

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
  close_toplevel_window(data);
}

const struct xdg_toplevel_listener xdg_toplevel_listener = {
  xdg_toplevel_configure, xdg_toplevel_close
};

static void xdg_popup_configure(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y,
                                int32_t width, int32_t height) {
  ui_display_t *disp = data;
  int resized;

#ifdef __DEBUG
  bl_debug_printf("Configuring size from w %d h %d to w %d h %d.\n",
                  disp->display->width, disp->display->height, width, height);
#endif

  if (width == 0) {
    width = disp->display->width;
  }
  if (height == 0) {
    height = disp->display->height;
  }

  if (rotate_display) {
    resized = resize_display(disp, height, width, 0);
  } else {
    resized = resize_display(disp, width, height, 0);
  }

  if (resized) {
    ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height, NOTIFY_TO_MYSELF);
  }
}

static void xdg_popup_done(void *data, struct xdg_popup *popup) {
  ui_display_t *disp = data;

  ui_window_final(disp->roots[0]);
}

const struct xdg_popup_listener xdg_popup_listener = {
  xdg_popup_configure, xdg_popup_done
};
#endif
#endif /* Not COMPAT_LIBVTE */

static void update_mime(char **mime, const char *new_mime) {
  if (strcmp(new_mime, "text/plain;charset=utf-8") == 0) {
    *mime = "text/plain;charset=utf-8";
  } else if (*mime == NULL && strcmp(new_mime, "UTF8_STRING") == 0) {
    *mime = "UTF8_STRING";
  }
}

static void data_offer_offer(void *data, struct wl_data_offer *offer, const char *type) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("data_offer_offer %s\n", type);
#endif

  update_mime(&wlserv->sel_offer_mime, type);
}

static void data_offer_source_action(void *data, struct wl_data_offer *data_offer,
                                     uint32_t source_actions) {
#ifdef __DEBUG
  bl_debug_printf("DND: source_action %d\n", source_actions);
#endif
}

static void data_offer_action(void *data, struct wl_data_offer *data_offer,
                              uint32_t dnd_action) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("DND: action %d\n", dnd_action);
#endif

  wlserv->dnd_action = dnd_action;
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
  bl_debug_printf("data_device_data_offer %p\n", offer);
#endif

  wl_data_offer_add_listener(offer, &data_offer_listener, wlserv);
  wlserv->dnd_offer = offer;
  wlserv->sel_offer_mime = NULL;
}

static void data_device_enter(void *data, struct wl_data_device *data_device,
                              uint32_t serial, struct wl_surface *surface,
                              wl_fixed_t x_w, wl_fixed_t y_w,
                              struct wl_data_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("DND: enter %p\n", offer);
#endif

  wl_data_offer_accept(offer, serial, "text/uri-list");

  /* wl_data_device_manager_get_version() >= 3 */
  wl_data_offer_set_actions(offer,
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY|
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);

  wlserv->data_surface = surface;
}

static void data_device_leave(void *data, struct wl_data_device *data_device) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("DND: leave\n");
#endif

  if (wlserv->dnd_offer) {
    wl_data_offer_destroy(wlserv->dnd_offer);
    wlserv->dnd_offer = NULL;
  }
  if (wlserv->sel_offer) {
    wl_data_offer_destroy(wlserv->sel_offer);
    wlserv->sel_offer = NULL;
  }
  wlserv->data_surface = NULL;
}

static void data_device_motion(void *data, struct wl_data_device *data_device,
                               uint32_t time, wl_fixed_t x_w, wl_fixed_t y_w) {
}

static int unescape(char *src) {
  char *dst;
  int c;

  dst = src;

  while (*src) {
    if (*src == '%' && sscanf(src, "%%%2x", &c) == 1) {
      *(dst++) = c;
      src += 3;
    } else {
      *(dst++) = *(src++);
    }
  }
  *dst = '\0';

  return (src != dst);
}

static void receive_data(ui_display_t *disp, void *offer, const char *mime, int clipboard) {
  int fds[2];

  if (pipe(fds) == 0) {
    char buf[512];
    ssize_t len;

    if (clipboard) {
      wl_data_offer_receive(offer, mime, fds[1]);
    } else {
#ifdef GTK_PRIMARY
      if (disp->display->wlserv->gxsel_device_manager) {
        gtk_primary_selection_offer_receive(offer, mime, fds[1]);
      }
#ifdef ZWP_PRIMARY
      else
#endif
#endif
#ifdef ZWP_PRIMARY
      if (disp->display->wlserv->zxsel_device_manager) {
        zwp_primary_selection_offer_v1_receive(offer, mime, fds[1]);
      }
#endif
    }

    wl_display_flush(disp->display->wlserv->display);
    close(fds[1]);

    while ((len = read(fds[0], buf, sizeof(buf) - 1)) > 0) {
      if (strcmp(mime, "text/uri-list") == 0) {
        char *p;

        while (buf[len - 1] == '\n' || buf[len - 1] == '\r') {
          len--;
        }
        buf[len] = '\0';

        for (p = buf; *p; p++) {
          if (*p == 'f') {
            if (strncmp(p + 1, "ile://", 6) == 0) {
              memmove(p, p + 7, len - 7 + 1);
              p += 6;
              len -= 7;
            }
          } else if (*p == '\r' || *p == '\n') {
            *p = ' ';
          }
        }

        if (unescape(buf)) {
          len = strlen(buf);
        }
      }
#ifdef __DEBUG
      else {
        buf[len] = '\0';
      }

      bl_debug_printf("Receive data (len %d, mime %s): %s\n", len, mime, buf);
#endif

      if (disp->display->wlserv->dnd_action ==
          WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE /* Shift+Drop */ &&
          disp->roots[0]->set_xdnd_config) {
        (*disp->roots[0]->set_xdnd_config)(disp->roots[0], NULL, "scp", buf);
        break;
      } else if (disp->roots[0]->utf_selection_notified) {
        (*disp->roots[0]->utf_selection_notified)(disp->roots[0], buf, len);
      }
    }
    close(fds[0]);
  }
}

static void data_device_drop(void *data, struct wl_data_device *data_device) {
  ui_wlserv_t *wlserv = data;
  ui_display_t *disp;

#ifdef  __DEBUG
  bl_debug_printf("DND: drop\n");
#endif

  if ((disp = surface_to_display(wlserv->data_surface))) {
    receive_data(disp, wlserv->dnd_offer, "text/uri-list", 1);
  }
}

static void data_device_selection(void *data, struct wl_data_device *data_device,
                                  struct wl_data_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("data_device_selection %p -> %p\n", wlserv->sel_offer, offer);
#endif

  if (wlserv->sel_source) {
    return;
  }

  if (wlserv->sel_offer) {
    wl_data_offer_destroy(wlserv->sel_offer);
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
  bl_debug_printf("data_source_send %s\n", mime);
#endif

  if (disp->clipboard_owner->utf_selection_requested) {
    /* utf_selection_requested() calls ui_display_send_text_selection() */
    (*disp->clipboard_owner->utf_selection_requested)(disp->clipboard_owner, NULL, 0);
  }
}

#ifdef COMPAT_LIBVTE
#include <glib.h>
gint64 deadline_ignoring_source_cancelled_event;
#endif

static void data_source_cancelled(void *data, struct wl_data_source *source) {
  ui_display_t *disp = data;
  ui_wlserv_t *wlserv = disp->display->wlserv;

#ifdef __DEBUG
  bl_debug_printf("data_source_cancelled %p %p\n", source, wlserv->sel_source);
#endif

#ifdef COMPAT_LIBVTE
  if (deadline_ignoring_source_cancelled_event != 0 &&
      deadline_ignoring_source_cancelled_event > g_get_monotonic_time()) {
  } else {
    deadline_ignoring_source_cancelled_event = 0;
#endif
    if (source == wlserv->sel_source) {
      ui_display_clear_clipboard(disp, disp->clipboard_owner);
    }
#ifdef COMPAT_LIBVTE
  }
#endif
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

#ifdef GTK_PRIMARY
static void gxsel_offer_offer(void *data, struct gtk_primary_selection_offer *offer,
                              const char *type) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("gxsel_offer_offer %s\n", type);
#endif

  update_mime(&wlserv->xsel_offer_mime, type);
}

static const struct gtk_primary_selection_offer_listener gxsel_offer_listener = {
  gxsel_offer_offer,
};

static void gxsel_device_data_offer(void *data,
                                    struct gtk_primary_selection_device *device,
                                    struct gtk_primary_selection_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("gtk_device_data_offer %p\n", offer);
#endif

  gtk_primary_selection_offer_add_listener(offer, &gxsel_offer_listener, wlserv);
  wlserv->xsel_offer_mime = NULL;
}

static void gxsel_device_selection(void *data,
                                   struct gtk_primary_selection_device *device,
                                   struct gtk_primary_selection_offer *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("gtk_device_selection %p -> %p\n", wlserv->gxsel_offer, offer);
#endif

  if (wlserv->gxsel_source) {
    return;
  }

  if (wlserv->gxsel_offer) {
    gtk_primary_selection_offer_destroy(wlserv->gxsel_offer);
  }
  wlserv->gxsel_offer = offer;
}

static const struct gtk_primary_selection_device_listener gxsel_device_listener = {
  gxsel_device_data_offer,
  gxsel_device_selection
};

static void gxsel_source_send(void *data, struct gtk_primary_selection_source *source,
                              const char *mime, int32_t fd) {
  ui_display_t *disp = data;
  disp->display->wlserv->sel_fd = fd;

#ifdef __DEBUG
  bl_debug_printf("gxsel_source_send %s\n", mime);
#endif

  if (disp->selection_owner->utf_selection_requested) {
    /* utf_selection_requested() calls ui_display_send_text_selection() */
    (*disp->selection_owner->utf_selection_requested)(disp->selection_owner, NULL, 0);
  }
}

static void gxsel_source_cancelled(void *data, struct gtk_primary_selection_source *source) {
  ui_display_t *disp = data;
  ui_wlserv_t *wlserv = disp->display->wlserv;

#ifdef __DEBUG
  bl_debug_printf("gxsel_source_cancelled %p %p\n", source, wlserv->gxsel_source);
#endif

#ifdef COMPAT_LIBVTE
  if (deadline_ignoring_source_cancelled_event != 0 &&
      deadline_ignoring_source_cancelled_event > g_get_monotonic_time()) {
  } else {
    deadline_ignoring_source_cancelled_event = 0;
#endif
    if (source == wlserv->gxsel_source) {
      ui_display_clear_selection(disp, disp->selection_owner);
    }
#ifdef COMPAT_LIBVTE
  }
#endif
}

static const struct gtk_primary_selection_source_listener gxsel_source_listener = {
  gxsel_source_send,
  gxsel_source_cancelled,
};
#endif

#ifdef ZWP_PRIMARY
static void zxsel_offer_offer(void *data, struct zwp_primary_selection_offer_v1 *offer,
                              const char *type) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("zxsel_offer_offer %s\n", type);
#endif

  update_mime(&wlserv->xsel_offer_mime, type);
}

static const struct zwp_primary_selection_offer_v1_listener zxsel_offer_listener = {
  zxsel_offer_offer,
};

static void zxsel_device_data_offer(void *data,
                                    struct zwp_primary_selection_device_v1 *device,
                                    struct zwp_primary_selection_offer_v1 *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("zwp_device_data_offer %p\n", offer);
#endif

  zwp_primary_selection_offer_v1_add_listener(offer, &zxsel_offer_listener, wlserv);
  wlserv->xsel_offer_mime = NULL;
}

static void zxsel_device_selection(void *data,
                                   struct zwp_primary_selection_device_v1 *device,
                                   struct zwp_primary_selection_offer_v1 *offer) {
  ui_wlserv_t *wlserv = data;

#ifdef __DEBUG
  bl_debug_printf("zwp_device_selection %p -> %p\n", wlserv->zxsel_offer, offer);
#endif

  if (wlserv->zxsel_source) {
    return;
  }

  if (wlserv->zxsel_offer) {
    zwp_primary_selection_offer_v1_destroy(wlserv->zxsel_offer);
  }
  wlserv->zxsel_offer = offer;
}

static const struct zwp_primary_selection_device_v1_listener zxsel_device_listener = {
  zxsel_device_data_offer,
  zxsel_device_selection
};

static void zxsel_source_send(void *data, struct zwp_primary_selection_source_v1 *source,
                              const char *mime, int32_t fd) {
  ui_display_t *disp = data;
  disp->display->wlserv->sel_fd = fd;

#ifdef __DEBUG
  bl_debug_printf("zxsel_source_send %s\n", mime);
#endif

  if (disp->selection_owner->utf_selection_requested) {
    /* utf_selection_requested() calls ui_display_send_text_selection() */
    (*disp->selection_owner->utf_selection_requested)(disp->selection_owner, NULL, 0);
  }
}

static void zxsel_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *source) {
  ui_display_t *disp = data;
  ui_wlserv_t *wlserv = disp->display->wlserv;

#ifdef __DEBUG
  bl_debug_printf("zxsel_source_cancelled %p %p\n", source, wlserv->zxsel_source);
#endif

#ifdef COMPAT_LIBVTE
  if (deadline_ignoring_source_cancelled_event != 0 &&
      deadline_ignoring_source_cancelled_event > g_get_monotonic_time()) {
  } else {
    deadline_ignoring_source_cancelled_event = 0;
#endif
    if (source == wlserv->zxsel_source) {
      ui_display_clear_selection(disp, disp->selection_owner);
    }
#ifdef COMPAT_LIBVTE
  }
#endif
}

static const struct zwp_primary_selection_source_v1_listener zxsel_source_listener = {
  zxsel_source_send,
  zxsel_source_cancelled,
};
#endif

static ui_wlserv_t *open_wl_display(char *name) {
  ui_wlserv_t *wlserv;

  if (!(wlserv = calloc(1, sizeof(ui_wlserv_t) + sizeof(*wlserv->xkb)))) {
    return NULL;
  }

  wlserv->xkb = (struct ui_xkb*)(wlserv + 1);

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

  wlserv->xkb->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  /* See seat_capabilities() */
#if 0
  wlserv->keyboard = wl_seat_get_keyboard(wlserv->seat);
  wl_keyboard_add_listener(wlserv->keyboard, &keyboard_listener, wlserv);

  wlserv->pointer = wl_seat_get_pointer(wlserv->seat);
  wl_pointer_add_listener(wlserv->pointer, &pointer_listener, wlserv);
#endif

  if ((wlserv->cursor_theme = wl_cursor_theme_load(NULL, 32, wlserv->shm))) {
    /* The order should be the one of wl_shell_surface_resize. */
    char *names[] = { "xterm", "n-resize", "s-resize", "w-resize", "nw-resize",
                      "sw-resize", "e-resize", "ne-resize", "se-resize", "move" };
    int count;
    int has_cursor = 0;

    for (count = 0; count < BL_ARRAY_SIZE(names); count++) {
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

  if (wlserv->data_device_manager) {
    wlserv->data_device = wl_data_device_manager_get_data_device(wlserv->data_device_manager,
                                                                 wlserv->seat);
    wl_data_device_add_listener(wlserv->data_device, &data_device_listener, wlserv);
  }

#ifdef GTK_PRIMARY
  if (wlserv->gxsel_device_manager) {
    wlserv->gxsel_device =
      gtk_primary_selection_device_manager_get_device(wlserv->gxsel_device_manager,
                                                      wlserv->seat);
    gtk_primary_selection_device_add_listener(wlserv->gxsel_device,
                                              &gxsel_device_listener, wlserv);
  }
#ifdef ZWP_PRIMARY
  else
#endif
#endif
#ifdef ZWP_PRIMARY
  if (wlserv->zxsel_device_manager) {
    wlserv->zxsel_device =
      zwp_primary_selection_device_manager_v1_get_device(wlserv->zxsel_device_manager,
                                                         wlserv->seat);
    zwp_primary_selection_device_v1_add_listener(wlserv->zxsel_device,
                                                 &zxsel_device_listener, wlserv);
  }
#endif

  wlserv->sel_fd = -1;

  return wlserv;
}

static void close_wl_display(ui_wlserv_t *wlserv) {
#ifdef __DEBUG
  bl_debug_printf("Closing wldisplay.\n");
#endif

  if (wlserv->text_input_manager) {
    zwp_text_input_manager_v3_destroy(wlserv->text_input_manager);
  }

  /* dnd_offer and sel_offer are destroyed in data_device_leave() */
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
#ifdef GTK_PRIMARY
  if (wlserv->gxsel_offer) {
    gtk_primary_selection_offer_destroy(wlserv->gxsel_offer);
    wlserv->gxsel_offer = NULL;
  }
  if (wlserv->gxsel_source) {
    gtk_primary_selection_source_destroy(wlserv->gxsel_source);
    wlserv->gxsel_source = NULL;
  }
#endif
#ifdef ZWP_PRIMARY
  if (wlserv->zxsel_offer) {
    zwp_primary_selection_offer_v1_destroy(wlserv->zxsel_offer);
    wlserv->zxsel_offer = NULL;
  }
  if (wlserv->zxsel_source) {
    zwp_primary_selection_source_v1_destroy(wlserv->zxsel_source);
    wlserv->zxsel_source = NULL;
  }
#endif

#ifndef COMPAT_LIBVTE
  if (wlserv->decoration_manager) {
    zxdg_decoration_manager_v1_destroy(wlserv->decoration_manager);
  }
#endif

#ifdef COMPAT_LIBVTE
  wl_subcompositor_destroy(wlserv->subcompositor);
#else
#ifdef ZXDG_SHELL_V6
  if (wlserv->zxdg_shell) {
    zxdg_shell_v6_destroy(wlserv->zxdg_shell);
    wlserv->zxdg_shell = NULL;
  } else
#endif
#ifdef XDG_SHELL
  if (wlserv->xdg_shell) {
    xdg_wm_base_destroy(wlserv->xdg_shell);
    wlserv->xdg_shell = NULL;
  } else
#endif
  if (wlserv->shell) {
    wl_shell_destroy(wlserv->shell);
    wlserv->shell = NULL;
  }
#endif

  wl_shm_destroy(wlserv->shm);
  wl_output_destroy(wlserv->output);
  wl_compositor_destroy(wlserv->compositor);
#ifndef COMPAT_LIBVTE
  wl_registry_destroy(wlserv->registry);
#endif

  if (wlserv->cursor_surface) {
    wl_surface_destroy(wlserv->cursor_surface);
  }
  if (wlserv->cursor_theme) {
    wl_cursor_theme_destroy(wlserv->cursor_theme);
  }

  if (wlserv->keyboard) {
    wl_keyboard_destroy(wlserv->keyboard);
    wlserv->keyboard = NULL;
  }

  xkb_state_unref(wlserv->xkb->state);
  xkb_keymap_unref(wlserv->xkb->keymap);
  xkb_context_unref(wlserv->xkb->ctx);

  if (wlserv->pointer) {
    wl_pointer_destroy(wlserv->pointer);
    wlserv->pointer = NULL;
  }

  wl_seat_destroy(wlserv->seat);

  wl_data_device_destroy(wlserv->data_device);
  wl_data_device_manager_destroy(wlserv->data_device_manager);

#ifdef GTK_PRIMARY
  if (wlserv->gxsel_device) {
    gtk_primary_selection_device_destroy(wlserv->gxsel_device);
  }
  if (wlserv->gxsel_device_manager) {
    gtk_primary_selection_device_manager_destroy(wlserv->gxsel_device_manager);
  }
#endif
#ifdef ZWP_PRIMARY
  if (wlserv->zxsel_device) {
    zwp_primary_selection_device_v1_destroy(wlserv->zxsel_device);
  }
  if (wlserv->zxsel_device_manager) {
    zwp_primary_selection_device_manager_v1_destroy(wlserv->zxsel_device_manager);
  }
#endif

#ifndef COMPAT_LIBVTE
  wl_display_disconnect(wlserv->display);

  free(wlserv);
#endif
}

static void close_display(ui_display_t *disp) {
  u_int count;

#ifdef __DEBUG
  bl_debug_printf("Closing ui_display_t\n");
#endif

  free(disp->name);

  for (count = 0; count < disp->num_roots; count++) {
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
#ifdef ZXDG_SHELL_V6
  if (disp->display->zxdg_surface) {
    destroy_shm_buffer(disp->display);
    disp->display->buffer = NULL;
    if (disp->display->zxdg_toplevel) {
      zxdg_toplevel_v6_destroy(disp->display->zxdg_toplevel);
    } else /* if (disp->display->zxdg_popup) */ {
      zxdg_popup_v6_destroy(disp->display->zxdg_popup);
    }
    zxdg_surface_v6_destroy(disp->display->zxdg_surface);
    wl_surface_destroy(disp->display->surface);
  } else
#endif
#ifdef XDG_SHELL
  if (disp->display->xdg_surface) {
#ifndef COMPAT_LIBVTE
    if (disp->display->toplevel_decoration) {
      zxdg_toplevel_decoration_v1_destroy(disp->display->toplevel_decoration);
    }
#endif

    destroy_shm_buffer(disp->display);
    disp->display->buffer = NULL;
    if (disp->display->xdg_toplevel) {
      xdg_toplevel_destroy(disp->display->xdg_toplevel);
    } else /* if (disp->display->xdg_popup) */ {
      xdg_popup_destroy(disp->display->xdg_popup);
    }
    xdg_surface_destroy(disp->display->xdg_surface);
    wl_surface_destroy(disp->display->surface);
  } else
#endif
  if (disp->display->shell_surface) {
    destroy_shm_buffer(disp->display);
    disp->display->buffer = NULL;
    wl_shell_surface_destroy(disp->display->shell_surface);
    wl_surface_destroy(disp->display->surface);
  }
#endif

  ui_display_set_use_text_input(disp, 0);

  if (disp->display->wlserv->ref_count == 1) {
    u_int count;

    close_wl_display(disp->display->wlserv);

    for (count = 0; count < num_wlservs; count++) {
      if (disp->display->wlserv == wlservs[count]) {
        if (--num_wlservs == 0) {
          free(wlservs);
          wlservs = NULL;
        } else {
          wlservs[count] = wlservs[--num_wlservs];
        }
      }
    }
  } else {
    disp->display->wlserv->ref_count--;
  }

  free(disp);
}

static int flush_damage(Display *display) {
  if (display->damage_height > 0) {
    /*
     * zxdg_shell and xdg_shell seem to require calling wl_surface_attach() every time
     * differently from wl_shell.
     */
#ifdef COMPAT_LIBVTE
    if (display->wlserv->shell_type >= 2 /* ZXDG_SHELL, XDG_SHELL */) {
      wl_surface_attach(display->surface, display->buffer, 0, 0);
    } else
#else
#ifdef ZXDG_SHELL_V6
    if (display->zxdg_surface) {
      if (display->zxdg_surface_configured) {
        wl_surface_attach(display->surface, display->buffer, 0, 0);
      } else {
        return 0;
      }
    } else
#endif
#ifdef XDG_SHELL
    if (display->xdg_surface) {
      if (display->xdg_surface_configured) {
        wl_surface_attach(display->surface, display->buffer, 0, 0);
      } else {
        return 0;
      }
    } else
#endif
    { ; }
#endif

    wl_surface_damage(display->surface, display->damage_x, display->damage_y,
                      display->damage_width, display->damage_height);

    display->damage_x = display->damage_y = display->damage_width = display->damage_height = 0;

    return 1;
  } else {

    return 0;
  }
}

static int cache_damage_h(Display *display, int x, int y, u_int width, u_int height) {
  if (x == display->damage_x && width == display->damage_width) {
    if (y == display->damage_y + display->damage_height) {
      display->damage_height += height;

      return 1;
    } else if (y + height == display->damage_y) {
      display->damage_y = y;
      display->damage_height += height;

      return 1;
    }
  }

  return 0;
}

static int cache_damage_v(Display *display, int x, int y, u_int width, u_int height) {
  if (y == display->damage_y && height == display->damage_height) {
    if (x == display->damage_x + display->damage_width) {
      display->damage_width += width;

      return 1;
    } else if (x + width == display->damage_x) {
      display->damage_x = x;
      display->damage_width += width;

      return 1;
    }
  }

  return 0;
}

static void damage_buffer(Display *display, int x, int y, u_int width, u_int height) {
  if (!cache_damage_v(display, x, y, width, height) &&
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
#ifdef COMPAT_LIBVTE
    if (display->parent) {
      /*
       * Input method window.
       * refrect wl_surface_damage() immediately.
       */
      wl_surface_commit(display->parent_surface);
    }
#endif

    return 1;
  } else {
    return 0;
  }
}

#ifndef COMPAT_LIBVTE
static void decoration_configure(void *data,
                                 struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1,
                                 uint32_t mode) {
#ifdef __DEBUG
  bl_debug_printf("decoration mode: %d\n", mode);
#endif
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
  decoration_configure
};
#endif

static void input_enter(void *data, struct zwp_text_input_v3 *text_input,
                        struct wl_surface *surface) {
  ui_display_t *disp = data;
  ui_window_t *win;
  int x, y;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " input_enter\n");
#endif

  zwp_text_input_v3_enable(text_input);

  if (disp->display->text_input_x > 0 || disp->display->text_input_y > 0) {
    zwp_text_input_v3_set_cursor_rectangle(disp->display->text_input,
                                           disp->display->text_input_x,
                                           disp->display->text_input_y,
                                           0, disp->display->text_input_line_height);
  }

  zwp_text_input_v3_commit(text_input);
}

static void input_leave(void *data, struct zwp_text_input_v3 *text_input,
                        struct wl_surface *surface) {
  zwp_text_input_v3_disable(text_input);
  zwp_text_input_v3_commit(text_input);
}

static void input_preedit_string(void *data, struct zwp_text_input_v3 *text_input,
                                 const char *text, int32_t cursor_begin, int32_t cursor_end) {
  ui_display_t *disp = data;
  ui_window_t *win;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " preedit_string %s\n", text);
#endif

  if (bl_compare_str(disp->display->preedit_text, text) == 0) {
    return;
  }

  free(disp->display->preedit_text);
  if (text) {
    disp->display->preedit_text = strdup(text);
  } else {
    disp->display->preedit_text = NULL;
  }

  if ((win = search_inputtable_window(NULL, disp->roots[0]))) {
    (*win->preedit)(win, text ? text : "", NULL);
  }
}

static void input_commit_string(void *data, struct zwp_text_input_v3 *text_input,
                                const char *text) {
  ui_display_t *disp = data;
  ui_window_t *win;
  XKeyEvent kev;

  kev.type = KeyPress;
  kev.time = CurrentTime;
  kev.state = 0;
  kev.ksym = 0;
  kev.keycode = 0;
  kev.utf8 = text;

  if ((win = search_inputtable_window(NULL, disp->roots[0]))) {
    ui_window_receive_event(win, (XEvent *)&kev);
  }

  free(disp->display->preedit_text);
  disp->display->preedit_text = NULL;
}

static void input_delete_surrounding_text(void *data, struct zwp_text_input_v3 *text_input,
                                          uint32_t before_length, uint32_t after_length) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " text_input delete_surrounding_text | "
                  "before_length:%u, after_length:%u\n",
                  before_length, after_length);
#endif
}

static void input_done(void *data, struct zwp_text_input_v3 *text_input, uint32_t serial) {
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " text_input done | serial:%u\n", serial);
#endif
}

static const struct zwp_text_input_v3_listener text_input_listener = {
  input_enter, input_leave, input_preedit_string,
  input_commit_string, input_delete_surrounding_text, input_done
};

static void create_surface(ui_display_t *disp, int x, int y, u_int width, u_int height,
                           char *app_name) {
  Display *display = disp->display;
  ui_wlserv_t *wlserv = display->wlserv;

  display->surface = wl_compositor_create_surface(wlserv->compositor);

#ifdef COMPAT_LIBVTE
  display->subsurface = wl_subcompositor_get_subsurface(wlserv->subcompositor, display->surface,
                                                        display->parent_surface);
#else /* COMPAT_LIBVTE */
#ifdef ZXDG_SHELL_V6
  if (wlserv->zxdg_shell) {
    display->zxdg_surface = zxdg_shell_v6_get_xdg_surface(wlserv->zxdg_shell, display->surface);
    zxdg_surface_v6_add_listener(display->zxdg_surface, &zxdg_surface_listener, disp);
    if (display->parent) {
      /* Input Method */
      struct zxdg_positioner_v6 *pos = zxdg_shell_v6_create_positioner(wlserv->zxdg_shell);
      zxdg_positioner_v6_set_size(pos, width, height);
      zxdg_positioner_v6_set_anchor_rect(pos, x, y, width, height);
      zxdg_positioner_v6_set_offset(pos, 0, 0);
      zxdg_positioner_v6_set_anchor(pos,
                                    ZXDG_POSITIONER_V6_ANCHOR_TOP|ZXDG_POSITIONER_V6_ANCHOR_LEFT);
#if 0
      zxdg_positioner_v6_set_gravity(pos, ZXDG_POSITIONER_GRAVITY_TOP);
      zxdg_positioner_v6_set_constraint_adjustment(
        pos,
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X |
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X |
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y |
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X |
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y);
#endif
      display->zxdg_popup = zxdg_surface_v6_get_popup(display->zxdg_surface,
                                                      display->parent->display->zxdg_surface,
                                                      pos);
      zxdg_positioner_v6_destroy(pos);
      zxdg_popup_v6_add_listener(display->zxdg_popup, &zxdg_popup_listener, disp);
    } else {
      display->zxdg_toplevel = zxdg_surface_v6_get_toplevel(display->zxdg_surface);
      zxdg_toplevel_v6_set_app_id(display->zxdg_toplevel, app_name);
      zxdg_toplevel_v6_add_listener(display->zxdg_toplevel, &zxdg_toplevel_listener, disp);
      wl_surface_add_listener(display->surface, &surface_listener, wlserv);
#if 0
      zxdg_surface_v6_set_window_geometry(display->zxdg_surface, x, y, width, height);
#endif
    }
    wl_surface_commit(display->surface);
  } else
#endif
#ifdef XDG_SHELL
  if (wlserv->xdg_shell) {
    display->xdg_surface = xdg_wm_base_get_xdg_surface(wlserv->xdg_shell, display->surface);
    xdg_surface_add_listener(display->xdg_surface, &xdg_surface_listener, disp);
    if (display->parent) {
      /* Input Method */
      struct xdg_positioner *pos = xdg_wm_base_create_positioner(wlserv->xdg_shell);
      xdg_positioner_set_size(pos, width, height);
      xdg_positioner_set_anchor_rect(pos, x, y, width, height);
      xdg_positioner_set_offset(pos, 0, 0);
      xdg_positioner_set_anchor(pos, XDG_POSITIONER_ANCHOR_TOP|XDG_POSITIONER_ANCHOR_LEFT);
#if 0
      xdg_positioner_set_gravity(pos, XDG_POSITIONER_GRAVITY_TOP);
      xdg_positioner_set_constraint_adjustment(pos,
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y);
#endif
      display->xdg_popup = xdg_surface_get_popup(display->xdg_surface,
                                                 display->parent->display->xdg_surface,
                                                 pos);
      xdg_positioner_destroy(pos);
      xdg_popup_add_listener(display->xdg_popup, &xdg_popup_listener, disp);
    } else {
      display->xdg_toplevel = xdg_surface_get_toplevel(display->xdg_surface);
      xdg_toplevel_set_app_id(display->xdg_toplevel, app_name);
      xdg_toplevel_add_listener(display->xdg_toplevel, &xdg_toplevel_listener, disp);
      wl_surface_add_listener(display->surface, &surface_listener, wlserv);
#if 0
      xdg_surface_set_window_geometry(display->xdg_surface, x, y, width, height);
#endif

#ifndef COMPAT_LIBVTE
      if (wlserv->decoration_manager) {
        display->toplevel_decoration =
          zxdg_decoration_manager_v1_get_toplevel_decoration(wlserv->decoration_manager,
                                                             display->xdg_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(display->toplevel_decoration,
                                                 &decoration_listener, display);
        zxdg_toplevel_decoration_v1_set_mode(display->toplevel_decoration,
                                             ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      }
#endif
    }
    wl_surface_commit(display->surface);
  } else
#endif
  {
    display->shell_surface = wl_shell_get_shell_surface(wlserv->shell, display->surface);
    wl_shell_surface_set_class(display->shell_surface, app_name);

    if (!display->parent) {
      /* Not input method */

      wl_surface_add_listener(display->surface, &surface_listener, wlserv);
      wl_shell_surface_add_listener(display->shell_surface, &shell_surface_listener, disp);
      wl_shell_surface_set_toplevel(display->shell_surface);
      wl_shell_surface_set_title(display->shell_surface, app_name);
    }
  }
#endif /* COMPAT_LIBVTE */

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

  disp->display = (Display*)(disp + 1);

  if ((p = realloc(displays, sizeof(ui_display_t*) * (num_displays + 1))) == NULL) {
    free(disp);

    return NULL;
  }

  displays = p;
  displays[num_displays] = disp;

  if (*disp_name == '\0') {
    if ((disp_name = getenv("WAYLAND_DISPLAY")) == NULL) {
      disp_name = "wayland-0";
    }
  }

  for (count = 0; count < num_displays; count++) {
    if (strcmp(displays[count]->name, disp_name) == 0) {
      disp->selection_owner = displays[count]->selection_owner;
      disp->clipboard_owner = displays[count]->clipboard_owner;
      wlserv = displays[count]->display->wlserv;
      break;
    }
  }

  if (wlserv == NULL) {
    if ((wlserv = open_wl_display(disp_name)) == NULL) {
      free(disp);

      return NULL;
    }

    if ((p = realloc(wlservs, sizeof(ui_wlserv_t*) * (num_wlservs + 1))) == NULL) {
      close_wl_display(wlserv);
      free(disp);

      return NULL;
    }

    wlservs = p;
    wlservs[num_wlservs++] = wlserv;
  }

  disp->display->wlserv = wlserv;
  disp->name = strdup(disp_name);
  disp->display->rgbinfo = rgbinfo;
  disp->display->bytes_per_pixel = 4;
  disp->depth = 32;

  wlserv->ref_count++;
  num_displays++;

  ui_picture_display_opened(disp->display);

  if (!added_auto_repeat) {
    ui_event_source_add_fd(-10, auto_repeat);
    added_auto_repeat = 1;
  }

  return disp;
}

void ui_display_close(ui_display_t *disp) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if (displays[count] == disp) {
      close_display(disp);
      if (--num_displays == 0) {
        free(displays);
        displays = NULL;
      } else {
        displays[count] = displays[num_displays];
      }

#ifdef __DEBUG
      bl_debug_printf("wayland display connection closed.\n");
#endif

      return;
    }
  }
}

void ui_display_close_all(void) {
  while (num_displays > 0) {
    close_display(displays[0]);
  }
}

ui_display_t **ui_get_opened_displays(u_int *num) {
  *num = num_displays;

  return displays;
}

int ui_display_fd(ui_display_t *disp) { return wl_display_get_fd(disp->display->wlserv->display); }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, char *wm_role, Window parent_window) {
  void *p;

  if (parent_window) {
#ifdef COMPAT_LIBVTE
    disp = add_root_to_display(disp, root, parent_window);
#endif
  } else {
    if (disp->num_roots > 0) {
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

  if ((p = realloc(disp->roots, sizeof(ui_window_t *) * (disp->num_roots + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  disp->roots = p;

  root->disp = disp;
  root->parent = NULL;

  root->gc = disp->gc;

  /*
   * Don't set root->x and root->y to 1 or greater value.
   * root->x and root->y specify the relative position to the display, and
   * the size of the root window is the same as that of the display on wayland.
   */
  root->x = root->y = 0;

  if ((hint & XValue) == 0) {
    x = 0;
  }
  if ((hint & YValue) == 0) {
    y = 0;
  }

  if (app_name) {
    root->app_name = app_name;
  }

  /*
   * root is added to disp->roots before ui_window_show() because
   * ui_display_get_group_leader() is called in ui_window_show().
   */
  disp->roots[disp->num_roots++] = root;

  /* x and y are for xdg_popup and zxdg_popup */
  create_surface(disp, x, y, ACTUAL_WIDTH(root), ACTUAL_HEIGHT(root), root->app_name);

#ifdef COMPAT_LIBVTE
  if (!disp->display->parent) {
    /* do nothing until ui_display_map() except input method */
  } else
#endif
  {
    create_shm_buffer(disp->display);

#ifndef COMPAT_LIBVTE
#ifdef ZXDG_SHELL_V6
    if (disp->display->zxdg_surface || disp->display->zxdg_popup) {
      /* x and y is processed in create_surface() */
    } else
#endif
#ifdef XDG_SHELL
    if (disp->display->xdg_surface || disp->display->xdg_popup) {
      /* x and y is processed in create_surface() */
    } else
#endif
    if (disp->display->parent)
#endif
    {
      /* XXX Input Method */

      /* XXX
       * x and y of wl_shell_surface_set_transient() aren't applied without calling
       * wl_surface_commit() here before calling wl_shell_surface_set_transient().
       */
      wl_surface_commit(disp->display->surface);

      /*
       * XXX wl_shell_surface_set_transient() doesn't move surface to the specified position
       * if it is called before create_shm_buffer().
       */
      ui_display_move(disp, x, y);
    }

    ui_window_show(root, hint);
  }

  /* XXX mlterm -sbmod=none on sway 1.0 stops without calling wl_display_dispatch() here. */
  wl_display_dispatch(disp->display->wlserv->display);

  return 1;
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_roots; count++) {
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
      disp->num_roots--;

      if (count == disp->num_roots) {
        disp->roots[count] = NULL;

        if (disp->num_roots == 0
#ifndef COMPAT_LIBVTE
            && disp->display->parent /* XXX input method alone */
#endif
            ) {
          ui_display_close(disp);
        }
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
#ifdef COMPAT_LIBVTE
  static int display_idling_wait = 4;

  if (--display_idling_wait > 0) {
    goto skip;
  }
  display_idling_wait = 4;
#endif

  for (count = 0; count < disp->num_roots; count++) {
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
#include <errno.h>

int ui_display_receive_next_event(ui_display_t *disp) {
  u_int count;

  ui_display_sync(disp);

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->display->wlserv == disp->display->wlserv) {
      if (displays[count] == disp) {
        if (wl_display_dispatch(disp->display->wlserv->display) == -1 && errno != EAGAIN) {
          close_toplevel_window(disp);

          return 0;
        } else {
          return 1;
        }
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

  for (count = 0; count < num_displays; count++) {
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

  /*
   * ui_display_sync() is called from ui_display_idling() alone on libvte compat library.
   * ui_display_idling() is called with disp in vte.c, not displays in ui_display.c, so
   * call flush_display() with all displays here.
   */
  for (count = 0; count < num_displays; count++) {
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

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->display->wlserv == wlserv) {
      displays[count]->selection_owner = win;
    }
  }

#ifdef GTK_PRIMARY
  if (wlserv->gxsel_device_manager) {
    wlserv->gxsel_source =
      gtk_primary_selection_device_manager_create_source(wlserv->gxsel_device_manager);
    gtk_primary_selection_source_offer(wlserv->gxsel_source, "UTF8_STRING");
    /* gtk_primary_selection_source_offer(wlserv->gxsel_source, "COMPOUND_TEXT"); */
    gtk_primary_selection_source_offer(wlserv->gxsel_source, "TEXT");
    gtk_primary_selection_source_offer(wlserv->gxsel_source, "STRING");
    gtk_primary_selection_source_offer(wlserv->gxsel_source, "text/plain;charset=utf-8");
    gtk_primary_selection_source_offer(wlserv->gxsel_source, "text/plain");
    gtk_primary_selection_source_add_listener(wlserv->gxsel_source,
                                              &gxsel_source_listener, disp);
    gtk_primary_selection_device_set_selection(wlserv->gxsel_device,
                                               wlserv->gxsel_source, wlserv->serial);
  }
#ifdef ZWP_PRIMARY
  else
#endif
#endif
#ifdef ZWP_PRIMARY
  if (wlserv->zxsel_device_manager) {
    wlserv->zxsel_source =
      zwp_primary_selection_device_manager_v1_create_source(wlserv->zxsel_device_manager);
    zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "UTF8_STRING");
    /* zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "COMPOUND_TEXT"); */
    zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "TEXT");
    zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "STRING");
    zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "text/plain;charset=utf-8");
    zwp_primary_selection_source_v1_offer(wlserv->zxsel_source, "text/plain");
    zwp_primary_selection_source_v1_add_listener(wlserv->zxsel_source,
                                                 &zxsel_source_listener, disp);
    zwp_primary_selection_device_v1_set_selection(wlserv->zxsel_device,
                                                  wlserv->zxsel_source, wlserv->serial);
  }
#endif

#ifdef __DEBUG
  bl_debug_printf("set_selection\n");
#endif

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp, /* NULL means all selection owner windows. */
                               ui_window_t *win) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if ((disp == NULL || disp->display->wlserv == displays[count]->display->wlserv) &&
        (displays[count]->selection_owner == win)) {
      displays[count]->selection_owner = NULL;
#ifdef GTK_PRIMARY
      if (displays[count]->display->wlserv->gxsel_source) {
        gtk_primary_selection_source_destroy(displays[count]->display->wlserv->gxsel_source);
        displays[count]->display->wlserv->gxsel_source = NULL;
      }
#ifdef ZWP_PRIMARY
      else
#endif
#endif
#ifdef ZWP_PRIMARY
      if (displays[count]->display->wlserv->zxsel_source) {
        zwp_primary_selection_source_v1_destroy(displays[count]->display->wlserv->zxsel_source);
        displays[count]->display->wlserv->zxsel_source = NULL;
      }
#endif
    }
  }

  if (win->selection_cleared) {
    (*win->selection_cleared)(win);
  }

  return 1;
}

int ui_display_own_clipboard(ui_display_t *disp, ui_window_t *win) {
  ui_wlserv_t *wlserv = disp->display->wlserv;
  u_int count;

  if (disp->clipboard_owner) {
    ui_display_clear_clipboard(NULL, disp->clipboard_owner);
  }

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->display->wlserv == wlserv) {
      displays[count]->clipboard_owner = win;
    }
  }

  if (wlserv->data_device_manager) {
    wlserv->sel_source = wl_data_device_manager_create_data_source(wlserv->data_device_manager);
    wl_data_source_offer(wlserv->sel_source, "UTF8_STRING");
    /* wl_data_source_offer(wlserv->sel_source, "COMPOUND_TEXT"); */
    wl_data_source_offer(wlserv->sel_source, "TEXT");
    wl_data_source_offer(wlserv->sel_source, "STRING");
    wl_data_source_offer(wlserv->sel_source, "text/plain;charset=utf-8");
    wl_data_source_offer(wlserv->sel_source, "text/plain");
    wl_data_source_add_listener(wlserv->sel_source, &data_source_listener, disp);
    wl_data_device_set_selection(wlserv->data_device,
                                 wlserv->sel_source, wlserv->serial);
  }

#ifdef __DEBUG
  bl_debug_printf("set_clipboard\n");
#endif

  return 1;
}

int ui_display_clear_clipboard(ui_display_t *disp, /* NULL means all clipboard owner windows. */
                               ui_window_t *win) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if ((disp == NULL || disp->display->wlserv == displays[count]->display->wlserv) &&
        (displays[count]->clipboard_owner == win)) {
      displays[count]->clipboard_owner = NULL;
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
  if (disp->num_roots > 0) {
    return disp->roots[0]->my_window;
  } else {
    return None;
  }
}

void ui_display_rotate(int rotate) {
  if (num_displays > 0) {
    bl_msg_printf("rotate_display option is not changeable.\n");

    return;
  }

  rotate_display = rotate;
}

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
#else /* COMPAT_LIBVTE */
#ifdef ZXDG_SHELL_V6
  if (display->zxdg_toplevel || display->zxdg_popup) {
    /* XXX */
    static int output_msg;
    if (!output_msg) {
      bl_warn_printf("It is impossible to move windows on zxdg_shell for now.\n");
      output_msg = 1;
    }
  } else
#endif
#ifdef XDG_SHELL
  if (display->xdg_toplevel || display->xdg_popup) {
    /* XXX */
    static int output_msg;
    if (!output_msg) {
      bl_warn_printf("It is impossible to move windows on xdg_shell for now.\n");
      output_msg = 1;
    }
  } else
#endif
  if (display->parent) {
    /* input method window */
#ifdef __DEBUG
    bl_debug_printf("Move display (set transient) at %d %d\n", x, y);
#endif
    wl_shell_surface_set_transient(display->shell_surface,
                                   display->parent->display->surface, x, y,
                                   WL_SHELL_SURFACE_TRANSIENT_INACTIVE);
  }
#endif /* COMPAT_LIBVTE */

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

    damage_buffer(display, x, rotate_display > 0 ? y : y - size + 1, 1, size);
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

#ifdef COMPAT_LIBVTE
  if (!display->buffer) {
    return;
  }
#endif

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
  } else {
    ui_wlserv_t *wlserv = disp->display->wlserv;

    if (wlserv->xsel_offer_mime) {
#ifdef GTK_PRIMARY
      if (wlserv->gxsel_offer) {
        receive_data(disp, wlserv->gxsel_offer, wlserv->xsel_offer_mime, 0);
      }
#ifdef ZWP_PRIMARY
      else
#endif
#endif
#ifdef ZWP_PRIMARY
      if (wlserv->zxsel_offer) {
        receive_data(disp, wlserv->zxsel_offer, wlserv->xsel_offer_mime, 0);
      }
#endif
    }
  }
}

void ui_display_request_text_clipboard(ui_display_t *disp) {
  if (disp->clipboard_owner) {
    XSelectionRequestEvent ev;

    ev.type = 0;
    ev.target = disp->roots[0];
    if (disp->clipboard_owner->utf_selection_requested) {
      /* utf_selection_requested() calls ui_display_send_text_selection() */
      (*disp->clipboard_owner->utf_selection_requested)(disp->clipboard_owner, &ev, 0);
    }
  } else {
    ui_wlserv_t *wlserv = disp->display->wlserv;

    if (wlserv->sel_offer && wlserv->sel_offer_mime) {
      receive_data(disp, wlserv->sel_offer, wlserv->sel_offer_mime, 1);
    }
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

size_t ui_display_get_utf8(u_char *utf8 /* 7 bytes (UTF_MAX_SIZE + 1) */, KeySym ksym) {
  /* xkb_keysym_to_utf8() appends '\0' to utf8 bytes and increments len. */
  size_t len = xkb_keysym_to_utf8(ksym, utf8, 7);

  if (len > 0) {
    return len - 1;
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

void ui_display_set_maximized(ui_display_t *disp, int flag) {
#ifndef COMPAT_LIBVTE
  if (flag) {
#ifdef ZXDG_SHELL_V6
    if (disp->display->zxdg_toplevel) {
      zxdg_toplevel_v6_set_maximized(disp->display->zxdg_toplevel);
    } else
#endif
#ifdef XDG_SHELL
    if (disp->display->xdg_toplevel) {
      xdg_toplevel_set_maximized(disp->display->xdg_toplevel);
    } else
#endif
    {
      wl_shell_surface_set_maximized(disp->display->shell_surface, disp->display->wlserv->output);
    }
  }
#endif
}

void ui_display_set_title(ui_display_t *disp, const u_char *name) {
#ifndef COMPAT_LIBVTE
#ifdef ZXDG_SHELL_V6
  if (disp->display->zxdg_toplevel) {
    zxdg_toplevel_v6_set_title(disp->display->zxdg_toplevel, name);
  } else
#endif
#ifdef XDG_SHELL
  if (disp->display->xdg_toplevel) {
    xdg_toplevel_set_title(disp->display->xdg_toplevel, name);
  } else
#endif
  {
    wl_shell_surface_set_title(disp->display->shell_surface, name);
  }
#endif
}

void ui_display_set_use_text_input(ui_display_t *disp, int use) {
  if (use) {
    if (disp->display->text_input == NULL) {
      disp->display->text_input =
        zwp_text_input_manager_v3_get_text_input(disp->display->wlserv->text_input_manager,
                                                 disp->display->wlserv->seat);
      zwp_text_input_v3_add_listener(disp->display->text_input, &text_input_listener, disp);
    }
  } else {
    if (disp->display->text_input) {
      zwp_text_input_v3_destroy(disp->display->text_input);
      disp->display->text_input = NULL;

      free(disp->display->preedit_text);
      disp->display->preedit_text = NULL;
    }
  }
}

void ui_display_set_text_input_spot(ui_display_t *disp, int x, int y, u_int line_height) {
  disp->display->text_input_x = x;
  disp->display->text_input_y = y;
  disp->display->text_input_line_height = line_height;
  zwp_text_input_v3_set_cursor_rectangle(disp->display->text_input, x, y, 0, line_height);
  zwp_text_input_v3_commit(disp->display->text_input);
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

  wlserv->xkb->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  /* See seat_capabilities() */
#if 0
  wlserv->keyboard = wl_seat_get_keyboard(wlserv->seat);
  wl_keyboard_add_listener(wlserv->keyboard, &keyboard_listener, wlserv);

  wlserv->pointer = wl_seat_get_pointer(wlserv->seat);
  wl_pointer_add_listener(wlserv->pointer, &pointer_listener, wlserv);
#endif

  if (wlserv->data_device_manager) {
    wlserv->data_device = wl_data_device_manager_get_data_device(wlserv->data_device_manager,
                                                                 wlserv->seat);
    wl_data_device_add_listener(wlserv->data_device, &data_device_listener, wlserv);
  }

#ifdef GTK_PRIMARY
  if (wlserv->gxsel_device_manager) {
    wlserv->gxsel_device =
      gtk_primary_selection_device_manager_get_device(wlserv->gxsel_device_manager,
                                                      wlserv->seat);
    gtk_primary_selection_device_add_listener(wlserv->gxsel_device,
                                              &gxsel_device_listener, wlserv);
  }
#ifdef ZWP_PRIMARY
  else
#endif
#endif
#ifdef ZWP_PRIMARY
  if (wlserv->zxsel_device_manager) {
    wlserv->zxsel_device =
      zwp_primary_selection_device_manager_v1_get_device(wlserv->zxsel_device_manager,
                                                         wlserv->seat);
    zwp_primary_selection_device_v1_add_listener(wlserv->zxsel_device,
                                                 &zxsel_device_listener, wlserv);
  }
#endif

  if ((wlserv->cursor_theme = wl_cursor_theme_load(NULL, 32, wlserv->shm))) {
    wlserv->cursor[0] = wl_cursor_theme_get_cursor(wlserv->cursor_theme, "xterm");
    wlserv->cursor_surface = wl_compositor_create_surface(wlserv->compositor);
  }

  wlserv->sel_fd = -1;
}

/*
 * dummy == is_initial_allocation()
 * vte_terminal_realize() doesn't call ui_window_resize_with_margin()
 * if is_initial_allocation() is true, so the size of ui_window and vt_term
 * is incorrect and ui_window_update_all() should be never called here.
 * (see vte_terminal_realize())
 */
void ui_display_map(ui_display_t *disp, int dummy) {
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
                                      NOTIFY_TO_MYSELF) && !dummy) {
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
