/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <sys/consio.h>
#include <sys/time.h>

#if __FreeBSD__ >= 5
#include <sys/mouse.h>
#define MOUSE_DEV "/dev/sysmouse"
#define PACKET_SIZE 8
#elif defined(PC98)
#define MOUSE_DEV "/dev/mse0"
#define PACKET_SIZE 5
#else
#define MOUSE_DEV "/dev/psm0"
#define PACKET_SIZE 3
#endif

/* --- static variables --- */

static keymap_t keymap;
static accentmap_t *accentmap;

/* --- static functions --- */

static int load_accentmap(void) {
  if (accentmap) {
    return 1;
  } else if ((accentmap = malloc(sizeof(*accentmap)))) { /* XXX Leadked */
    ioctl(STDIN_FILENO, GIO_DEADKEYMAP, accentmap);

    return 1;
  } else {
    return 0;
  }
}

static int open_display(u_int depth) {
  char *dev;
  int vmode;
  video_info_t vinfo;
  video_adapter_info_t vainfo;
  video_display_start_t vstart;
  struct termios tm;

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open((dev = getenv("FRAMEBUFFER")) ? dev : "/dev/ttyv0", O_RDWR);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev ? dev : "/dev/ttyv0");

    return 0;
  }

  bl_file_set_cloexec(_display.fb_fd);

#ifdef PC98
  /* XXX 256 colors is not supported for now. */
#if 0
  if (depth == 8) {
    ioctl(_display.fb_fd, SW_PC98_PEGC640x400, NULL);
  } else
#endif
  {
    ioctl(_display.fb_fd, SW_PC98_EGC640x400, NULL);
  }
#endif

  ioctl(_display.fb_fd, FBIO_GETMODE, &vmode);

  vinfo.vi_mode = vmode;
  ioctl(_display.fb_fd, FBIO_MODEINFO, &vinfo);
  ioctl(_display.fb_fd, FBIO_ADPINFO, &vainfo);
  ioctl(_display.fb_fd, FBIO_GETDISPSTART, &vstart);

  if ((_display.fb = _display.fb_base = mmap(NULL, (_display.smem_len = vainfo.va_window_size), PROT_WRITE | PROT_READ,
                          MAP_SHARED, _display.fb_fd, (off_t)0)) == MAP_FAILED) {
    bl_error_printf("Retry another mode of resolution and depth.\n");

    goto error;
  }

  _disp.depth = vinfo.vi_depth;

  if ((_display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3) {
    _display.bytes_per_pixel = 4;
  }

  if (_disp.depth < 15) {
#ifdef M_PC98_EGC640x400
    if (vainfo.va_mode == M_PC98_EGC640x400) {
      _display.pixels_per_byte = 8;
      _disp.depth = 4;
      _display.shift_0 = 7;
      _display.mask = 1;
      _display.plane_offset[0] = 0;       /* 0xA8000 */
      _display.plane_offset[1] = 0x8000;  /* 0xB0000 */
      _display.plane_offset[2] = 0x10000; /* 0xB8000 */
      _display.plane_offset[3] = 0x38000; /* 0xE0000 */
    } else
#endif
    if (_disp.depth < 8) {
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

    if (!cmap_init()) {
      goto error;
    }
  } else {
    _display.pixels_per_byte = 1;
  }

#ifdef ENABLE_DOUBLE_BUFFER
  if (_display.pixels_per_byte > 1 && !(_display.back_fb = malloc(_display.smem_len))) {
    cmap_final();

    goto error;
  }
#endif

  _display.line_length = vainfo.va_line_width;
  _display.xoffset = vstart.x;
  _display.yoffset = vstart.y;

  _display.width = _disp.width = vinfo.vi_width;
  _display.height = _disp.height = vinfo.vi_height;

  _display.rgbinfo.r_limit = 8 - vinfo.vi_pixel_fsizes[0];
  _display.rgbinfo.g_limit = 8 - vinfo.vi_pixel_fsizes[1];
  _display.rgbinfo.b_limit = 8 - vinfo.vi_pixel_fsizes[2];
  _display.rgbinfo.a_limit = 8 - vinfo.vi_pixel_fsizes[3];
  _display.rgbinfo.r_offset = vinfo.vi_pixel_fields[0];
  _display.rgbinfo.g_offset = vinfo.vi_pixel_fields[1];
  _display.rgbinfo.b_offset = vinfo.vi_pixel_fields[2];
  _display.rgbinfo.a_offset = vinfo.vi_pixel_fields[3];

  tcgetattr(STDIN_FILENO, &tm);
  orig_tm = tm;
  tm.c_iflag = tm.c_oflag = 0;
  tm.c_cflag &= ~CSIZE;
  tm.c_cflag |= CS8;
  tm.c_lflag &= ~(ECHO | ISIG | IEXTEN | ICANON);
  tm.c_cc[VMIN] = 1;
  tm.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tm);

  ioctl(STDIN_FILENO, GIO_KEYMAP, &keymap);
  ioctl(STDIN_FILENO, KDSKBMODE, K_CODE);
  ioctl(STDIN_FILENO, KDGKBSTATE, &_display.lock_state);

  _display.fd = STDIN_FILENO;

  _disp.display = &_display;

  bl_priv_restore_euid();
  bl_priv_restore_egid();
  _mouse.fd = open(MOUSE_DEV, O_RDWR | O_NONBLOCK);
  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_mouse.fd != -1) {
    struct mouse_info info;
#ifdef MOUSE_SETLEVEL
    int level;
    mousemode_t mode;

    level = 1;
    ioctl(_mouse.fd, MOUSE_SETLEVEL, &level);
    ioctl(_mouse.fd, MOUSE_GETMODE, &mode);

    if (mode.packetsize != PACKET_SIZE) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Failed to open " MOUSE_DEV ".\n");
#endif
      close(_mouse.fd);
      _mouse.fd = -1;
    } else
#endif
    {
      bl_file_set_cloexec(_mouse.fd);

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

      info.operation = MOUSE_HIDE;
      ioctl(STDIN_FILENO, CONS_MOUSECTL, &info);
    }
  }
#ifdef DEBUG
  else {
    bl_debug_printf(BL_DEBUG_TAG " Failed to open " MOUSE_DEV ".\n");
  }
#endif

  return 1;

error:
  if (_display.fb) {
    munmap(_display.fb, _display.smem_len);
    _display.fb = _display.fb_base = NULL;
  }

  close(_display.fb_fd);

  return 0;
}

static int receive_mouse_event(int fd) {
  u_char buf[PACKET_SIZE * 8];
  ssize_t len;

  while ((len = read(fd, buf, sizeof(buf))) > 0) {
    static u_char packet[PACKET_SIZE];
    static ssize_t packet_len;
    ssize_t count;

    for (count = 0; count < len; count++) {
      int x;
      int y;
      int move;
      struct timeval tv;
      XButtonEvent xev;
      ui_window_t *win;

#if __FreeBSD__ >= 5
      int z;

      if (packet_len == 0) {
        if ((buf[count] & 0xf8) != 0x80) {
          /* is not packet header */
          continue;
        }
      }
#endif

      packet[packet_len++] = buf[count];

      if (packet_len < PACKET_SIZE) {
        continue;
      }

      packet_len = 0;

      /* set mili seconds */
      gettimeofday(&tv, NULL);
      xev.time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

#if __FreeBSD__ >= 5
      x = (char)packet[1] + (char)packet[3];
      y = (char)packet[2] + (char)packet[4];
      z = ((char)(packet[5] << 1) + (char)(packet[6] << 1)) >> 1;
#else
      x = (char)packet[1];
      y = (char)packet[2];
#ifndef PC98
      /* XXX */
      if (packet[0] & 0x40) {
        if (packet[0] & 0x10) {
          x -= 128;
        } else {
          x += 128;
        }
      }

      if (packet[0] & 0x80) {
        if (packet[0] & 0x20) {
          y -= 128;
        } else {
          y += 128;
        }
      }
#endif
#endif

      move = 0;

      if (x != 0) {
        restore_hidden_region();

        _mouse.x += x;

        if (_mouse.x < 0) {
          _mouse.x = 0;
        } else if (_display.width <= _mouse.x) {
          _mouse.x = _display.width - 1;
        }

        move = 1;
      }

      if (y != 0) {
        restore_hidden_region();

        _mouse.y -= y;

        if (_mouse.y < 0) {
          _mouse.y = 0;
        } else if (_display.height <= _mouse.y) {
          _mouse.y = _display.height - 1;
        }

        move = 1;
      }

      if (move) {
        update_mouse_cursor_state();
      }

#if __FreeBSD__ >= 5
      if (~packet[0] & 0x04) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if (~packet[0] & 0x02) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if (~packet[0] & 0x01) {
        xev.button = Button3;
        _mouse.button_state = Button3Mask;
      } else if (z < 0) {
        xev.button = Button4;
        _mouse.button_state = Button4Mask;
      } else if (z > 0) {
        xev.button = Button5;
        _mouse.button_state = Button5Mask;
      }
#elif defined(PC98)
      if ((packet[0] & 0x4) == 0) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if ((packet[0] & 0x2) == 0) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if ((packet[0] & 0x01) == 0) {
        xev.button = Button3;
        _mouse.button_state = Button3Mask;
      }
#else
      if (packet[0] & 0x1) {
        xev.button = Button1;
        _mouse.button_state = Button1Mask;
      } else if (packet[0] & 0x4) {
        xev.button = Button2;
        _mouse.button_state = Button2Mask;
      } else if (packet[0] & 0x02) {
        xev.button = Button3;
        _mouse.button_state = Button3Mask;
      }
#endif
      else {
        xev.button = 0;
      }

      if (move) {
        xev.type = MotionNotify;
        xev.state = _mouse.button_state | _display.key_state;
      } else {
        if (xev.button) {
          xev.type = ButtonPress;
        } else {
          xev.type = ButtonRelease;

          if (_mouse.button_state & Button1Mask) {
            xev.button = Button1;
          } else if (_mouse.button_state & Button2Mask) {
            xev.button = Button2;
          } else if (_mouse.button_state & Button3Mask) {
            xev.button = Button3;
          } else if (_mouse.button_state & Button4Mask) {
            xev.button = Button4;
          } else if (_mouse.button_state & Button5Mask) {
            xev.button = Button5;
          }

          /* Reset button_state in releasing button */
          _mouse.button_state = 0;
        }

        xev.state = _display.key_state;
      }

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

#ifdef __DEBUG
      bl_debug_printf(
          BL_DEBUG_TAG "Button is %s x %d y %d btn %d time %u\n",
          xev.type == ButtonPress ? "pressed" : xev.type == MotionNotify ? "motion" : "released",
          xev.x, xev.y, xev.button, xev.time);
#endif

      if (!check_virtual_kbd(&xev)) {
        win = get_window(xev.x, xev.y);
        xev.x -= win->x;
        xev.y -= win->y;

        ui_window_receive_event(win, &xev);
      }

      if (move) {
        save_hidden_region();
        draw_mouse_cursor();
      }
    }
  }

  return 1;
}

/* http://kaworu.jpn.org/doc/FreeBSD/jman.ORG/man4/keyboard.4.php */
static int receive_key_event(int fd) {
  u_char code;

  while (read(fd, &code, 1) == 1) {
    static int dead;
    XKeyEvent xev;
    int pressed;
    int idx;
    int key_state = _display.key_state;

    if (code & 0x80) {
      pressed = 0;
      code &= 0x7f;
    } else {
      pressed = 1;
    }

    if (code >= keymap.n_keys) {
      continue;
    }

    if (keymap.key[code].flgs & 2) {
      /* The key should react on num-lock(2). (Keypad keys) */

      int kcode;

      if ((kcode = keymap.key[code].map[0]) != 0 && pressed) {
        /*
         * KEY_KP0 etc are 0x100 larger than KEY_INSERT etc to
         * distinguish them.
         * (see ui.h)
         */
        xev.ksym = kcode + 0x200;

        goto send_event;
      } else {
        continue;
      }
    }

    if (_display.key_state) {
      /* CommandMask is ignored. */

      idx = (_display.key_state & ShiftMask);
      if (_display.key_state & ControlMask) {
        idx |= 2;
      }
      if (_display.key_state & Mod2Mask) { /* ALTGR */
        idx |= 4;
      }
    } else {
      idx = 0;
    }

    if (!(keymap.key[code].spcl & (1 << (7 - idx)))) {
      /* Character keys */

      if (pressed) {
        if ((keymap.key[code].flgs & 1) && (_display.lock_state & CLKED)) {
          /* xor shift bit(1) */
          idx ^= 1;
        }

#if 1
        if (code == 41) {
          xev.ksym = XK_Zenkaku_Hankaku;
        } else if (code == 121) {
          xev.ksym = XK_Henkan_Mode;
        } else if (code == 123) {
          xev.ksym = XK_Muhenkan;
        } else if (code == 28) {
          /*
           * Always convert code 28 to XK_Return
           *
           * key[28].map[0] (Normal)     -> 0xd
           * key[28].map[1] (Shift)      -> 0xd
           * key[28].map[2] (Control)    -> 0xa
           * key[28].map[3] (Shift+Ctrl) -> 0xa
           */
          xev.ksym = XK_Return;
        } else
#endif
        {
          xev.ksym = keymap.key[code].map[idx];
          if (xev.ksym >= 0x100) {
            /* Unicode */
            xev.ksym += 0x1000; /* See ui_window_get_str() in ui_window.c */
          } else if ((_display.key_state & Mod2Mask) /* ALTRIGHT */ &&
                     /*
                      * e.g.) German keyboard
                      *       Alt + ( -> [
                      *       map[0] = (, map[idx] = [
                      */
                     keymap.key[code].map[idx] != keymap.key[code].map[0]) {
            key_state &= ~ModMask;
          }
        }

        if (dead) {
          if ('a' <= (xev.ksym | 0x20) && (xev.ksym | 0x20) <= 'z') {
            if (load_accentmap()) {
              struct acc_t *acc = &accentmap->acc[dead - F_ACC];

              if (acc->accchar) {
                int count;

                for (count = 0; count < NUM_ACCENTCHARS; count++) {
                  if (acc->map[count][0] == 0) {
                    break;
                  } else if (acc->map[count][0] == xev.ksym) {
                    xev.ksym = acc->map[count][1];
                    break;
                  }
                }
              }
            }
          }

          dead = 0;
        }

        goto send_event;
      }
    } else {
      /* Function keys */

      int kcode;

      if ((kcode = keymap.key[code].map[0]) == 0) {
        /* do nothing */
      } else if (pressed) {
        int kcode2 = keymap.key[code].map[idx];

        if (F_ACC <= kcode2 && kcode2 <= L_ACC) {
          dead = kcode2;
        } else {
          dead = 0;

          if (kcode == KEY_RIGHTSHIFT || kcode == KEY_LEFTSHIFT) {
            _display.key_state |= ShiftMask;
          } else if (kcode == KEY_RIGHTCTRL || kcode == KEY_LEFTCTRL) {
            _display.key_state |= ControlMask;
          } else if (kcode == KEY_RIGHTALT) {
            _display.key_state |= (Mod1Mask|Mod2Mask);
          } else if (kcode == KEY_LEFTALT) {
            _display.key_state |= Mod1Mask;
          } else if (kcode == KEY_NUMLOCK) {
            _display.lock_state ^= NLKED;
          } else if (kcode == KEY_CAPSLOCK) {
            _display.lock_state ^= CLKED;
          } else {
            if (0x20 <= kcode && kcode <= 0x3f) {
              /*
               * Pressing Control+' '..'?' comes here with kcode = 0x20..0x3f but
               * they are not function keys.
               */
              xev.ksym = kcode;
            } else {
              /* function keys */
              xev.ksym = kcode + 0x100;
            }

            goto send_event;
          }
        }
      } else {
        if (kcode == KEY_RIGHTSHIFT || kcode == KEY_LEFTSHIFT) {
          _display.key_state &= ~ShiftMask;
        } else if (kcode == KEY_RIGHTCTRL || kcode == KEY_LEFTCTRL) {
          _display.key_state &= ~ControlMask;
        } else if (kcode == KEY_RIGHTALT) {
          _display.key_state &= ~(Mod1Mask|Mod2Mask);
        } else if (kcode == KEY_LEFTALT) {
          _display.key_state &= ~Mod1Mask;
        }
      }
    }

    continue;

  send_event:
    xev.type = KeyPress;
    xev.state = _mouse.button_state | key_state /* Don't use _display.key_state */ ;
    xev.keycode = code;

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG "scancode %d -> ksym 0x%x state 0x%x\n", code, xev.ksym,
                    xev.state);
#endif

    receive_event_for_multi_roots(&xev);
  }

  return 1;
}
