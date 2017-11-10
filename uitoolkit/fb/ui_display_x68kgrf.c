/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <dev/wscons/wsdisplay_usl_io.h> /* VT_GETSTATE */
#include <machine/grfioctl.h>
#include <machine/vuid_event.h>
#include <dev/sun/kbio.h>

#define get_key_state() (0)

typedef struct fb_reg {
  /* CRT controller */
  struct {
    u_short r00, r01, r02, r03, r04, r05, r06, r07;
    u_short r08, r09, r10, r11, r12, r13, r14, r15;
    u_short r16, r17, r18, r19, r20, r21, r22, r23;
    char pad0[0x450];
    u_short ctrl;
    char pad1[0x1b7e];

  } crtc;

  u_short gpal[256]; /* graphic palette */
  u_short tpal[256]; /* text palette */

  /* video controller */
  struct {
    u_short r0;
    char pad0[0xfe];
    u_short r1;
    char pad1[0xfe];
    u_short r2;
    char pad2[0x19fe];

  } videoc;

  u_short pad0[0xa000];

  /* system port */
  struct {
    u_short r1, r2, r3, r4;
    u_short pad0[2];
    u_short r5, r6;
    u_short pad[0x1ff0];

  } sysport;

} fb_reg_t;

typedef struct fb_reg_conf {
  struct {
    u_short r00, r01, r02, r03, r04, r05, r06, r07, r08, r20;

  } crtc;

  struct {
    u_short r0, r1, r2;

  } videoc;

} fb_reg_conf_t;

/* --- static variables --- */

static int console_id = -1;
u_int fb_width = 768;
u_int fb_height = 512;
u_int fb_depth = 4;
int separate_wall_picture = 1;
static fb_reg_conf_t orig_reg;
static int grf0_fd = -1;
static size_t grf0_len;
static fb_reg_t *grf0_reg;
static u_short *tpal_orig;
static u_short gpal_12_orig;
static int use_tvram_cmap;
static fb_cmap_t *tcmap; /* If NULL, T-VRAM palette is the same as G-VRAM. */
static fb_cmap_t *gcmap;

/* --- static functions --- */

static void close_grf0(void) {
  if (grf0_fd != -1) {
    if (tcmap) {
      free(tcmap);
      tcmap = NULL;
    }

    grf0_reg->gpal[TP_COLOR] = gpal_12_orig;

    if (tpal_orig) {
      memcpy(grf0_reg->tpal, tpal_orig, sizeof(u_short) * 16);
      free(tpal_orig);
    }

    grf0_reg->videoc.r2 = 0x0010;

    munmap(grf0_reg, grf0_len);

    close(grf0_fd);
    grf0_fd = -1;
  }
}

static void setup_reg(fb_reg_t *reg, fb_reg_conf_t *conf) {
  if ((reg->crtc.r20 & 0x3) < (conf->crtc.r20 & 0x3) ||
      ((reg->crtc.r20 & 0x3) == (conf->crtc.r20 & 0x3) &&
       (reg->crtc.r20 & 0x10) < (conf->crtc.r20 & 0x10))) {
    /* to higher resolution */

    reg->crtc.r00 = conf->crtc.r00;
    reg->crtc.r01 = conf->crtc.r01;
    reg->crtc.r02 = conf->crtc.r02;
    reg->crtc.r03 = conf->crtc.r03;
    reg->crtc.r04 = conf->crtc.r04;
    reg->crtc.r05 = conf->crtc.r05;
    reg->crtc.r06 = conf->crtc.r06;
    reg->crtc.r07 = conf->crtc.r07;
    reg->crtc.r20 = conf->crtc.r20;
  } else {
    /* to lower resolution */

    reg->crtc.r20 = conf->crtc.r20;
    reg->crtc.r01 = conf->crtc.r01;
    reg->crtc.r02 = conf->crtc.r02;
    reg->crtc.r03 = conf->crtc.r03;
    reg->crtc.r04 = conf->crtc.r04;
    reg->crtc.r05 = conf->crtc.r05;
    reg->crtc.r06 = conf->crtc.r06;
    reg->crtc.r07 = conf->crtc.r07;
    reg->crtc.r00 = conf->crtc.r00;
  }

  reg->crtc.r08 = conf->crtc.r08;

  reg->videoc.r0 = conf->videoc.r0;
  reg->videoc.r1 = conf->videoc.r1;
  reg->videoc.r2 = conf->videoc.r2;
}

static int open_display(u_int depth) {
  char *dev;
  struct grfinfo vinfo;
  fb_reg_t *reg;
  fb_reg_conf_t *conf;
  fb_reg_conf_t conf_512_512_15 = {{91, 9, 17, 81, 567, 5, 40, 552, 27, 789}, {3, 0x21e4, 0x000f}};
  fb_reg_conf_t conf_512_512_8 = {{91, 9, 17, 81, 567, 5, 40, 552, 27, 277}, {1, 0x21e4, 0x0003}};
  fb_reg_conf_t conf_768_512_4 = {{137, 14, 28, 124, 567, 5, 40, 552, 27, 1046},
                                  {4, 0x24e4 /* Graphic vram is prior to text one. */, 0x0010}};
  fb_reg_conf_t conf_1024_768_4 = {{169, 14, 28, 156, 439, 5, 40, 424, 27, 1050},
                                   {4, 0x21e4, 0x0010}};
  struct rgb_info rgb_info_15bpp = {3, 3, 3, 6, 11, 1};
  struct termios tm;

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open((dev = getenv("FRAMEBUFFER")) ? dev : "/dev/grf1", O_RDWR);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev ? dev : "/dev/grf1");

    return 0;
  }

  bl_file_set_cloexec(_display.fb_fd);

  if (ioctl(_display.fb_fd, GRFIOCGINFO, &vinfo) == -1) {
    goto error;
  }

  _display.smem_len = vinfo.gd_fbsize + vinfo.gd_regsize;

  if ((_display.fb = mmap(NULL, _display.smem_len, PROT_WRITE | PROT_READ, MAP_FILE | MAP_SHARED,
                          _display.fb_fd, (off_t)0)) == MAP_FAILED) {
    bl_error_printf("Retry another mode of resolution and depth.\n");

    goto error;
  }

  reg = _display.fb;

/* XXX Here reg->crtc.rXX are 0 which will slide the screen unexpectedly on
 * exit. */
#if 0
  orig_reg.crtc.r00 = reg->crtc.r00;
  orig_reg.crtc.r01 = reg->crtc.r01;
  orig_reg.crtc.r02 = reg->crtc.r02;
  orig_reg.crtc.r03 = reg->crtc.r03;
  orig_reg.crtc.r04 = reg->crtc.r04;
  orig_reg.crtc.r05 = reg->crtc.r05;
  orig_reg.crtc.r06 = reg->crtc.r06;
  orig_reg.crtc.r07 = reg->crtc.r07;
  orig_reg.crtc.r08 = reg->crtc.r08;
  orig_reg.crtc.r20 = reg->crtc.r20;
  orig_reg.videoc.r0 = reg->videoc.r0;
  orig_reg.videoc.r1 = reg->videoc.r1;
  orig_reg.videoc.r2 = reg->videoc.r2;

  bl_debug_printf(BL_DEBUG_TAG " crtc %d %d %d %d %d %d %d %d %d 0x%x videoc 0x%x 0x%x 0x%x\n",
                  orig_reg.crtc.r00, orig_reg.crtc.r01, orig_reg.crtc.r02, orig_reg.crtc.r03,
                  orig_reg.crtc.r04, orig_reg.crtc.r05, orig_reg.crtc.r06, orig_reg.crtc.r07,
                  orig_reg.crtc.r08, orig_reg.crtc.r20, orig_reg.videoc.r0, orig_reg.videoc.r1,
                  orig_reg.videoc.r2);
#else
  orig_reg = conf_768_512_4;
  orig_reg.videoc.r2 = 0x20;
#endif

  if (fb_depth == 15) {
    conf = &conf_512_512_15;

    _display.width = _disp.width = 512;
    _display.height = _disp.height = 512;
    _disp.depth = 15;

    _display.rgbinfo = rgb_info_15bpp;
  } else {
    if (fb_depth == 8) {
      conf = &conf_512_512_8;

      _display.width = _disp.width = 512;
      _display.height = _disp.height = 512;
      _disp.depth = 8;
    } else /* if( fb_depth == 4) */
    {
      if (fb_width == 1024 && fb_height == 768) {
        conf = &conf_1024_768_4;

        _display.width = _disp.width = 1024;
        _display.height = _disp.height = 768;
      } else {
        conf = &conf_768_512_4;

        _display.width = _disp.width = 768;
        _display.height = _disp.height = 512;
      }

      if (fb_depth == 1) {
        _disp.depth = 1;
      } else {
        _disp.depth = 4;
      }
    }

    if (!cmap_init()) {
      goto error;
    }
  }

  _display.bytes_per_pixel = 2;
  _display.pixels_per_byte = 1;

  _display.line_length = (_disp.width * 2 + 1023) / 1024 * 1024;
  _display.xoffset = 0;
  /* XXX gd_regsize is regarded as multiple of line_length */
  _display.yoffset = vinfo.gd_regsize / _display.line_length;

  setup_reg(reg, conf);

#ifdef ENABLE_DOUBLE_BUFFER
  if (_display.pixels_per_byte > 1 && !(_display.back_fb = malloc(_display.smem_len))) {
    goto error;
  }
#endif

  tcgetattr(STDIN_FILENO, &tm);
  orig_tm = tm;
  tm.c_iflag = tm.c_oflag = 0;
  tm.c_cflag &= ~CSIZE;
  tm.c_cflag |= CS8;
  tm.c_lflag &= ~(ECHO | ISIG | IEXTEN | ICANON);
  tm.c_cc[VMIN] = 1;
  tm.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tm);

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  if ((_display.fd = open("/dev/kbd", O_RDONLY)) >= 0) {
    int mode;

    fcntl(_display.fd, F_SETOWN, getpid());
    fcntl(_display.fd, F_SETFL, O_NONBLOCK | O_ASYNC);
    mode = 1;
    ioctl(_display.fd, KIOCSDIRECT, &mode);
  } else {
    _display.fd = STDIN_FILENO;
  }

  if ((_mouse.fd = open("/dev/mouse", O_RDONLY)) >= 0) {
    int format;

    format = VUID_FIRM_EVENT;
    ioctl(_mouse.fd, VUIDSFORMAT, &format);
    fcntl(_mouse.fd, F_SETOWN, getpid());
    fcntl(_mouse.fd, F_SETFL, O_NONBLOCK | O_ASYNC);
  }

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_mouse.fd != -1) {
    bl_file_set_cloexec(_mouse.fd);

    _mouse.x = _display.width / 2;
    _mouse.y = _display.height / 2;
    _disp_mouse.display = (Display*)&_mouse;

#if 0
    tcgetattr(_mouse.fd, &tm);
    tm.c_iflag = IGNBRK | IGNPAR;
    tm.c_oflag = 0;
    tm.c_lflag = 0;
    tm.c_cc[VTIME] = 0;
    tm.c_cc[VMIN] = 1;
    tm.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
    cfsetispeed(&tm, B1200);
    cfsetospeed(&tm, B1200);
    tcsetattr(_mouse.fd, TCSAFLUSH, &tm);
#endif
  }
#ifdef DEBUG
  else {
    bl_debug_printf(BL_DEBUG_TAG " Failed to open /dev/wsmouse.\n");
  }
#endif
  _disp.display = &_display;

  console_id = get_active_console();

  return 1;

error:
  cmap_final();

  if (_display.fb) {
    setup_reg(reg, &orig_reg);
    munmap(_display.fb, _display.smem_len);
    _display.fb = NULL;
  }

  close(_display.fb_fd);

  ioctl(_display.fb_fd, GRFIOCOFF, 0);

  return 0;
}

static int receive_mouse_event(void) {
#define MS_LEFT 0x7f20     /* left mouse button */
#define MS_MIDDLE 0x7f21   /* middle mouse button */
#define MS_RIGHT 0x7f22    /* right mouse button */
#define LOC_X_DELTA 0x7f80 /* mouse delta-X */
#define LOC_Y_DELTA 0x7f81 /* mouse delta-Y */
#define VKEY_UP 0
#define VKEY_DOWN 1

  Firm_event ev;
  ssize_t len;

  if (console_id != get_active_console()) {
    return 0;
  }

  while ((len = read(_mouse.fd, memset(&ev, 0, sizeof(ev)), sizeof(ev))) > 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " MOUSE event (len)%d (id)%d (val)%d\n", len, ev.id, ev.value);
#endif

    if (ev.value == VKEY_DOWN || ev.value == VKEY_UP) {
      XButtonEvent xev;
      ui_window_t *win;

      if (ev.id == MS_LEFT) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if (ev.id == MS_MIDDLE) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if (ev.id == MS_RIGHT) {
        xev.button = Button3;
        _mouse.button_state = Button3Mask;
      } else {
        continue;
      }

      if (ev.value == VKEY_DOWN) {
        xev.type = ButtonPress;
      } else /* if( ev.value == VKEY_UP) */
      {
        xev.type = ButtonRelease;

        /* Reset button_state in releasing button. */
        _mouse.button_state = 0;
      }

      xev.time = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
      xev.x = _mouse.x;
      xev.y = _mouse.y;
      xev.state = _display.key_state;

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG "Button is %s x %d y %d btn %d time %d\n",
                      xev.type == ButtonPress ? "pressed" : "released", xev.x, xev.y, xev.button,
                      xev.time);
#endif

      if (!check_virtual_kbd(&xev)) {
        win = get_window(xev.x, xev.y);
        xev.x -= win->x;
        xev.y -= win->y;

        ui_window_receive_event(win, &xev);
      }
    } else if (ev.id == LOC_X_DELTA || ev.id == LOC_Y_DELTA) {
      XMotionEvent xev;
      ui_window_t *win;

      restore_hidden_region();

      if (ev.id == LOC_X_DELTA) {
        _mouse.x += ((int)ev.value * 2);

        if (_mouse.x < 0) {
          _mouse.x = 0;
        } else if (_display.width <= _mouse.x) {
          _mouse.x = _display.width - 1;
        }
      } else /* if( ev.id == LOC_Y_DELTA) */
      {
        _mouse.y -= ((int)ev.value * 2);

        if (_mouse.y < 0) {
          _mouse.y = 0;
        } else if (_display.height <= _mouse.y) {
          _mouse.y = _display.height - 1;
        }
      }

      update_mouse_cursor_state();

      xev.type = MotionNotify;
      xev.x = _mouse.x;
      xev.y = _mouse.y;
      xev.time = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
      xev.state = _mouse.button_state | _display.key_state;

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Button is moved %d x %d y %d btn %d time %d\n", xev.type,
                      xev.x, xev.y, xev.state, xev.time);
#endif

      win = get_window(xev.x, xev.y);
      xev.x -= win->x;
      xev.y -= win->y;

      ui_window_receive_event(win, &xev);

      save_hidden_region();
      draw_mouse_cursor();
    }
  }

  return 1;
}

static int receive_key_event(void) {
  static u_int16_t keymap[] = {
      XK_Escape, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', '\\', XK_BackSpace,
      XK_Tab, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', '[', XK_Return, 'a', 's', 'd',
      'f', 'g', 'h', 'j', 'k', 'l', ';', ':', ']', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
      0, ' ', XK_Home, XK_Delete, XK_Prior, XK_Next, 0 /* XK_Undo */, XK_Left, XK_Up, XK_Right,
      XK_Down, XK_Clear, XK_KP_Divide, XK_KP_Multiply, XK_KP_Subtract, XK_KP_7, XK_KP_8, XK_KP_9,
      XK_KP_Add, XK_KP_4, XK_KP_5, XK_KP_6, '=' /* XK_KP_Equal */, XK_KP_1, XK_KP_2, XK_KP_3,
      XK_Return /* XK_KP_Enter */, XK_KP_0, ',', '.', 0, 0 /* XK_Touroku */, XK_Help, XK_Alt_L,
      XK_Meta_L, XK_Meta_R, XK_Alt_R, XK_Control_R, 0 /* XK_Kana_Lock */, 0 /* XK_Romaji */, 0,
      XK_Caps_Lock, XK_Insert, XK_Hiragana_Katakana, XK_Zenkaku_Hankaku, 0 /* XK_Break */, XK_Print,
      XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6, XK_F7, XK_F8, XK_F9, XK_F10, 0, 0, 0, XK_Shift_L,
      XK_Control_L, XK_Super_L, XK_Super_R,
  };
  static u_int8_t shift_keymap[] = {
      0,   '!', '\"', '#', '$', '%', '&', '\'', '(', ')', 0,   '=', '~', '|', 0,   0,   'Q', 'W',
      'E', 'R', 'T',  'Y', 'U', 'I', 'O', 'P',  '`', '{', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J',
      'K', 'L', '+',  '*', '}', 'Z', 'X', 'C',  'V', 'B', 'N', 'M', '<', '>', '?', '_',
  };
  Firm_event ev;

  if (_display.fd == STDIN_FILENO) {
    return receive_stdin_key_event();
  }

  if (console_id != get_active_console()) {
    return 0;
  }

  while (read(_display.fd, memset(&ev, 0, sizeof(ev)), sizeof(ev)) > 0) {
    XKeyEvent xev;
    int pressed;
    int shift;

    pressed = (ev.value == VKEY_DOWN);

    ev.id--;
    ev.id &= 0x7f;

    if (_display.lock_state & CLKED) {
      shift = !(_display.key_state & ShiftMask);
    } else {
      shift = _display.key_state & ShiftMask;
    }

    if (!shift || ev.id >= sizeof(shift_keymap) / sizeof(*shift_keymap) ||
        !(xev.ksym = shift_keymap[ev.id])) {
      if (ev.id >= sizeof(keymap) / sizeof(*keymap) || !(xev.ksym = keymap[ev.id])) {
        continue;
      }
    }

    if (pressed) {
      if (xev.ksym == XK_Shift_R || xev.ksym == XK_Shift_L) {
        _display.key_state |= ShiftMask;
      } else if (xev.ksym == XK_Control_R || xev.ksym == XK_Control_L) {
        _display.key_state |= ControlMask;
      } else if (xev.ksym == XK_Alt_R || xev.ksym == XK_Alt_L || xev.ksym == XK_Meta_R ||
                 xev.ksym == XK_Meta_L) {
        _display.key_state |= Mod1Mask;
      } else if (xev.ksym == XK_Num_Lock) {
        _display.lock_state ^= NLKED;
      } else if (xev.ksym == XK_Caps_Lock) {
        _display.lock_state ^= CLKED;
      } else {
        xev.type = KeyPress;
        xev.state = _mouse.button_state | _display.key_state;
        xev.keycode = ev.id + 1;

#ifdef __DEBUG
        bl_debug_printf(BL_DEBUG_TAG "scancode %d -> ksym 0x%x state 0x%x\n", ev.id, xev.ksym,
                        xev.state);
#endif

        receive_event_for_multi_roots(&xev);
      }
    } else {
      if (xev.ksym == XK_Shift_R || xev.ksym == XK_Shift_L) {
        _display.key_state &= ~ShiftMask;
      } else if (xev.ksym == XK_Control_R || xev.ksym == XK_Control_L) {
        _display.key_state &= ~ControlMask;
      } else if (xev.ksym == XK_Alt_R || xev.ksym == XK_Alt_L || xev.ksym == XK_Meta_R ||
                 xev.ksym == XK_Meta_L) {
        _display.key_state &= ~Mod1Mask;
      }
    }
  }

  return 1;
}

static int gpal_init(u_short *gpal) {
  if (grf0_fd != -1) {
    u_long color;

    if (ui_cmap_get_closest_color(&color, 0, 0, 0)) {
      /* Opaque black */
      gpal[color] |= 0x1;
    }

    /* Transparent (Wall paper is visible) */
    gpal_12_orig = gpal[TP_COLOR];
    gpal[TP_COLOR] = 0x0;
  }

  return 1;
}

static void x68k_set_use_tvram_colors(int use) {
  if (separate_wall_picture && _disp.depth == 4 && use) {
    if (tcmap) {
      free(tcmap);
      tcmap = NULL;
    }

    use_tvram_cmap = 1;
  } else {
    if (_display.cmap == tcmap) {
      _display.cmap = gcmap;

      if (_display.color_cache) {
        memset(_display.color_cache, 0, sizeof(*_display.color_cache));
      }
    }

    use_tvram_cmap = 0;
  }
}

static fb_cmap_t *cmap_new(int num_colors);

static int x68k_set_tvram_cmap(u_int32_t *pixels, u_int cmap_size) {
  if (use_tvram_cmap && cmap_size <= 16) {
    if ((tcmap = cmap_new(cmap_size))) {
      u_int count;

      for (count = 0; count < cmap_size; count++) {
        tcmap->red[count] = (pixels[count] >> 16) & 0xff;
        tcmap->green[count] = (pixels[count] >> 8) & 0xff;
        tcmap->blue[count] = pixels[count] & 0xff;
      }

      gcmap = _display.cmap;
      _display.cmap = tcmap;

      if (_display.color_cache) {
        memset(_display.color_cache, 0, sizeof(*_display.color_cache));
      }

      return 1;
    }
  }

  return 0;
}

/* --- global functions --- */

int x68k_tvram_is_enabled(void) { return (grf0_fd != -1) ? 1 : 0; }

/*
 * On success, if /dev/grf0 is opened just now, 2 is returned, while if
 * /dev/grf0 has been already opened, 1 is returned.
 */
int x68k_tvram_set_wall_picture(u_short *image, u_int width, u_int height) {
  int ret;
  static u_char *vram;
  u_char *pl0;
  u_char *pl1;
  u_char *pl2;
  u_char *pl3;
  u_short *img;
  int y;
  int img_y;

  if (!separate_wall_picture || _disp.depth != 4 || width < 8 || !image) {
    close_grf0();

    return 0;
  }

  ret = 1;

  while (grf0_fd == -1) {
    struct grfinfo vinfo;

    bl_priv_restore_euid();
    bl_priv_restore_egid();

    grf0_fd = open("/dev/grf0", O_RDWR);

    bl_priv_change_euid(bl_getuid());
    bl_priv_change_egid(bl_getgid());

    if (grf0_fd >= 0) {
      bl_file_set_cloexec(grf0_fd);

      if (ioctl(grf0_fd, GRFIOCGINFO, &vinfo) >= 0) {
        grf0_len = vinfo.gd_fbsize + vinfo.gd_regsize;

        if ((grf0_reg = mmap(NULL, grf0_len, PROT_WRITE | PROT_READ, MAP_FILE | MAP_SHARED, grf0_fd,
                             (off_t)0)) != MAP_FAILED) {
          /* Enale the text vram. */
          grf0_reg->videoc.r2 = 0x0030;

          /* Initialize scroll registers. */
          grf0_reg->crtc.r10 = grf0_reg->crtc.r11 = 0;

          grf0_reg->crtc.r21 = 0;

          if ((tpal_orig = malloc(sizeof(u_short) * 16))) {
            memcpy(tpal_orig, grf0_reg->tpal, sizeof(u_short) * 16);
          }

          vram = ((u_char*)grf0_reg) + vinfo.gd_regsize;

          gpal_init(grf0_reg->gpal);

          ret = 2;

          break;
        }
      }

      close(grf0_fd);
      grf0_fd = -1;
    }

    return 0;
  }

  bl_msg_printf("Wall picture on Text VRAM. %s\n", tcmap ? "" : "(ANSI 16 colors)");

  if (tcmap) {
    u_int count;

    for (count = 0; count < CMAP_SIZE(tcmap); count++) {
      grf0_reg->tpal[count] = (tcmap->red[count] >> 3) << 6 | (tcmap->green[count] >> 3) << 11 |
                              (tcmap->blue[count] >> 3) << 1;
    }

    free(tcmap);
    tcmap = NULL;
  } else {
    /* Reset text palette. */
    memcpy(grf0_reg->tpal, grf0_reg->gpal, sizeof(u_short) * 16);
    grf0_reg->tpal[TP_COLOR] = gpal_12_orig;
  }

  pl0 = vram;
  pl1 = pl0 + 0x20000;
  pl2 = pl1 + 0x20000;
  pl3 = pl2 + 0x20000;
  img = image;

  /* Don't use _disp.height because this function doesn't concern display
   * rotation. */
  for (y = 0, img_y = 0; y < _display.height; y++, img_y++) {
    int x;
    int img_x;

    if (img_y >= height) {
      img = image;
      img_y = 0;
    }

    img_x = 0;

    /* 128 bytes per line */
    for (x = 0; x < 128; x++) {
      *(pl3++) = ((img[img_x] & 0x8) << 4) | ((img[img_x + 1] & 0x8) << 3) |
                 ((img[img_x + 2] & 0x8) << 2) | ((img[img_x + 3] & 0x8) << 1) |
                 (img[img_x + 4] & 0x8) | ((img[img_x + 5] & 0x8) >> 1) |
                 ((img[img_x + 6] & 0x8) >> 2) | ((img[img_x + 7] & 0x8) >> 3);
      *(pl2++) = ((img[img_x] & 0x4) << 5) | ((img[img_x + 1] & 0x4) << 4) |
                 ((img[img_x + 2] & 0x4) << 3) | ((img[img_x + 3] & 0x4) << 2) |
                 ((img[img_x + 4] & 0x4) << 1) | (img[img_x + 5] & 0x4) |
                 ((img[img_x + 6] & 0x4) >> 1) | ((img[img_x + 7] & 0x4) >> 2);
      *(pl1++) = ((img[img_x] & 0x2) << 6) | ((img[img_x + 1] & 0x2) << 5) |
                 ((img[img_x + 2] & 0x2) << 4) | ((img[img_x + 3] & 0x2) << 3) |
                 ((img[img_x + 4] & 0x2) << 2) | ((img[img_x + 5] & 0x2) << 1) |
                 (img[img_x + 6] & 0x2) | ((img[img_x + 7] & 0x2) >> 1);
      *(pl0++) = ((img[img_x] & 0x1) << 7) | ((img[img_x + 1] & 0x1) << 6) |
                 ((img[img_x + 2] & 0x1) << 5) | ((img[img_x + 3] & 0x1) << 4) |
                 ((img[img_x + 4] & 0x1) << 3) | ((img[img_x + 5] & 0x1) << 2) |
                 ((img[img_x + 6] & 0x1) << 1) | (img[img_x + 7] & 0x1);

      if ((img_x += 8) >= width) {
        /* XXX tiling with chopping the last 7 or less pixels. */
        img_x = 0;
      }
    }

    img += width;
  }

  if (y < 1024) {
    u_long color;

    if (ui_cmap_get_closest_color(&color, 0, 0, 0)) {
      size_t len;

      len = (1024 - y) * 128;
      memset(pl3, (color & 0x8) ? 0xff : 0, len);
      memset(pl2, (color & 0x4) ? 0xff : 0, len);
      memset(pl1, (color & 0x2) ? 0xff : 0, len);
      memset(pl0, (color & 0x1) ? 0xff : 0, len);
    }
  }

  return ret;
}
