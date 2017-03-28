/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/vt.h> /* VT_GETSTATE */

#define _GNU_SOURCE /* strcasestr */
#include <string.h>

#if 0
#define READ_CTRL_KEYMAP
#endif

/* --- static variables --- */

static int console_id = -1;

/* --- static functions --- */

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

static int kcode_to_ksym(int kcode, int state) {
  if (kcode == KEY_ENTER || kcode == KEY_KPENTER) {
    /* KDGKBENT returns '\n'(0x0a) */
    return 0x0d;
  } else if (kcode == KEY_BACKSPACE) {
    /* KDGKBDENT returns 0x7f */
    return 0x08;
  } else if (kcode <= KEY_SLASH || kcode == KEY_SPACE || kcode == KEY_YEN || kcode == KEY_RO) {
    struct kbentry ent;

    if (state & ShiftMask) {
      ent.kb_table = (1 << KG_SHIFT);
    }
#ifdef READ_CTRL_KEYMAP
    else if (state & ControlMask) {
      ent.kb_table = (1 << KG_CTRL);
    }
#endif
    else {
      ent.kb_table = 0;
    }

    ent.kb_index = kcode;

    if (ioctl(STDIN_FILENO, KDGKBENT, &ent) == 0 && ent.kb_value != K_HOLE &&
        ent.kb_value != K_NOSUCHMAP) {
      ent.kb_value &= 0xff;

#if 1
      /* XXX linux returns KEY_GRAVE for HankakuZenkaku key. */
      if (kcode == KEY_GRAVE && ent.kb_value == '\x1b') {
        static int is_jp106 = -1;

        if (is_jp106 == -1) {
          struct kbentry ent;

          is_jp106 = 0;

          ent.kb_table = (1 << KG_SHIFT);
          ent.kb_index = KEY_MINUS;

          if (ioctl(STDIN_FILENO, KDGKBENT, &ent) == 0 && ent.kb_value != K_HOLE &&
              ent.kb_value != K_NOSUCHMAP && ent.kb_value == '=') {
            /* is jp106 or netherland */
            is_jp106 = 1;
          }
        }

        if (is_jp106) {
          return KEY_ZENKAKUHANKAKU + 0x100;
        }
      }
#endif

      return ent.kb_value;
    }
  }

  return kcode + 0x100;
}

static void set_use_console_backscroll(int use) {
  struct kbentry ent;

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  ent.kb_table = (1 << KG_SHIFT);
  ent.kb_index = KEY_PAGEUP;
  ent.kb_value = use ? K_SCROLLBACK : K_PGUP;
  ioctl(STDIN_FILENO, KDSKBENT, &ent);

  ent.kb_index = KEY_PAGEDOWN;
  ent.kb_value = use ? K_SCROLLFORW : K_PGDN;
  ioctl(STDIN_FILENO, KDSKBENT, &ent);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());
}

static void get_event_device_num_intern(int *kbd, int *mouse, const char *fmt) {
  char *class;
  int count;
  FILE* fp;

  *kbd = *mouse = -1;

  if (!(class = alloca(strlen(fmt) - 2 /* %d */ + 3 /* 0 - 999 */ + 1))) {
    return;
  }

  for (count = 0;; count++) {
    sprintf(class, fmt, count);

    if (!(fp = fopen(class, "r"))) {
      break;
    } else {
      char buf[128];

      if (fgets(buf, sizeof(buf), fp)) {
        if (strcasestr(buf, "key")) {
          *kbd = count;
        } else {
          static char *mouse_names[] = {"mouse", "touch"};
          u_int idx;

          for (idx = 0; idx < sizeof(mouse_names) / sizeof(mouse_names[0]); idx++) {
            if (strcasestr(buf, mouse_names[idx])) {
              *mouse = count;

              break;
            }
          }
        }
      }

      fclose(fp);

      if (*kbd != -1 && *mouse != -1) {
        break;
      }
    }
  }
}

static void get_event_device_num(int *kbd, int *mouse) {
  get_event_device_num_intern(kbd, mouse, "/sys/class/input/input%d/name");

  if (*kbd == -1 || *mouse == -1) {
    int k;
    int m;

    get_event_device_num_intern(&k, &m, "/sys/class/input/event%d/device/name");

    if (*kbd == -1) {
      *kbd = k;
    }

    if (*mouse == -1) {
      *mouse = m;
    }
  }
}

static int open_event_device(u_int num, const char *path) {
  char event[16 + 3 /* 0 - 999 */ + 1];
  int fd;

  if (!path) {
    if (num >= 1000) { /* num < 0 is impossible because num is u_int. */
      return -1;
    }

    sprintf(event, "/dev/input/event%d", num);
    path = event;
  }

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  if ((fd = open(path, O_RDONLY | O_NONBLOCK)) == -1) {
    bl_error_printf("Couldn't open %s.\n", path);
  }
#if 0
  else {
    /* Occupy /dev/input/eventN */
    ioctl(fd, EVIOCGRAB, 1);
  }
#endif

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  return fd;
}

static void convert_input_num(int *num, const char *str) {
  if (strcmp(str, "-1") == 0) {
    *num = -1;
  } else if ('0' <= *str && *str <= '9') {
    *num = atoi(str);
  }
}

static int open_display(u_int depth) {
  char *dev;
  struct fb_fix_screeninfo finfo;
  struct fb_var_screeninfo vinfo;
  int kbd_num;
  int mouse_num;
  struct termios tm;

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open((dev = getenv("FRAMEBUFFER")) ? dev : "/dev/fb0", O_RDWR);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev ? dev : "/dev/fb0");

    return 0;
  }

  bl_file_set_cloexec(_display.fb_fd);

  ioctl(_display.fb_fd, FBIOGET_FSCREENINFO, &finfo);
  ioctl(_display.fb_fd, FBIOGET_VSCREENINFO, &vinfo);

  if ((_disp.depth = vinfo.bits_per_pixel) < 8) {
#ifdef ENABLE_2_4_PPB
    _display.pixels_per_byte = 8 / _disp.depth;
#else
    /* XXX Forcibly set 1 bpp */
    _display.pixels_per_byte = 8;
    _disp.depth = 1;
#endif

    _display.shift_0 = FB_SHIFT_0(_display.pixels_per_byte, _disp.depth);
    _display.mask = FB_MASK(_display.pixels_per_byte);
  } else {
    _display.pixels_per_byte = 1;
  }

  if ((_display.fb = mmap(NULL, (_display.smem_len = finfo.smem_len), PROT_WRITE | PROT_READ,
                          MAP_SHARED, _display.fb_fd, (off_t)0)) == MAP_FAILED) {
    goto error;
  }

  if ((_display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3) {
    _display.bytes_per_pixel = 4;
  }

  if (_disp.depth < 15 && !cmap_init()) {
    goto error;
  }

#ifdef ENABLE_DOUBLE_BUFFER
  if (_display.pixels_per_byte > 1 && !(_display.back_fb = malloc(_display.smem_len))) {
    cmap_final();

    goto error;
  }
#endif

  _display.line_length = finfo.line_length;
  _display.xoffset = vinfo.xoffset;
  _display.yoffset = vinfo.yoffset;

  _display.width = _disp.width = vinfo.xres;
  _display.height = _disp.height = vinfo.yres;

  _display.rgbinfo.r_limit = 8 - vinfo.red.length;
  _display.rgbinfo.g_limit = 8 - vinfo.green.length;
  _display.rgbinfo.b_limit = 8 - vinfo.blue.length;
  _display.rgbinfo.r_offset = vinfo.red.offset;
  _display.rgbinfo.g_offset = vinfo.green.offset;
  _display.rgbinfo.b_offset = vinfo.blue.offset;

  get_event_device_num(&kbd_num, &mouse_num);

  if ((dev = getenv("KBD_INPUT_NUM"))) {
    convert_input_num(&kbd_num, dev);
  }

  if ((dev = getenv("MOUSE_INPUT_NUM"))) {
    convert_input_num(&mouse_num, dev);
  }

  tcgetattr(STDIN_FILENO, &tm);
  orig_tm = tm;
  tm.c_iflag = tm.c_oflag = 0;
  tm.c_cflag &= ~CSIZE;
  tm.c_cflag |= CS8;
  tm.c_lflag &= ~(ECHO | ISIG | ICANON);
  tm.c_cc[VMIN] = 1;
  tm.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tm);

  /* Disable backscrolling of default console. */
  set_use_console_backscroll(0);

  if (kbd_num == -1 || (_display.fd = open_event_device(kbd_num, NULL)) == -1) {
    _display.fd = STDIN_FILENO;
  } else {
    bl_file_set_cloexec(_display.fd);
  }

  _disp.display = &_display;

  if ((mouse_num == -1 || (_mouse.fd = open_event_device(mouse_num, NULL)) == -1) &&
      (_mouse.fd = open_event_device(0, "/dev/input/mice")) == -1 &&
      (_mouse.fd = open_event_device(0, "/dev/input/mouse0")) == -1) {
    _mouse.fd = -1;
  } else {
    bl_file_set_cloexec(_mouse.fd);
    _mouse.x = _display.width / 2;
    _mouse.y = _display.height / 2;
    _disp_mouse.display = (Display*)&_mouse;
  }

  console_id = get_active_console();

  return 1;

error:
  if (_display.fb) {
    munmap(_display.fb, _display.smem_len);
    _display.fb = NULL;
  }

  close(_display.fb_fd);

  return 0;
}

static int receive_mouse_event(void) {
  struct input_event ev;

  if (console_id != get_active_console()) {
    return 0;
  }

  while (read(_mouse.fd, &ev, sizeof(ev)) > 0) {
    if (ev.type == EV_ABS) {
#ifdef EVIOCGABS
      static int max_abs_x;
      static int max_abs_y;

      if (max_abs_x == 0 /* || max_abs_y == 0 */) {
        struct input_absinfo info;

        ioctl(_mouse.fd, EVIOCGABS(ABS_X), &info);
        max_abs_x = info.maximum;

        ioctl(_mouse.fd, EVIOCGABS(ABS_Y), &info);
        max_abs_y = info.maximum;
      }

      if (ev.code == ABS_PRESSURE) {
        ev.type = EV_KEY;
        ev.code = BTN_LEFT;
        ev.value = 1; /* ButtonPress */
      } else if (ev.code == ABS_X) {
        ev.type = EV_REL;
        ev.code = REL_X;
        ev.value = ev.value * _display.width / max_abs_x - _mouse.x;
      } else if (ev.code == ABS_Y) {
        ev.type = EV_REL;
        ev.code = REL_Y;
        ev.value = ev.value * _display.height / max_abs_y - _mouse.y;
      } else
#endif
      {
        continue;
      }
    }

    if (ev.type == EV_KEY) {
      XButtonEvent xev;
      ui_window_t *win;

      if (ev.code == BTN_LEFT) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if (ev.code == BTN_MIDDLE) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if (ev.code == BTN_RIGHT) {
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

      if (ev.value == 1) {
        xev.type = ButtonPress;
      } else if (ev.value == 0) {
        xev.type = ButtonRelease;

        /* Reset button_state in releasing button. */
        _mouse.button_state = 0;
      } else {
        continue;
      }

      xev.time = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
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
    } else if (ev.type == EV_REL) {
      XMotionEvent xev;
      ui_window_t *win;

      if (ev.code == REL_X) {
        restore_hidden_region();

        _mouse.x += (int)ev.value;

        if (_mouse.x < 0) {
          _mouse.x = 0;
        } else if (_display.width <= _mouse.x) {
          _mouse.x = _display.width - 1;
        }
      } else if (ev.code == REL_Y) {
        restore_hidden_region();

        _mouse.y += (int)ev.value;

        if (_mouse.y < 0) {
          _mouse.y = 0;
        } else if (_display.height <= _mouse.y) {
          _mouse.y = _display.height - 1;
        }
      } else if (ev.code == REL_WHEEL) {
        if (ev.value > 0) {
          /* Up */
          goto button4;
        } else if (ev.value < 0) {
          /* Down */
          goto button5;
        }
      } else if (ev.code == REL_HWHEEL) {
        if (ev.value < 0) {
          /* Left */
          goto button6;
        } else if (ev.value > 0) {
          /* Right */
          goto button7;
        }
      } else {
        continue;
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
  if (_display.fd == STDIN_FILENO) {
    return receive_stdin_key_event();
  } else {
    struct input_event ev;

    if (console_id != get_active_console()) {
      return 0;
    }

    while (read(_display.fd, &ev, sizeof(ev)) > 0) {
      if (ev.type == EV_KEY && ev.code < 0x100 /* Key event is less than 0x100 */) {
        if (ev.value == 1 /* Pressed */ || ev.value == 2 /* auto repeat */) {
          if (ev.code == KEY_RIGHTSHIFT || ev.code == KEY_LEFTSHIFT) {
            _display.key_state |= ShiftMask;
          } else if (ev.code == KEY_CAPSLOCK) {
            if (_display.key_state & ShiftMask) {
              _display.key_state &= ~ShiftMask;
            } else {
              _display.key_state |= ShiftMask;
            }
          } else if (ev.code == KEY_RIGHTCTRL || ev.code == KEY_LEFTCTRL) {
            _display.key_state |= ControlMask;
          } else if (ev.code == KEY_RIGHTALT || ev.code == KEY_LEFTALT) {
            _display.key_state |= ModMask;
          } else if (ev.code == KEY_NUMLOCK) {
            _display.lock_state ^= NLKED;
          } else {
            XKeyEvent xev;

            xev.type = KeyPress;
            xev.ksym = kcode_to_ksym(ev.code, _display.key_state);
            xev.keycode = ev.code;
            xev.state = _mouse.button_state | _display.key_state;

            receive_event_for_multi_roots(&xev);
          }
        } else if (ev.value == 0 /* Released */) {
          if (ev.code == KEY_RIGHTSHIFT || ev.code == KEY_LEFTSHIFT) {
            _display.key_state &= ~ShiftMask;
          } else if (ev.code == KEY_RIGHTCTRL || ev.code == KEY_LEFTCTRL) {
            _display.key_state &= ~ControlMask;
          } else if (ev.code == KEY_RIGHTALT || ev.code == KEY_LEFTALT) {
            _display.key_state &= ~ModMask;
          }
        }
      }
    }
  }

  return 1;
}
