/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_display.h"

#include <string.h> /* memset/memcpy */
#include <stdio.h>
#include <unistd.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>  /* strdup/bl_compare_str */

#include <mef/ef_utf8_parser.h>

#include "../ui_window.h"
#include "../ui_picture.h"
#include "../ui_imagelib.h"
#include "../ui_screen.h" /* update_ime_text */

#ifndef USE_WIN32API
#include <sys/select.h>
#include <pthread.h>
#include <sched.h>
#include "../vtemu/vt_term_manager.h"
#endif

#define START_PRESENT_MSEC 4

#if 0
#define MEASURE_TIME
#endif

#if 0
#define __DEBUG
#endif

#define DECORATION_MARGIN 0 /* for decorataion */

/* --- static variables --- */

static u_int num_displays;
static ui_display_t **displays;
static int rotate_display;
static Uint32 pty_event_type;
static int pty_event_queued;
static Uint32 vsync_interval_msec;
static Uint32 next_vsync_msec;
static ef_parser_t *utf8_parser;
static u_char *cur_preedit_text;

/* --- static functions --- */

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

static void update_ime_text(ui_window_t *uiwindow, const char *preedit_text) {
  vt_term_t *term;

  if (!(uiwindow = search_focused_window(uiwindow)) || !uiwindow->inputtable) {
    return;
  }
  term = ((ui_screen_t*)uiwindow)->term;

  if (cur_preedit_text) {
    vt_term_set_config(term, "use_local_echo", "false");
  }

  (*utf8_parser->init)(utf8_parser);

  if (*preedit_text == '\0') {
    free(cur_preedit_text);
    cur_preedit_text = NULL;
  } else {
    u_char buf[128];
    size_t len;

    if (bl_compare_str(preedit_text, cur_preedit_text) == 0) {
      return;
    }

    vt_term_set_config(term, "use_local_echo", "true");

    (*utf8_parser->set_str)(utf8_parser, preedit_text, strlen(preedit_text));
    while (!utf8_parser->is_eos &&
           (len = vt_term_convert_to(term, buf, sizeof(buf), utf8_parser)) > 0) {
      vt_term_preedit(term, buf, len);
    }

    free(cur_preedit_text);
    cur_preedit_text = strdup(preedit_text);
  }

  ui_window_update(uiwindow, 3); /* UPDATE_SCREEN|UPDATE_CURSOR */
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

static u_int total_width_inc(ui_window_t *win) {
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

  for (count = 0; count < win->num_children; count++) {
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

#ifdef USE_BG_TEXTURE
  if (disp->display->bg_texture) {
    SDL_DestroyTexture(disp->display->bg_texture);
  }
#endif

  SDL_DestroyTexture(disp->display->texture);
  SDL_DestroyWindow(disp->display->window);
  SDL_DestroyRenderer(disp->display->renderer);

  ui_picture_display_closed(disp->display);

  free(disp);
}

static int init_display(Display *display, char *app_name) {
  SDL_Rect rect;

  if (!display->window) {
    if (!(display->window = SDL_CreateWindow(app_name, SDL_WINDOWPOS_CENTERED,
                                             SDL_WINDOWPOS_CENTERED,
                                             display->width, display->height,
                                             SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                                             SDL_WINDOW_INPUT_FOCUS))) {
      return 0;
    }

    if (!(display->renderer = SDL_CreateRenderer(display->window, -1,
                                                 SDL_RENDERER_ACCELERATED
#if 1
                                                 | SDL_RENDERER_PRESENTVSYNC
#endif
                                                 ))) {
      SDL_DestroyWindow(display->window);

      return 0;
    }

    if (vsync_interval_msec == 0) {
      int idx;
      SDL_DisplayMode mode;
      SDL_RendererInfo info;
      Uint32 count;

      idx = SDL_GetWindowDisplayIndex(display->window);
      if (SDL_GetCurrentDisplayMode(idx, &mode) == 0) {
        vsync_interval_msec = 1000 / mode.refresh_rate;
      } else {
        vsync_interval_msec = 16;
      }

      next_vsync_msec = SDL_GetTicks() + vsync_interval_msec;

      SDL_GetRendererInfo(display->renderer, &info);
      bl_msg_printf("SDL2 with %s\n", info.name);

      if (!(info.flags & SDL_RENDERER_ACCELERATED)) {
        bl_msg_printf("SDL2 doesn't use hardware acceleration.\n");
      }

      for (count = 0; count < info.num_texture_formats; count++) {
        if (info.texture_formats[count] == SDL_PIXELFORMAT_ARGB8888) {
          goto next_step;
        }
      }

      bl_error_printf("ARGB8888 is not supported.\n");
    }

  next_step:
    SDL_SetRenderDrawColor(display->renderer, 0xff, 0xff, 0xff, 0xff);
  }

  rect.x = 0;
  rect.y = 0;
  rect.w = display->width;
  rect.h = display->height;
  SDL_RenderSetViewport(display->renderer, &rect);

  if (display->texture) {
    SDL_DestroyTexture(display->texture);
  }

  display->texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       display->width, display->height);
  SDL_SetTextureBlendMode(display->texture, SDL_BLENDMODE_BLEND);

  SDL_LockTexture(display->texture, NULL, &display->fb,
                  &display->line_length);

  return 1;
}

#ifndef USE_WIN32API
static void *monitor_ptys(void *p) {
  vt_term_t **terms;
  u_int num_terms;
  int ptyfd;
  int maxfd = 0;
  fd_set read_fds;
  u_int count;
  SDL_Event ev;

  while (1) {
    while (pty_event_queued || (num_terms = vt_get_all_terms(&terms)) == 0) {
      if (num_displays == 0) {
        return 0;
      }

      sched_yield();
    }

    FD_ZERO(&read_fds);
    for (count = 0; count < num_terms; count++) {
      if ((ptyfd = vt_term_get_master_fd(terms[count])) >= 0) {
        FD_SET(ptyfd, &read_fds);

        if (ptyfd > maxfd) {
          maxfd = ptyfd;
        }
      }
    }

    select(maxfd + 1, &read_fds, NULL, NULL, NULL);

    SDL_zero(ev);
    ev.type = pty_event_type;
    SDL_PushEvent(&ev);
    pty_event_queued = 1;
  }

#ifdef DEBUG
  bl_debug_printf("Finish monitoring pty.\n");
#endif

  return 0;
}
#endif

static ui_display_t *get_display(Uint32 window_id) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if (SDL_GetWindowID(displays[count]->display->window) == window_id) {
      return displays[count];
    }
  }

  return displays[0];
}

static void present_displays(void) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    Display *display = displays[count]->display;

    if (display->damaged) {
#ifdef MEASURE_TIME
      Uint32 msec[5];
      Uint32 old_next_vsync_msec = next_vsync_msec;
      msec[0] = SDL_GetTicks();
#endif

      SDL_UnlockTexture(display->texture);
#ifdef MEASURE_TIME
      msec[1] = SDL_GetTicks();
#endif

#ifdef USE_BG_TEXTURE
      if (display->bg_texture) {
        SDL_RenderCopy(display->renderer, display->bg_texture, NULL, NULL);
      } else {
        SDL_RenderClear(display->renderer);
      }
#endif
      SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);

#ifdef MEASURE_TIME
      msec[2] = SDL_GetTicks();
#endif

      SDL_RenderPresent(display->renderer);
      next_vsync_msec = SDL_GetTicks() + vsync_interval_msec;

#ifdef MEASURE_TIME
      msec[3] = SDL_GetTicks();
#endif

      SDL_LockTexture(display->texture, NULL, &display->fb, &display->line_length);

#ifdef MEASURE_TIME
      msec[4] = SDL_GetTicks();
      bl_msg_printf("%d: %d %d %d %d %d\n",
                    old_next_vsync_msec, msec[0], msec[1], msec[2], msec[3], msec[4]);
#endif

      display->damaged = 0;
    }
  }
}

static void receive_mouse_event(ui_display_t *disp, XButtonEvent *xev) {
  ui_window_t *win;

  if (rotate_display) {
    int tmp = xev->x;
    if (rotate_display > 0) {
      xev->x = xev->y;
      xev->y = disp->display->width - tmp - 1;
    } else {
      xev->x = disp->display->height - xev->y - 1;
      xev->y = tmp;
    }
  }

  win = get_window(disp->roots[0], xev->x, xev->y);
  xev->x -= win->x;
  xev->y -= win->y;

  ui_window_receive_event(win, xev);
}

static u_int get_mod_state(SDL_Keymod mod) {
  u_int state = 0;

  if (mod & KMOD_CTRL) {
    state |= ControlMask;
  }
  if (mod & KMOD_SHIFT) {
    state |= ShiftMask;
  }
  if (mod & KMOD_ALT) {
    state |= Mod1Mask;
  }

  return state;
}

static void poll_event(void) {
  SDL_Event ev;
  XEvent xev;
  ui_display_t *disp;

  if (!SDL_PollEvent(&ev)) {
    Uint32 spent_time = 0;
    Uint32 now;
    Uint32 orig_now;

    do {
      orig_now = now = SDL_GetTicks();
      if (now > next_vsync_msec) {
        while (now > (next_vsync_msec += vsync_interval_msec));
      }
#if 0
      else if (now + vsync_interval_msec < next_vsync_msec) {
        /* SDL_GetTicks() is reset. */
        next_vsync_msec = now;
      }
#endif

      if (next_vsync_msec - now <= START_PRESENT_MSEC) {
        present_displays();
        now = SDL_GetTicks();
        if (now + START_PRESENT_MSEC > next_vsync_msec) {
          while (now + START_PRESENT_MSEC > (next_vsync_msec += vsync_interval_msec));
        }
#if 0
        else if (now + vsync_interval_msec < next_vsync_msec) {
          /* SDL_GetTicks() is reset. */
          next_vsync_msec = now;
        }
#endif
      }

      if (SDL_WaitEventTimeout(&ev, next_vsync_msec - now - START_PRESENT_MSEC)) {
        break;
      }

      if ((spent_time += (next_vsync_msec - orig_now - START_PRESENT_MSEC)) >= 100) {
        u_int count;

        for (count = 0; count < num_displays; count++) {
          ui_display_idling(displays[count]);
        }

        spent_time = 0;
      }
    } while (1);
  }

  switch(ev.type) {
  case SDL_QUIT:
    {
      u_int count;

      disp = get_display(ev.window.windowID);

      for (count = 0; count < disp->num_roots; count++) {
        if (disp->roots[count]->window_deleted) {
          (*disp->roots[count]->window_deleted)(disp->roots[count]);
        }
      }
    }
    
    break;

  case SDL_KEYDOWN:
    xev.xkey.type = KeyPress;
    xev.xkey.time = ev.key.timestamp;
    xev.xkey.ksym = ev.key.keysym.sym;
    xev.xkey.keycode = ev.key.keysym.scancode;
    xev.xkey.str = NULL;
    xev.xkey.parser = NULL;
    xev.xkey.state = get_mod_state(ev.key.keysym.mod);

    if (!cur_preedit_text &&
        (xev.xkey.ksym < 0x20 || xev.xkey.ksym >= 0x7f || xev.xkey.state == ControlMask)) {
      ui_window_receive_event(get_display(ev.key.windowID)->roots[0], &xev);
    }

    break;

  case SDL_TEXTINPUT:
    {
      ui_window_t *win = get_display(ev.text.windowID)->roots[0];

      update_ime_text(win, "");

      xev.xkey.type = KeyPress;
      xev.xkey.time = ev.text.timestamp;
      if (strlen(ev.text.text) == 1) {
        xev.xkey.ksym = ev.text.text[0];
        xev.xkey.keycode = ev.text.text[0];
      } else {
        xev.xkey.ksym = 0;
        xev.xkey.keycode = 0;
      }
      xev.xkey.str = ev.text.text;
      xev.xkey.parser = utf8_parser;
      xev.xkey.state = get_mod_state(SDL_GetModState());

      ui_window_receive_event(win, &xev);
    }
#ifdef DEBUG
    bl_debug_printf("SDL_TEXTINPUT event: %s(%x...)\n", ev.text.text, ev.text.text[0]);
#endif

    break;

  case SDL_TEXTEDITING:
    if (strlen(ev.edit.text) > 0) {
      update_ime_text(get_display(ev.edit.windowID)->roots[0], ev.edit.text);
    }
#ifdef DEBUG
    bl_debug_printf("SDL_TEXTEDITING event: %s(%x...)\n", ev.edit.text, ev.edit.text[0]);
#endif
    break;

  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    disp = get_display(ev.window.windowID);

    if (ev.button.type == SDL_MOUSEBUTTONDOWN) {
      xev.type = ButtonPress;
    } else {
      xev.type = ButtonRelease;
    }
    xev.xbutton.time = ev.button.timestamp;
    if (ev.button.button == SDL_BUTTON_LEFT) {
      xev.xbutton.button = 1;
    } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
      xev.xbutton.button = 2;
    } else if (ev.button.button == SDL_BUTTON_RIGHT) {
      xev.xbutton.button = 3;
    }
    xev.xbutton.state = get_mod_state(SDL_GetModState());

    xev.xbutton.x = ev.button.x;
    xev.xbutton.y = ev.button.y;

    receive_mouse_event(disp, &xev.xbutton);
 
    break;

  case SDL_MOUSEMOTION:
    disp = get_display(ev.window.windowID);

    xev.xmotion.type = MotionNotify;
    xev.xmotion.time = ev.motion.timestamp;
    xev.xmotion.state = 0;
    if (ev.motion.state & SDL_BUTTON_LMASK) {
      xev.xmotion.state |= Button1Mask;
    }
    if (ev.motion.state & SDL_BUTTON_MMASK) {
      xev.xmotion.state |= Button2Mask;
    }
    if (ev.motion.state & SDL_BUTTON_RMASK) {
      xev.xmotion.state |= Button3Mask;
    }

    xev.xmotion.x = ev.motion.x;
    xev.xmotion.y = ev.motion.y;

    receive_mouse_event(disp, &xev.xmotion);

    break;

  case SDL_WINDOWEVENT:
    disp = get_display(ev.window.windowID);

    if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
      u_int width = ev.window.data1;
      u_int height = ev.window.data2;

#if 0
      if (check_resize(disp->display->width, disp->display->height, &width, &height,
                       total_min_width(disp->roots[0]), total_min_height(disp->roots[0]),
                       total_width_inc(disp->roots[0]), total_height_inc(disp->roots[0]), 1)) {
        SDL_SetWindowSize(disp->display->window, width, height);
      } else
#endif
      if (width != disp->display->width || height != disp->display->height) {
        disp->display->width = width;
        disp->display->height = height;
        if (rotate_display) {
          disp->width = height;
          disp->height = width;
        } else {
          disp->width = width;
          disp->height = height;
        }

        init_display(disp->display, NULL);
        ui_window_resize_with_margin(disp->roots[0], disp->width, disp->height, NOTIFY_TO_MYSELF);
      }
    }
    break;

  default:
    if (ev.type == pty_event_type) {
      pty_event_queued = 0;
    }

    break;
  }
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, u_int depth) {
  ui_display_t *disp;
  void *p;
  struct rgb_info rgbinfo = {0, 0, 0, 16, 8, 0};

  if (!utf8_parser && !(utf8_parser = ef_utf8_parser_new())) {
    return NULL;
  }

  if (!(disp = calloc(1, sizeof(ui_display_t) + sizeof(Display)))) {
    return NULL;
  }

  disp->display = disp + 1;

  if ((p = realloc(displays, sizeof(ui_display_t*) * (num_displays + 1))) == NULL) {
    free(disp);

    return NULL;
  }

  displays = p;
  displays[num_displays] = disp;

  if (num_displays == 0) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      return NULL;
    }

    SDL_StartTextInput();

    pty_event_type = SDL_RegisterEvents(1);

#ifndef USE_WIN32API
    {
      pthread_t thrd;

      pthread_create(&thrd, NULL, monitor_ptys, NULL);
    }
#endif
  }

  disp->name = strdup(disp_name);
  disp->display->rgbinfo = rgbinfo;
  disp->display->bytes_per_pixel = 4;
  disp->depth = 32;

  num_displays++;

  ui_picture_display_opened(disp->display);

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

        free(cur_preedit_text);
        cur_preedit_text = NULL;

        (*utf8_parser->delete)(utf8_parser);
        utf8_parser = NULL;

        SDL_Quit();
      } else {
        displays[count] = displays[num_displays];
      }

#ifdef DEBUG
      bl_debug_printf("sdl2 display connection closed.\n");
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
  poll_event();

  *num = 0;

  return NULL;
}

int ui_display_fd(ui_display_t *disp) { return -1; }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, Window parent_window) {
  void *p;

  if (disp->num_roots > 0) {
    /* XXX Input Method */
    ui_display_t *parent = disp;

    disp = ui_display_open(disp->name, disp->depth);
    disp->display->parent = parent;
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
  root->x = 0;
  root->y = 0;

  if (app_name) {
    root->app_name = app_name;
  }

  /*
   * root is added to disp->roots before ui_window_show() because
   * ui_display_get_group_leader() is called in ui_window_show().
   */
  disp->roots[disp->num_roots++] = root;

  disp->width = ACTUAL_WIDTH(root);
  disp->height = ACTUAL_HEIGHT(root);
  if (rotate_display) {
    disp->display->width = disp->height;
    disp->display->height = disp->width;
  } else {
    disp->display->width = disp->width;
    disp->display->height = disp->height;
  }
  init_display(disp->display, root->app_name);

  ui_window_show(root, hint);

  return 1;
}

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

        if (disp->num_roots == 0 && disp->display->parent /* XXX input method alone */) {
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

  for (count = 0; count < disp->num_roots; count++) {
    ui_window_idling(disp->roots[count]);
  }
}

int ui_display_receive_next_event(ui_display_t *disp) { return 0; }

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_t *win) {
  u_int count;

  if (disp->selection_owner) {
    ui_display_clear_selection(NULL, disp->selection_owner);
  }

  for (count = 0; count < num_displays; count++) {
    displays[count]->selection_owner = win;
  }

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp, /* NULL means all selection owner windows. */
                               ui_window_t *win) {
  u_int count;

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->selection_owner == win) {
      displays[count]->selection_owner = NULL;
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

void ui_display_rotate(int rotate) { rotate_display = rotate; }

int ui_display_resize(ui_display_t *disp, u_int width, u_int height) {
  return 0;
}

int ui_display_move(ui_display_t *disp, int x, int y) { return 0; }

u_long ui_display_get_pixel(ui_display_t *disp, int x, int y) {
  u_char *fb;
  u_long pixel;

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
  } else {
    memcpy(get_fb(display, x, y), image, size);
  }

  display->damaged = 1;
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

  display->damaged = 1;
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
  }
}

void ui_display_send_text_selection(ui_display_t *disp, XSelectionRequestEvent *ev,
                                    u_char *sel_data, size_t sel_len) {
  if (ev && ev->target->utf_selection_notified) {
    (*ev->target->utf_selection_notified)(ev->target, sel_data, sel_len);
  }
}

void ui_display_logical_to_physical_coordinates(ui_display_t *disp, int *x, int *y) {
  int global_x;
  int global_y;

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

  SDL_GetWindowPosition(disp->display->window, &global_x, &global_y);

  *x += global_x;
  *y += global_y;
}

#ifdef USE_WIN32API
void ui_display_trigger_pty_read(void) {
  SDL_Event ev;

  SDL_zero(ev);
  ev.type = pty_event_type;
  SDL_PushEvent(&ev);
}
#endif

#ifdef USE_BG_TEXTURE
void ui_display_set_bg_color(ui_display_t *disp, u_long bg) {
  SDL_SetRenderDrawColor(disp->display->renderer, (bg >> 16) & 0xff, (bg >> 8) & 0xff, bg & 0xff,
                         (bg >> 24) & 0xff);
}

void ui_display_set_wall_picture(ui_display_t *disp, u_char *image, u_int width, u_int height) {
  Display *display = disp->display;

  if (display->bg_texture) {
    SDL_DestroyTexture(display->bg_texture);
  }

  if (image) {
    u_char *fb;
    u_int line_length;
    int y;

    display->bg_texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, width, height);
    SDL_SetTextureBlendMode(display->bg_texture, SDL_BLENDMODE_BLEND);
    SDL_LockTexture(display->bg_texture, NULL, &fb, &line_length);

    for (y = 0; y < height; y++) {
      memcpy(fb, image, width * 4);
      fb += line_length;
      image += (width * 4);
    }

    SDL_UnlockTexture(display->bg_texture);
  } else {
    display->bg_texture = NULL;
  }
}
#endif
