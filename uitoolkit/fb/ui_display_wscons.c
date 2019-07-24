/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <sys/param.h>                   /* MACHINE */
#include <dev/wscons/wsdisplay_usl_io.h> /* VT_GETSTATE */

#ifdef __NetBSD__
#include "../ui_event_source.h"
#endif

#ifdef __NetBSD__
#define KEY_REPEAT_UNIT 25       /* msec (see ui_event_source.c) */
#define DEFAULT_KEY_REPEAT_1 400 /* msec */
#define DEFAULT_KEY_REPEAT_N 50  /* msec */
#define DEFAULT_FBDEV "/dev/ttyE0"
#else /* __OpenBSD__ */
#define DEFAULT_FBDEV "/dev/ttyC0"
#endif

#define get_key_state() (0)

/* --- static variables --- */

static u_int kbd_type;
static struct wskbd_map_data keymap;
static int console_id = -1;
static struct wscons_event prev_key_event;
u_int fb_width = 640;
u_int fb_height = 480;
u_int fb_depth = 8;
#ifdef __NetBSD__
static int kbd_repeat_wait = (DEFAULT_KEY_REPEAT_1 + KEY_REPEAT_UNIT / 2) / KEY_REPEAT_UNIT;
int kbd_repeat_1 = DEFAULT_KEY_REPEAT_1;
int kbd_repeat_N = DEFAULT_KEY_REPEAT_N;
#endif
static int orig_console_mode = WSDISPLAYIO_MODE_EMUL; /* 0 */
static int wskbd_mode_switch = 0;

/* --- static functions --- */

/* For iBus which requires ps/2 keycode. */
static u_int get_ps2_kcode(u_int kcode) {
  if (kbd_type == WSKBD_TYPE_USB) {
    static u_char map_table1[] = {
        30,  /* A (4) */
        48,  /* B */
        46,  /* C */
        32,  /* D */
        18,  /* E */
        33,  /* F */
        34,  /* G (10) */
        35,  /* H */
        23,  /* I */
        36,  /* J */
        37,  /* K */
        38,  /* L */
        50,  /* M */
        49,  /* N */
        24,  /* O */
        25,  /* P */
        16,  /* Q (20) */
        19,  /* R */
        31,  /* S */
        20,  /* T */
        22,  /* U */
        47,  /* V */
        17,  /* W */
        45,  /* X */
        21,  /* Y */
        44,  /* Z */
        2,   /* 1 (30) */
        3,   /* 2 */
        4,   /* 3 */
        5,   /* 4 */
        6,   /* 5 */
        7,   /* 6 */
        8,   /* 7 */
        9,   /* 8 */
        10,  /* 9 */
        11,  /* 0 */
        28,  /* Enter (40) */
        1,   /* ESC */
        14,  /* BackSpace */
        15,  /* Tab */
        57,  /* Space */
        12,  /* _ - */
        13,  /* + = */
        26,  /* { [ */
        27,  /* } ] */
        43,  /* \ | */
        0,   /* (50) */
        39,  /* : ; */
        40,  /* " ' */
        41,  /* ~ ` */
        51,  /* < , */
        52,  /* > . */
        53,  /* ? / */
        58,  /* CapsLock */
        59,  /* F1 */
        60,  /* F2 */
        61,  /* F3 (60) */
        62,  /* F4 */
        63,  /* F5 */
        64,  /* F6 */
        65,  /* F7 */
        66,  /* F8 */
        67,  /* F9 */
        68,  /* F10 */
        87,  /* F11 */
        88,  /* F12 */
        0,   /* Print Screen (70) */
        70,  /* ScreenLock */
        0,   /* Pause */
        110, /* Insert */
        102, /* Home */
        104, /* Page Up */
        111, /* Delete */
        107, /* End */
        109, /* Page Down */
        106, /* Right */
        105, /* Left (80) */
        108, /* Down */
        103, /* Up */
        69,  /* NumLock */
        0,   /* Num / */
        55,  /* Num * */
        74,  /* Num - */
        78,  /* Num + */
        0,   /* Num Enter */
        79,  /* Num 1 */
        80,  /* Num 2 (90) */
        81,  /* Num 3 */
        75,  /* Num 4 */
        76,  /* Num 5 */
        77,  /* Num 6 */
        71,  /* Num 7 */
        72,  /* Num 8 */
        73,  /* Num 9 */
        82,  /* Num 0 */
        83,  /* Num . */
    };

    static u_char map_table2[] = {
        29,  /* Control L (224) */
        42,  /* Shift L */
        56,  /* Alt L */
        0,   /* Windows L */
        97,  /* Control R */
        54,  /* Shift R */
        100, /* Alt R (230) */
        0,   /* Windows R */
    };

    if (4 <= kcode) {
      if (kcode <= 99) {
        return map_table1[kcode - 4];
      } else if (224 <= kcode) {
        if (kcode <= 231) {
          return map_table2[kcode - 224];
        }
      }
    }

    return 0;
  } else {
    return kcode;
  }
}

static void process_wskbd_event(struct wscons_event *ev) {
  keysym_t ksym;

  if (keymap.map[ev->value].command == KS_Cmd_ResetEmul) {
    /* XXX */
    ksym = XK_BackSpace;
  } else {
    keysym_t *group;

    if (wskbd_mode_switch) {
      group = keymap.map[ev->value].group2;
    } else {
      group = keymap.map[ev->value].group1;
    }

    if (_display.key_state & ShiftMask) {
      ksym = group[1];
    } else {
      ksym = group[0];
    }

    if (KS_f1 <= ksym && ksym <= KS_f20) {
      /* KS_f1 => KS_F1 */
      ksym += (KS_F1 - KS_f1);
    } else if (_display.lock_state & CLKED) {
      if (KS_a <= ksym && ksym <= KS_z) {
        ksym += (KS_A - KS_a);
      } else if (KS_agrave <= ksym && ksym <= KS_thorn && ksym != KS_division) {
        ksym += (KS_Agrave - KS_agrave);
      }
    }
  }

  if (ev->type == WSCONS_EVENT_KEY_DOWN) {
    if (ksym == KS_Shift_R || ksym == KS_Shift_L) {
      _display.key_state |= ShiftMask;
    } else if (ksym == KS_Caps_Lock) {
      _display.lock_state ^= CLKED;
    } else if (ksym == KS_Control_R || ksym == KS_Control_L) {
      _display.key_state |= ControlMask;
    } else if (ksym == KS_Alt_R || ksym == KS_Alt_L) {
      _display.key_state |= Mod1Mask;
    } else if (ksym == KS_Mode_switch) {
      wskbd_mode_switch = 1;
    } else if (ksym == KS_Num_Lock) {
      _display.lock_state ^= NLKED;
    } else {
      XKeyEvent xev;

      xev.type = KeyPress;
      xev.ksym = ksym;
      xev.state = _mouse.button_state | _display.key_state;
      xev.keycode = get_ps2_kcode(ev->value);

      receive_event_for_multi_roots(&xev);

      prev_key_event = *ev;
#ifdef __NetBSD__
      kbd_repeat_wait = (kbd_repeat_1 + KEY_REPEAT_UNIT / 2) / KEY_REPEAT_UNIT;
#endif
    }
  } else if (ev->type == WSCONS_EVENT_KEY_UP) {
    if (ksym == KS_Shift_R || ksym == KS_Shift_L) {
      _display.key_state &= ~ShiftMask;
    } else if (ksym == KS_Control_R || ksym == KS_Control_L) {
      _display.key_state &= ~ControlMask;
    } else if (ksym == KS_Alt_R || ksym == KS_Alt_L) {
      _display.key_state &= ~Mod1Mask;
    } else if (ksym == KS_Mode_switch) {
      wskbd_mode_switch = 0;
    } else if (ev->value == prev_key_event.value) {
      prev_key_event.value = 0;
    }
  }
}

#ifdef __NetBSD__
static void auto_repeat(void) {
  if (prev_key_event.value && --kbd_repeat_wait == 0) {
    process_wskbd_event(&prev_key_event);
    kbd_repeat_wait = (kbd_repeat_N + KEY_REPEAT_UNIT / 2) / KEY_REPEAT_UNIT;
  }
}
#endif

static fb_cmap_t *cmap_new(int num_colors);

static int open_display(u_int depth /* used on luna68k alone. */
                        ) {
  char *dev;
  struct wsdisplay_fbinfo vinfo;
#ifdef WSDISPLAYIO_GET_FBINFO
  struct wsdisplayio_fbinfo vinfo2;
#endif
  int fboffset;
  int mode;
  int wstype;
  struct rgb_info rgbinfos[] = {
      {3, 3, 3, 10, 5, 0}, {3, 2, 3, 11, 5, 0}, {0, 0, 0, 16, 8, 0},
  };
  struct termios tm;
  static struct wscons_keymap map[KS_NUMKEYCODES];

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open((dev = getenv("FRAMEBUFFER")) ? dev : DEFAULT_FBDEV, O_RDWR);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev ? dev : DEFAULT_FBDEV);

    return 0;
  }

  bl_file_set_cloexec(_display.fb_fd);

  ioctl(STDIN_FILENO, WSDISPLAYIO_GMODE, &orig_console_mode);

#ifdef __OpenBSD__
  {
    struct wsdisplay_gfx_mode gfx_mode;

    gfx_mode.width = fb_width;
    gfx_mode.height = fb_height;
    gfx_mode.depth = fb_depth;

    if (ioctl(_display.fb_fd, WSDISPLAYIO_SETGFXMODE, &gfx_mode) == -1) {
      bl_error_printf("Couldn't set screen resolution (gfx mode).\n");
    }
  }
#endif

  mode = WSDISPLAYIO_MODE_DUMBFB;

  if (ioctl(STDIN_FILENO, WSDISPLAYIO_SMODE, &mode) == -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " WSDISPLAYIO_SMODE failed.\n");
#endif

    goto error;
  }

  if (ioctl(_display.fb_fd, WSDISPLAYIO_GTYPE, &wstype) == -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " WSDISPLAYIO_GTYPE failed.\n");
#endif

    goto error;
  }

#ifdef WSDISPLAYIO_GET_FBINFO
  vinfo2.fbi_stride = 0;
  if (ioctl(_display.fb_fd, WSDISPLAYIO_GET_FBINFO, &vinfo2) == 0) {
    vinfo.width = vinfo2.fbi_width;
    vinfo.height = vinfo2.fbi_height;
    if (vinfo2.fbi_bitsperpixel == 32 &&
        vinfo2.fbi_subtype.fbi_rgbmasks.alpha_size == 0) {
      vinfo.depth = 24;
    } else {
      vinfo.depth = vinfo2.fbi_bitsperpixel;
    }
    vinfo.cmsize = vinfo2.fbi_subtype.fbi_cmapinfo.cmap_entries;

    fboffset = vinfo2.fbi_fboffset;
  } else
#endif
  if (ioctl(_display.fb_fd, WSDISPLAYIO_GINFO, &vinfo) == 0) {
#ifdef WSDISPLAY_TYPE_LUNA
    if (wstype == WSDISPLAY_TYPE_LUNA) {
      fboffset = 8;
    } else
#endif
    {
      fboffset = 0;
    }
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " WSDISPLAYIO_GTYPE and WSDISPLAYIO_GET_FBINFO failed.\n");
#endif

    goto error;
  }

  _display.xoffset = 0;
  _display.yoffset = 0;

  _display.width = _disp.width = vinfo.width;
  _display.height = _disp.height = vinfo.height;
  _disp.depth = vinfo.depth;

#ifdef WSDISPLAY_TYPE_LUNA
  if (wstype == WSDISPLAY_TYPE_LUNA) {
    /* always 8 or less bpp */

    if (_disp.depth > 8) {
      goto error;
    } else if (depth == 1 || depth == 4 || depth == 8) {
      _disp.depth = depth;
    }

    _display.pixels_per_byte = 8;
    _display.shift_0 = 7;
    _display.mask = 1;
  } else
#endif
      if (_disp.depth < 8) {
#ifdef ENABLE_2_4_PPB
    _display.pixels_per_byte = 8 / _disp.depth;
#else
    /* XXX Forcibly set 1 bpp */
    _disp.depth = 1;
    _display.pixels_per_byte = 8;
#endif

    _display.shift_0 = FB_SHIFT_0(_display.pixels_per_byte, _disp.depth);
    _display.mask = FB_MASK(_display.pixels_per_byte);
  } else {
    _display.pixels_per_byte = 1;
  }

  if ((_display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3) {
    _display.bytes_per_pixel = 4;
  }

#ifdef WSDISPLAYIO_GET_FBINFO
  if (vinfo2.fbi_stride > 0) {
    _display.line_length = vinfo2.fbi_stride;
  } else
#endif
      if (ioctl(_display.fb_fd, WSDISPLAYIO_LINEBYTES, &_display.line_length) == -1) {
/* WSDISPLAYIO_LINEBYTES isn't defined in some ports. */

#ifdef MACHINE
    /* XXX Hack for NetBSD 5.ui.hpcmips */
    if (strcmp(MACHINE, "hpcmips") == 0 && _disp.depth == 16) {
      _display.line_length = _display.width * 5 / 2;
    } else
#endif
    {
      _display.line_length = _display.width * _display.bytes_per_pixel / _display.pixels_per_byte;
    }
  }

#ifdef WSDISPLAY_TYPE_LUNA
  if (wstype == WSDISPLAY_TYPE_LUNA && (_disp.depth == 4 || _disp.depth == 8)) {
    u_int plane;

    _display.smem_len = (fboffset + 0x40000) * _disp.depth;

    for (plane = 0; plane < _disp.depth; plane++) {
      _display.plane_offset[plane] = 0x40000 * plane;
    }
  } else
#endif
  {
    _display.smem_len = fboffset + _display.line_length * _display.height;
  }

  if ((_display.fb = mmap(NULL, _display.smem_len, PROT_WRITE | PROT_READ, MAP_SHARED,
                          _display.fb_fd, (off_t)0)) == MAP_FAILED) {
    bl_error_printf("Retry another mode of resolution and depth.\n");

    goto error;
  }

  _display.fb_base = _display.fb + fboffset;

  if (_disp.depth < 15) {
    if (vinfo.depth >= 2 && _disp.depth == 1) {
      int num_colors;
      vt_color_t color;

      num_colors = (2 << (vinfo.depth - 1));

      if (!_display.cmap) {
        if (!(_display.cmap_orig = cmap_new(num_colors))) {
          goto error;
        }

        ioctl(_display.fb_fd, FBIOGETCMAP, _display.cmap_orig);

        if (!(_display.cmap = cmap_new(num_colors))) {
          free(_display.cmap_orig);

          goto error;
        }

        if (!(_display.color_cache = calloc(1, sizeof(*_display.color_cache)))) {
          free(_display.cmap_orig);
          free(_display.cmap);

          goto error;
        }
      }

      for (color = 0; color < num_colors; color++) {
        _display.cmap->red[color] = (color & 1) ? 0xff : 0;
        _display.cmap->green[color] = (color & 1) ? 0xff : 0;
        _display.cmap->blue[color] = (color & 1) ? 0xff : 0;
      }

      ioctl(_display.fb_fd, FBIOPUTCMAP, _display.cmap);
    } else if (!cmap_init()) {
      goto error;
    }
  }
#ifdef WSDISPLAYIO_GET_FBINFO
  else if (vinfo2.fbi_stride > 0) {
    _display.rgbinfo.r_limit = 8 - vinfo2.fbi_subtype.fbi_rgbmasks.red_size;
    _display.rgbinfo.g_limit = 8 - vinfo2.fbi_subtype.fbi_rgbmasks.green_size;
    _display.rgbinfo.b_limit = 8 - vinfo2.fbi_subtype.fbi_rgbmasks.blue_size;
    _display.rgbinfo.r_offset = vinfo2.fbi_subtype.fbi_rgbmasks.red_offset;
    _display.rgbinfo.g_offset = vinfo2.fbi_subtype.fbi_rgbmasks.green_offset;
    _display.rgbinfo.b_offset = vinfo2.fbi_subtype.fbi_rgbmasks.blue_offset;

#ifdef DEBUG
    bl_debug_printf("FBINFO: (limit)r%d g%d b%d (offset)r%d g%d b%d\n", _display.rgbinfo.r_limit,
                    _display.rgbinfo.g_limit, _display.rgbinfo.b_limit, _display.rgbinfo.r_offset,
                    _display.rgbinfo.g_offset, _display.rgbinfo.b_offset);
#endif
  }
#endif
  else {
    if (_disp.depth == 15) {
      _display.rgbinfo = rgbinfos[0];
    } else if (_disp.depth == 16) {
      _display.rgbinfo = rgbinfos[1];
    } else /* if( _disp.depth >= 24) */
    {
      _display.rgbinfo = rgbinfos[2];
    }

    if (wstype == WSDISPLAY_TYPE_SUN24 || wstype == WSDISPLAY_TYPE_SUNCG12 ||
        wstype == WSDISPLAY_TYPE_SUNCG14 || wstype == WSDISPLAY_TYPE_SUNTCX ||
        wstype == WSDISPLAY_TYPE_SUNFFB
#ifdef WSDISPLAY_TYPE_XVR1000
        || wstype == WSDISPLAY_TYPE_XVR1000
#endif
#ifdef WSDISPLAY_TYPE_VC4
        || wstype == WSDISPLAY_TYPE_VC4
#endif
        ) {
      /* RRGGBB => BBGGRR */
      u_int tmp;

      tmp = _display.rgbinfo.r_offset;
      _display.rgbinfo.r_offset = _display.rgbinfo.b_offset;
      _display.rgbinfo.b_offset = tmp;
    }
  }

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

  if (!getenv("WSKBD") ||
      /* If you want to use /dev/wskbd0, export WSKBD=/dev/wskbd0 */
      (_display.fd = open(getenv("WSKBD"), O_RDWR | O_NONBLOCK | O_EXCL)) == -1) {
    _display.fd = open("/dev/wskbd", O_RDWR | O_NONBLOCK | O_EXCL);
  }

  _mouse.fd = open("/dev/wsmouse", O_RDWR | O_NONBLOCK | O_EXCL);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fd == -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed to open /dev/wskbd.\n");
#endif

    _display.fd = STDIN_FILENO;
  } else {
    bl_file_set_cloexec(_display.fd);

#ifdef WSKBDIO_EVENT_VERSION
    mode = WSKBDIO_EVENT_VERSION;
    ioctl(_display.fd, WSKBDIO_SETVERSION, &mode);
#endif

    ioctl(_display.fd, WSKBDIO_GTYPE, &kbd_type);

    keymap.maplen = KS_NUMKEYCODES;
    keymap.map = map;
    ioctl(_display.fd, WSKBDIO_GETMAP, &keymap);

#if 0
    bl_debug_printf("DUMP KEYMAP (LEN %d)\n", keymap.maplen);
    {
      int count;

      for (count = 0; count < keymap.maplen; count++) {
        bl_debug_printf("%d: %x %x %x %x %x\n", count, keymap.map[count].command,
                        keymap.map[count].group1[0], keymap.map[count].group1[1],
                        keymap.map[count].group2[0], keymap.map[count].group2[1]);
      }
    }
#endif

    tcgetattr(_display.fd, &tm);
    tm.c_iflag = IGNBRK | IGNPAR;
    tm.c_oflag = 0;
    tm.c_lflag = 0;
    tm.c_cc[VTIME] = 0;
    tm.c_cc[VMIN] = 1;
    tm.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
    cfsetispeed(&tm, B1200);
    cfsetospeed(&tm, B1200);
    tcsetattr(_display.fd, TCSAFLUSH, &tm);

    ioctl(_display.fd, WSKBDIO_GETLEDS, &_display.lock_state);

#ifdef __NetBSD__
    ui_event_source_add_fd(-10, auto_repeat);
#endif
  }

  _disp.display = &_display;

  if (_mouse.fd != -1) {
    bl_file_set_cloexec(_mouse.fd);

#ifdef WSMOUSE_EVENT_VERSION
    mode = WSMOUSE_EVENT_VERSION;
    ioctl(_mouse.fd, WSMOUSEIO_SETVERSION, &mode);
#endif

    _mouse.x = _display.width / 2;
    _mouse.y = _display.height / 2;
    _disp_mouse.display = (Display*)&_mouse;

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
  }
#ifdef DEBUG
  else {
    bl_debug_printf(BL_DEBUG_TAG " Failed to open /dev/wsmouse.\n");
  }
#endif

  console_id = get_active_console();

  return 1;

error:
  if (_display.fb) {
    munmap(_display.fb, _display.smem_len);
    _display.fb = _display.fb_base = NULL;
  }

  close(_display.fb_fd);

  ioctl(STDIN_FILENO, WSDISPLAYIO_SMODE, &orig_console_mode);

  return 0;
}

static int receive_mouse_event(void) {
  struct wscons_event ev;
  ssize_t len;

  if (console_id != get_active_console()) {
    return 0;
  }

  while ((len = read(_mouse.fd, memset(&ev, 0, sizeof(ev)), sizeof(ev))) > 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " MOUSE event (len)%d (type)%d (val)%d\n", len, ev.type, ev.value);
#endif

    if (ev.type == WSCONS_EVENT_MOUSE_ABSOLUTE_X) {
      _mouse.x = ev.value;

      continue; /* Wait for ABSOLUTE_Y */
    } else if (ev.type == WSCONS_EVENT_MOUSE_ABSOLUTE_Y) {
      restore_hidden_region();

      _mouse.y = ev.value;
      update_mouse_cursor_state();

      /* XXX MotionNotify event is not sent. */

      save_hidden_region();
      draw_mouse_cursor();

      ev.type = WSCONS_EVENT_MOUSE_DOWN;
      ev.value = 0; /* Button1 */
    }

    if (ev.type == WSCONS_EVENT_MOUSE_DOWN || ev.type == WSCONS_EVENT_MOUSE_UP) {
      XButtonEvent xev;
      ui_window_t *win;

      if (ev.value == 0) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if (ev.value == 1) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if (ev.value == 2) {
        xev.button = Button3;
        _mouse.button_state = Button3Mask;
      } else {
        continue;

        while (1) {
        button4:
          xev.button = Button4;
          _mouse.button_state = Button4Mask;
          break;

        button5:
          xev.button = Button5;
          _mouse.button_state = Button5Mask;
          break;

        button6:
          xev.button = Button6;
          _mouse.button_state = Button6Mask;
          break;

        button7:
          xev.button = Button7;
          _mouse.button_state = Button7Mask;
          break;
        }

        ev.value = 1;
      }

      if (ev.type != WSCONS_EVENT_MOUSE_UP) {
        /*
         * WSCONS_EVENT_MOUSE_UP,
         * WSCONS_EVENT_MOUSE_DELTA_Z
         * WSCONS_EVENT_MOUSE_DELTA_W
         */
        xev.type = ButtonPress;
      } else /* if( ev.type == WSCONS_EVENT_MOUSE_UP) */
      {
        xev.type = ButtonRelease;

        /* Reset button_state in releasing button. */
        _mouse.button_state = 0;
      }

      xev.time = ev.time.tv_sec * 1000 + ev.time.tv_nsec / 1000000;
      if (rotate_display) {
        if (rotate_display > 0) {
          xev.x = _mouse.y;
          xev.y = _display.width - _mouse.x - 1;
        } else {
          xev.x = _display.height - _mouse.y - 1;
          xev.y = _mouse.x;
        }
      } else {
        xev.x = _mouse.x;
        xev.y = _mouse.y;
      }
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
    } else if (ev.type == WSCONS_EVENT_MOUSE_DELTA_X || ev.type == WSCONS_EVENT_MOUSE_DELTA_Y ||
               ev.type == WSCONS_EVENT_MOUSE_DELTA_Z || ev.type == WSCONS_EVENT_MOUSE_DELTA_W) {
      XMotionEvent xev;
      ui_window_t *win;

      if (ev.type == WSCONS_EVENT_MOUSE_DELTA_X) {
        restore_hidden_region();

        _mouse.x += (int)ev.value;

        if (_mouse.x < 0) {
          _mouse.x = 0;
        } else if (_display.width <= _mouse.x) {
          _mouse.x = _display.width - 1;
        }
      } else if (ev.type == WSCONS_EVENT_MOUSE_DELTA_Y) {
        restore_hidden_region();

        _mouse.y -= (int)ev.value;

        if (_mouse.y < 0) {
          _mouse.y = 0;
        } else if (_display.height <= _mouse.y) {
          _mouse.y = _display.height - 1;
        }
      } else if (ev.type == WSCONS_EVENT_MOUSE_DELTA_Z) {
        if (ev.value < 0) {
          /* Up */
          goto button4;
        } else if (ev.value > 0) {
          /* Down */
          goto button5;
        }
      } else /* if( ev.type == WSCONS_EVENT_MOUSE_DELTA_W) */
      {
        if (ev.value < 0) {
          /* Left */
          goto button6;
        } else if (ev.value > 0) {
          /* Right */
          goto button7;
        }
      }

      update_mouse_cursor_state();

      xev.type = MotionNotify;
      if (rotate_display) {
        if (rotate_display > 0) {
          xev.x = _mouse.y;
          xev.y = _display.width - _mouse.x - 1;
        } else {
          xev.x = _display.height - _mouse.y - 1;
          xev.y = _mouse.x;
        }
      } else {
        xev.x = _mouse.x;
        xev.y = _mouse.y;
      }
      xev.time = ev.time.tv_sec * 1000 + ev.time.tv_nsec / 1000000;
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
  if (_display.fd == STDIN_FILENO) {
    return receive_stdin_key_event();
  } else {
    ssize_t len;
    struct wscons_event ev;

    if (console_id != get_active_console()) {
      return 0;
    }

    while ((len = read(_display.fd, memset(&ev, 0, sizeof(ev)), sizeof(ev))) > 0) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " KEY event (len)%d (type)%d (val)%d\n", len, ev.type, ev.value);
#endif

      process_wskbd_event(&ev);
    }
  }

  return 1;
}
