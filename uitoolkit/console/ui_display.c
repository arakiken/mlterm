/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_display.h"

#include <stdio.h>     /* printf */
#include <unistd.h>    /* STDIN_FILENO */
#include <fcntl.h>     /* fcntl */
#include <sys/ioctl.h> /* ioctl */
#include <string.h>    /* memset/memcpy */
#include <stdlib.h>    /* getenv */
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h> /* select */

#include <pobl/bl_debug.h>
#include <pobl/bl_privilege.h> /* bl_priv_change_e(u|g)id */
#include <pobl/bl_unistd.h>    /* bl_getuid */
#include <pobl/bl_file.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_conf_io.h>
#include <pobl/bl_net.h>
#include <pobl/bl_util.h>

#include <vt_color.h>

#ifdef __linux__
#include <linux/keyboard.h>
#endif

#ifdef USE_LIBSIXEL
#include <sixel.h>
#endif

#include "../ui_window.h"
#include "../ui_picture.h"

/* --- static variables --- */

static ui_display_t **displays;
static u_int num_of_displays;

static struct termios orig_tm;

static vt_char_encoding_t encoding = VT_UTF8;

static u_int default_col_width = 8;
static u_int default_line_height = 16;

/* --- static functions --- */

static void set_blocking(int fd, int block) {
  fcntl(fd, F_SETFL,
        block ? (fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK) : (fcntl(fd, F_GETFL, 0) | O_NONBLOCK));
}

static int set_winsize(ui_display_t *disp, char *seq) {
  struct winsize ws;

  memset(&ws, 0, sizeof(ws));

  if (seq) {
    int col;
    int row;
    int x;
    int y;

    if (sscanf(seq, "8;%d;%d;4;%d;%dt", &row, &col, &y, &x) != 4) {
      return 0;
    }

    ws.ws_col = col;
    ws.ws_row = row;
    disp->width = x;
    disp->height = y;
  } else {
    if (ioctl(fileno(disp->display->fp), TIOCGWINSZ, &ws) == 0) {
      disp->width = ws.ws_xpixel;
      disp->height = ws.ws_ypixel;
    }
  }

  if (ws.ws_col == 0) {
    bl_error_printf("winsize.ws_col is 0\n");
    ws.ws_col = 80;
  }

  if (ws.ws_row == 0) {
    bl_error_printf("winsize.ws_row is 0\n");
    ws.ws_row = 24;
  }

  if (disp->width == 0) {
    disp->width = ws.ws_col * default_col_width;
    disp->display->col_width = default_col_width;
  } else {
    disp->display->col_width = disp->width / ws.ws_col;
    disp->width = disp->display->col_width * ws.ws_col;
  }

  if (disp->height == 0) {
    disp->height = ws.ws_row * default_line_height;
    disp->display->line_height = default_line_height;
  } else {
    disp->display->line_height = disp->height / ws.ws_row;
    disp->height = disp->display->line_height * ws.ws_row;
  }

  bl_msg_printf("Screen is %dx%d (Cell %dx%d)\n", disp->width / disp->display->col_width,
                disp->height / disp->display->line_height, disp->display->col_width,
                disp->display->line_height);

  return 1;
}

/* XXX */
int ui_font_cache_unload_all(void);

static void sig_winch(int sig) {
  u_int count;

  set_winsize(displays[0], NULL);

  /* XXX */
  ui_font_cache_unload_all();

  for (count = 0; count < displays[0]->num_of_roots; count++) {
    ui_window_resize_with_margin(displays[0]->roots[count], displays[0]->width, displays[0]->height,
                                 NOTIFY_TO_MYSELF);
  }

  signal(SIGWINCH, sig_winch);
}

static ui_display_t *open_display_socket(int fd) {
  void *p;

  if (!(p = realloc(displays, sizeof(ui_display_t *) * (num_of_displays + 1)))) {
    return NULL;
  }

  displays = p;

  if (!(displays[num_of_displays] = calloc(1, sizeof(ui_display_t))) ||
      !(displays[num_of_displays]->display = calloc(1, sizeof(Display)))) {
    free(displays[num_of_displays]);

    return NULL;
  }

  if (!(displays[num_of_displays]->display->fp = fdopen(fd, "w"))) {
    free(displays[num_of_displays]->display);
    free(displays[num_of_displays]);

    return NULL;
  }

  /*
   * Set the close-on-exec flag.
   * If this flag off, this fd remained open until the child process forked in
   * open_screen_intern()(vt_term_open_pty()) close it.
   */
  bl_file_set_cloexec(fd);
  set_blocking(fd, 1);

  write(fd, "\x1b[?25l", 6);
  write(fd, "\x1b[>4;2m", 7);
  write(fd, "\x1b[?1002h\x1b[?1006h", 16);
  write(fd, "\x1b[>c", 4);

  displays[num_of_displays]->display->conv = vt_char_encoding_conv_new(encoding);

  set_winsize(displays[num_of_displays], "8;24;80;4;384;640t");

  return displays[num_of_displays++];
}

static ui_display_t *open_display_console(void) {
  void *p;
  struct termios tio;
  int fd;

  if (num_of_displays > 0 || !isatty(STDIN_FILENO) ||
      !(p = realloc(displays, sizeof(ui_display_t *)))) {
    return NULL;
  }

  displays = p;

  if (!(displays[0] = calloc(1, sizeof(ui_display_t))) ||
      !(displays[0]->display = calloc(1, sizeof(Display)))) {
    free(displays[0]);

    return NULL;
  }

  tcgetattr(STDIN_FILENO, &orig_tm);

  if (!(displays[0]->display->fp = fopen(ttyname(STDIN_FILENO), "r+"))) {
    free(displays[0]->display);
    free(displays[0]);

    return NULL;
  }

  fd = fileno(displays[0]->display->fp);
  bl_file_set_cloexec(fd);

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  write(fd, "\x1b[?25l", 6);
  write(fd, "\x1b[>4;2m", 7);
  write(fd, "\x1b[?1002h\x1b[?1006h", 16);
  write(fd, "\x1b[>c", 4);

  tio = orig_tm;
  tio.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR | IGNCR | IMAXBEL | ISTRIP);
  tio.c_iflag |= IGNBRK;
  tio.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONLRET);
#ifdef ECHOPRT
  tio.c_lflag &=
      ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | ECHOCTL | ECHOPRT | ECHOKE | ECHOCTL | ISIG);
#else
  /* ECHOPRT is not defined on cygwin. */
  tio.c_lflag &= ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | ECHOCTL | ECHOKE | ECHOCTL | ISIG);
#endif
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;

  tcsetattr(fd, TCSANOW, &tio);

  displays[0]->display->conv = vt_char_encoding_conv_new(encoding);

  set_winsize(displays[0], NULL);

  signal(SIGWINCH, sig_winch);

  return displays[num_of_displays++];
}

#ifdef __linux__
static int get_key_state(void) {
  int ret;
  char state;

  state = 6;

  ret = ioctl(STDIN_FILENO, TIOCLINUX, &state);

  if (ret == -1) {
    return 0;
  } else {
    /* ShiftMask and ControlMask is the same. */

    return state | ((state & (1 << KG_ALT)) ? ModMask : 0);
  }
}
#else
#define get_key_state() (0)
#endif

static inline ui_window_t *get_window_intern(ui_window_t *win, int x, int y) {
  u_int count;

  for (count = 0; count < win->num_of_children; count++) {
    ui_window_t *child;

    if ((child = win->children[count])->is_mapped) {
      if (child->x <= x && x < child->x + ACTUAL_WIDTH(child) && child->y <= y &&
          y < child->y + ACTUAL_HEIGHT(child)) {
        return get_window_intern(child, x - child->x, y - child->y);
      }
    }
  }

  return win;
}

/*
 * disp->roots[1] is ignored.
 */
static inline ui_window_t *get_window(ui_display_t *disp, int x, /* X in display */
                                      int y                      /* Y in display */
                                      ) {
  return get_window_intern(disp->roots[0], x, y);
}

/* XXX defined in console/ui_window.c */
int ui_window_clear_margin_area(ui_window_t *win);

static void expose_window(ui_window_t *win, int x, int y, u_int width, u_int height) {
  if (x + width <= win->x || win->x + ACTUAL_WIDTH(win) < x || y + height <= win->y ||
      win->y + ACTUAL_HEIGHT(win) < y) {
    return;
  }

  if (x < win->x + win->hmargin || y < win->y + win->vmargin ||
      x - win->x + width > win->hmargin + win->width ||
      y - win->y + height > win->vmargin + win->height) {
    ui_window_clear_margin_area(win);
  }

  if (win->window_exposed) {
    if (x < win->x + win->hmargin) {
      width -= (win->x + win->hmargin - x);
      x = 0;
    } else {
      x -= (win->x + win->hmargin);
    }

    if (y < win->y + win->vmargin) {
      height -= (win->y + win->vmargin - y);
      y = 0;
    } else {
      y -= (win->y + win->vmargin);
    }

    (*win->window_exposed)(win, x, y, width, height);
  }
}

static void expose_display(ui_display_t *disp, int x, int y, u_int width, u_int height) {
  u_int count;

  /*
   * XXX
   * ui_im_{status|candidate}_screen can exceed display width or height,
   * because ui_im_{status|candidate}_screen_new() shows screen at
   * non-adjusted position.
   */

  if (x + width > disp->width) {
    width = disp->width - x;
  }

  if (y + height > disp->height) {
    height = disp->height - y;
  }

  expose_window(disp->roots[0], x, y, width, height);

  for (count = 0; count < disp->roots[0]->num_of_children; count++) {
    expose_window(disp->roots[0]->children[count], x, y, width, height);
  }
}

static int check_visibility_of_im_window(ui_display_t *disp) {
  static struct {
    int saved;
    int x;
    int y;
    u_int width;
    u_int height;

  } im_region;
  int redraw_im_win;

  redraw_im_win = 0;

  if (disp->num_of_roots == 2 && disp->roots[1]->is_mapped) {
    if (im_region.saved) {
      if (im_region.x == disp->roots[1]->x && im_region.y == disp->roots[1]->y &&
          im_region.width == ACTUAL_WIDTH(disp->roots[1]) &&
          im_region.height == ACTUAL_HEIGHT(disp->roots[1])) {
        return 0;
      }

      if (im_region.x < disp->roots[1]->x || im_region.y < disp->roots[1]->y ||
          im_region.x + im_region.width > disp->roots[1]->x + ACTUAL_WIDTH(disp->roots[1]) ||
          im_region.y + im_region.height > disp->roots[1]->y + ACTUAL_HEIGHT(disp->roots[1])) {
        expose_display(disp, im_region.x, im_region.y, im_region.width, im_region.height);
        redraw_im_win = 1;
      }
    }

    im_region.saved = 1;
    im_region.x = disp->roots[1]->x;
    im_region.y = disp->roots[1]->y;
    im_region.width = ACTUAL_WIDTH(disp->roots[1]);
    im_region.height = ACTUAL_HEIGHT(disp->roots[1]);
  } else {
    if (im_region.saved) {
      expose_display(disp, im_region.x, im_region.y, im_region.width, im_region.height);
      im_region.saved = 0;
    }
  }

  return redraw_im_win;
}

static void receive_event_for_multi_roots(ui_display_t *disp, XEvent *xev) {
  int redraw_im_win;

  if ((redraw_im_win = check_visibility_of_im_window(disp))) {
    /* Stop drawing input method window */
    disp->roots[1]->is_mapped = 0;
  }

  ui_window_receive_event(disp->roots[0], xev);

  if (redraw_im_win && disp->num_of_roots == 2) {
    /* Restart drawing input method window */
    disp->roots[1]->is_mapped = 1;
  } else if (!check_visibility_of_im_window(disp)) {
    return;
  }

  expose_window(disp->roots[1], disp->roots[1]->x, disp->roots[1]->y, ACTUAL_WIDTH(disp->roots[1]),
                ACTUAL_HEIGHT(disp->roots[1]));
}

static int receive_stdin(Display *display) {
  ssize_t len;

  if ((len = read(fileno(display->fp), display->buf + display->buf_len,
                  sizeof(display->buf) - display->buf_len - 1)) > 0) {
    display->buf_len += len;
    display->buf[display->buf_len] = '\0';

    return 1;
  } else {
    return 0;
  }
}

static u_char *skip_range(u_char *seq, int beg, int end) {
  while (beg <= *seq && *seq <= end) {
    seq++;
  }

  return seq;
}

static int parse(u_char **param, u_char **intermed, u_char **ft, u_char *seq) {
  *param = seq;

  if ((*intermed = skip_range(*param, 0x30, 0x3f)) == *param) {
    *param = NULL;
  }

  if ((*ft = skip_range(*intermed, 0x20, 0x2f)) == *intermed) {
    *intermed = NULL;
  }

  if (0x40 <= **ft && **ft <= 0x7e) {
    return 1;
  } else {
    return 0;
  }
}

static int parse_modify_other_keys(XKeyEvent *kev, const char *param, const char *format,
                                   int key_mod_order) {
  int key;
  int modcode;

  if (sscanf(param, format, &key, &modcode) == 2) {
    if (!key_mod_order) {
      int tmp;

      tmp = key;
      key = modcode;
      modcode = tmp;
    }

    kev->ksym = key;

    modcode--;
    if (modcode & 1) {
      kev->state |= ShiftMask;
    }
    if (modcode & (2 | 8)) {
      kev->state |= ModMask;
    }
    if (modcode & 4) {
      kev->state |= ControlMask;
    }

    return 1;
  } else {
    return 0;
  }
}

/* Same as fb/ui_display */
static int receive_stdin_event(ui_display_t *disp) {
  u_char *p;

  if (!receive_stdin(disp->display)) {
    u_int count;

    for (count = disp->num_of_roots; count > 0; count--) {
      if (disp->roots[count - 1]->window_deleted) {
        (*disp->roots[count - 1]->window_deleted)(disp->roots[count - 1]);
      }
    }

    return 0;
  }

  p = disp->display->buf;

  while (p - disp->display->buf < disp->display->buf_len) {
    XKeyEvent kev;
    u_char *param;
    u_char *intermed;
    u_char *ft;

    kev.type = KeyPress;
    kev.state = get_key_state();
    kev.ksym = 0;
    kev.keycode = 0;

    if (*p == '\x1b' && p[1] == '\x0') {
      fd_set fds;
      struct timeval tv;

      FD_ZERO(&fds);
      FD_SET(fileno(disp->display->fp), &fds);
      tv.tv_usec = 50000; /* 0.05 sec */
      tv.tv_sec = 0;

      if (select(fileno(disp->display->fp) + 1, &fds, NULL, NULL, &tv) == 1) {
        receive_stdin(disp->display);
      }
    }

    if (*p == '\x1b' && (p[1] == '[' || p[1] == 'O')) {
      if (p[1] == '[') {
        u_char *tmp;

        if (!parse(&param, &intermed, &ft, p + 2)) {
          set_blocking(fileno(disp->display->fp), 0);
          if (!receive_stdin(disp->display)) {
            break;
          }

          continue;
        }

        p = ft + 1;

        if (*ft == '~') {
          if (!param || intermed) {
            continue;
          } else if (!parse_modify_other_keys(&kev, param, "27;%d;%d~", 0)) {
            switch (atoi(param)) {
              case 2:
                kev.ksym = XK_Insert;
                break;
              case 3:
                kev.ksym = XK_Delete;
                break;
              case 5:
                kev.ksym = XK_Prior;
                break;
              case 6:
                kev.ksym = XK_Next;
                break;
              case 7:
                kev.ksym = XK_Home;
                break;
              case 8:
                kev.ksym = XK_End;
                break;
              case 17:
                kev.ksym = XK_F6;
                break;
              case 18:
                kev.ksym = XK_F7;
                break;
              case 19:
                kev.ksym = XK_F8;
                break;
              case 20:
                kev.ksym = XK_F9;
                break;
              case 21:
                kev.ksym = XK_F10;
                break;
              case 23:
                kev.ksym = XK_F11;
                break;
              case 24:
                kev.ksym = XK_F12;
                break;
              default:
                continue;
            }
          }
        } else if (*ft == 'M' || *ft == 'm') {
          XButtonEvent bev;
          int state;
          struct timeval tv;
          ui_window_t *win;

          if (*ft == 'M') {
            if (disp->display->is_pressing) {
              bev.type = MotionNotify;
            } else {
              bev.type = ButtonPress;
              disp->display->is_pressing = 1;
            }

            if (!param) {
              state = *(p++) - 0x20;
              bev.x = *(p++) - 0x20;
              bev.y = *(p++) - 0x20;

              goto make_event;
            }
          } else {
            bev.type = ButtonRelease;
            disp->display->is_pressing = 0;
          }

          *ft = '\0';
          if (!param || sscanf(param, "<%d;%d;%d", &state, &bev.x, &bev.y) != 3) {
            continue;
          }

        make_event:
          bev.button = (state & 0x2) + 1;
          if (bev.type == MotionNotify) {
            bev.state = Button1Mask << (bev.button - 1);
          }

          bev.x--;
          bev.x *= disp->display->col_width;
          bev.y--;
          bev.y *= disp->display->line_height;

          win = get_window(disp, bev.x, bev.y);
          bev.x -= win->x;
          bev.y -= win->y;

          gettimeofday(&tv, NULL);
          bev.time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
          bev.state = 0;

          set_blocking(fileno(disp->display->fp), 1);
          ui_window_receive_event(win, &bev);
          set_blocking(fileno(disp->display->fp), 0);

          continue;
        } else if (param && set_winsize(disp, param)) {
          u_int count;

          /* XXX */
          ui_font_cache_unload_all();

          for (count = 0; count < disp->num_of_roots; count++) {
            ui_window_resize_with_margin(disp->roots[count], disp->width, disp->height,
                                         NOTIFY_TO_MYSELF);
          }

          continue;
        } else if (param && *ft == 'u') {
          if (!parse_modify_other_keys(&kev, param, "%d;%du", 1)) {
            continue;
          }
        } else if (param && *ft == 'c') {
          int pp;
          int pv;
          int pc;

          /*
           * iTerm2: CSI?0;95;c
           * MacOSX Terminal: CSI?1;2c
           */
          if (sscanf(param + 1, "%d;%d;%dc", &pp, &pv, &pc) >= 2 &&
              (pp >= 41 /* VT420 or later */ ||
               /*
                * xterm 279 or later supports DECSLRM/DECLRMM
                * but mlterm 3.7.1 or before which supports it
                * responses 277.
                */
               pv >= 277)) {
            disp->display->support_hmargin = 1;
          } else {
            bl_msg_printf("Slow scrolling on splitted screens\n");
            disp->display->support_hmargin = 0;
          }

          continue;
        } else if ('P' <= *ft && *ft <= 'S') {
          kev.ksym = XK_F1 + (*ft - 'P');
        }
#ifdef __FreeBSD__
        else if ('Y' <= *ft && *ft <= 'Z') {
          kev.ksym = XK_F1 + (*ft - 'Y');
          kev.state = ShiftMask;
        } else if ('a' <= *ft && *ft <= 'j') {
          kev.ksym = XK_F3 + (*ft - 'a');
          kev.state = ShiftMask;
        } else if ('k' <= *ft && *ft <= 'v') {
          kev.ksym = XK_F1 + (*ft - 'k');
          kev.state = ControlMask;
        } else if ('w' <= *ft && *ft <= 'z') {
          kev.ksym = XK_F1 + (*ft - 'w');
          kev.state = ControlMask | ShiftMask;
        } else if (*ft == '@') {
          kev.ksym = XK_F5;
          kev.state = ControlMask | ShiftMask;
        } else if ('[' <= *ft && *ft <= '\`') {
          kev.ksym = XK_F6 + (*ft - '[');
          kev.state = ControlMask | ShiftMask;
        } else if (*ft == '{') {
          kev.ksym = XK_F12;
          kev.state = ControlMask | ShiftMask;
        }
#endif
        else {
          switch (*ft) {
            case 'A':
              kev.ksym = XK_Up;
              break;
            case 'B':
              kev.ksym = XK_Down;
              break;
            case 'C':
              kev.ksym = XK_Right;
              break;
            case 'D':
              kev.ksym = XK_Left;
              break;
            case 'F':
              kev.ksym = XK_End;
              break;
            case 'H':
              kev.ksym = XK_Home;
              break;
            default:
              continue;
          }
        }

        if (param && (tmp = strchr(param, ';'))) {
          param = tmp + 1;
        }
      } else /* if( p[1] == 'O') */
      {
        if (!parse(&param, &intermed, &ft, p + 2)) {
          set_blocking(fileno(disp->display->fp), 0);
          if (!receive_stdin(disp->display)) {
            break;
          }

          continue;
        }

        p = ft + 1;

        switch (*ft) {
          case 'P':
            kev.ksym = XK_F1;
            break;
          case 'Q':
            kev.ksym = XK_F2;
            break;
          case 'R':
            kev.ksym = XK_F3;
            break;
          case 'S':
            kev.ksym = XK_F4;
            break;
          default:
            continue;
        }
      }

      if (param && '1' <= *param && *param <= '9') {
        int state;

        state = atoi(param) - 1;

        if (state & 0x1) {
          kev.state |= ShiftMask;
        }
        if (state & 0x2) {
          kev.state |= ModMask;
        }
        if (state & 0x4) {
          kev.state |= ControlMask;
        }
      }
    } else {
      kev.ksym = *(p++);

      if ((u_int)kev.ksym <= 0x1f) {
        if (kev.ksym == '\0') {
          /* CTL+' ' instead of CTL+@ */
          kev.ksym = ' ';
        } else if (0x01 <= kev.ksym && kev.ksym <= 0x1a) {
          /* Lower case alphabets instead of upper ones. */
          kev.ksym = kev.ksym + 0x60;
        } else {
          kev.ksym = kev.ksym + 0x40;
        }

        kev.state = ControlMask;
      } else if ('A' <= kev.ksym && kev.ksym <= 'Z') {
        kev.state = ShiftMask;
      }
    }

    set_blocking(fileno(disp->display->fp), 1);
    receive_event_for_multi_roots(disp, &kev);
    set_blocking(fileno(disp->display->fp), 0);
  }

  if ((disp->display->buf_len = disp->display->buf + disp->display->buf_len - p) > 0) {
    memcpy(disp->display->buf, p, disp->display->buf_len + 1);
  }

  set_blocking(fileno(disp->display->fp), 1);

  return 1;
}

/* --- global functions --- */

ui_display_t *ui_display_open(char *disp_name, u_int depth) {
  ui_display_t *disp;

  if (disp_name && strncmp(disp_name, "client:", 7) == 0) {
    disp = open_display_socket(atoi(disp_name + 7));
  } else if (!displays) {
    disp = open_display_console();
  } else {
    return displays[0];
  }

  if (disp) {
    if (!(disp->name = getenv("DISPLAY"))) {
      disp->name = ":0.0";
    }
  }

  return disp;
}

int ui_display_close(ui_display_t *disp) {
  u_int count;

/* inline pictures are alive until vt_term_t is deleted. */
#if 0
  ui_picture_display_closed(disp->display);
#endif

  if (isatty(fileno(disp->display->fp))) {
    tcsetattr(fileno(disp->display->fp), TCSAFLUSH, &orig_tm);
    signal(SIGWINCH, SIG_IGN);
  }

  write(fileno(disp->display->fp), "\x1b[?25h", 6);
  write(fileno(disp->display->fp), "\x1b[>4;0m", 7);
  write(fileno(disp->display->fp), "\x1b[?1002l\x1b[?1006l", 16);
  fclose(disp->display->fp);
  (*disp->display->conv->delete)(disp->display->conv);

  for (count = 0; count < num_of_displays; count++) {
    if (displays[count] == disp) {
      memcpy(displays + count, displays + count + 1,
             sizeof(ui_display_t *) * (num_of_displays - count - 1));
      num_of_displays--;

      break;
    }
  }

  return 1;
}

int ui_display_close_all(void) {
  u_int count;

  for (count = num_of_displays; count > 0; count--) {
    ui_display_close(displays[count - 1]);
  }

  free(displays);
  displays = NULL;

  return 1;
}

ui_display_t **ui_get_opened_displays(u_int *num) {
  *num = num_of_displays;

  return displays;
}

int ui_display_fd(ui_display_t *disp) { return fileno(disp->display->fp); }

int ui_display_show_root(ui_display_t *disp, ui_window_t *root, int x, int y, int hint,
                         char *app_name, Window parent_window /* Ignored */
                         ) {
  void *p;

  if ((p = realloc(disp->roots, sizeof(ui_window_t *) * (disp->num_of_roots + 1))) == NULL) {
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

  /* Cursor is drawn internally by calling ui_display_put_image(). */
  if (!ui_window_show(root, hint)) {
    return 0;
  }

  return 1;
}

int ui_display_remove_root(ui_display_t *disp, ui_window_t *root) {
  u_int count;

  for (count = 0; count < disp->num_of_roots; count++) {
    if (disp->roots[count] == root) {
/* XXX ui_window_unmap resize all windows internally. */
#if 0
      ui_window_unmap(root);
#endif
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

int ui_display_receive_next_event(ui_display_t *disp) { return receive_stdin_event(disp); }

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_t *win) {
  if (disp->selection_owner) {
    ui_display_clear_selection(NULL, NULL);
  }

  disp->selection_owner = win;

  return 1;
}

int ui_display_clear_selection(ui_display_t *disp, /* NULL means all selection owner windows. */
                               ui_window_t *win) {
  if (disp == NULL) {
    u_int count;

    for (count = 0; count < num_of_displays; count++) {
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

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial) { /* dummy */ }

XID ui_display_get_group_leader(ui_display_t *disp) { return None; }

/* XXX for input method window */
void ui_display_reset_input_method_window(void) {
#if 0
  if (displays[0]->num_of_roots == 2 && displays[0]->roots[1]->is_mapped)
#endif
  {
    check_visibility_of_im_window(displays[0]);
    ui_window_clear_margin_area(displays[0]->roots[1]);
  }
}

void ui_display_set_char_encoding(ui_display_t *disp, vt_char_encoding_t e) {
  encoding = e;

  if (disp) {
    (*disp->display->conv->delete)(disp->display->conv);
    disp->display->conv = vt_char_encoding_conv_new(encoding);
  }
}

void ui_display_set_default_cell_size(u_int width, u_int height) {
  default_col_width = width;
  default_line_height = height;
}

#ifdef USE_LIBSIXEL
static int dither_id = BUILTIN_XTERM16;

void ui_display_set_sixel_colors(ui_display_t *disp, const char *colors) {
  if (strcmp(colors, "256") == 0) {
    dither_id = BUILTIN_XTERM256;
  } else if (strcmp(colors, "full") == 0) {
    dither_id = -1;
  } else {
    dither_id = BUILTIN_XTERM16;
  }

  if (disp) {
    if (disp->display->sixel_dither) {
      sixel_dither_destroy(disp->display->sixel_dither);
    }

    if (dither_id == -1) {
      disp->display->sixel_dither = sixel_dither_create(-1);
    } else {
      disp->display->sixel_dither = sixel_dither_get(dither_id);
    }

    sixel_dither_set_pixelformat(disp->display->sixel_dither, PIXELFORMAT_RGBA8888);
  }
}

static int callback(char *data, int size, void *out) { return fwrite(data, 1, size, out); }

void ui_display_output_picture(ui_display_t *disp, u_char *picture, u_int width, u_int height) {
  if (!disp->display->sixel_output) {
    disp->display->sixel_output = sixel_output_create(callback, disp->display->fp);
  }

  if (!disp->display->sixel_dither) {
    if (dither_id == -1) {
      disp->display->sixel_dither = sixel_dither_create(-1);
    } else {
      disp->display->sixel_dither = sixel_dither_get(dither_id);
    }

    sixel_dither_set_pixelformat(disp->display->sixel_dither, PIXELFORMAT_RGBA8888);
  }

  sixel_encode(picture, width, height, 4, disp->display->sixel_dither, disp->display->sixel_output);
}
#endif
