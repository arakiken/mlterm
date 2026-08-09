/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/vt.h> /* VT_GETSTATE */
#include <stdbool.h>

/*
 * <string.h> has already been included without _GNU_SOURCE,
 * so #define _GNU_SOURCE here doesn't work.
 */
#if 0
#define _GNU_SOURCE /* strcasestr */
#include <string.h>
#else
char *strcasestr(const char *haystack, const char *needle);
#endif

#if 0
#define READ_CTRL_KEYMAP
#endif

/* see /usr/include/linux/input.h */
#ifndef input_event_sec
#define input_event_sec time.tv_sec
#endif
#ifndef input_event_usec
#define input_event_usec time.tv_usec
#endif

/* --- static variables --- */

static int console_id = -1;
static bool have_keymap = false;

#ifdef KDGKBDIACRUC
static struct kbdiacrsuc *diacrs_uc;
#else
static struct kbdiacrs *diacrs;
#endif

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

    return state | ((state & (1 << KG_ALT)) ? Mod1Mask : 0);
  }
}

static int get_accented_char(int kb_value, int dead) {
  u_char table[] = { '`', '\'', '^', '~', '\"', ',' };
  u_char diacr = table[dead];

  if (dead <= 5) {
    u_int count;

#ifdef KDGKBDIACRUC
    if (diacrs_uc == NULL) {
      if ((diacrs_uc = malloc(sizeof(*diacrs_uc))) == NULL) { /* XXX Leaked */
        goto end;
      }
      ioctl(STDIN_FILENO, KDGKBDIACRUC, diacrs_uc);
#if 0
      {
        u_int count;
        for (count = 0; count < diacrs_uc.kb_cnt; count++) {
          bl_debug_printf("UC %x %x %x\n", diacrs_uc.kbdiacruc[count].diacr,
                          diacrs_uc.kbdiacruc[count].base,
                          diacrs_uc.kbdiacruc[count].result);
        }
      }
#endif
    }

    for (count = 0; count < diacrs_uc->kb_cnt; count++) {
      if (diacrs_uc->kbdiacruc[count].diacr == diacr &&
          diacrs_uc->kbdiacruc[count].base == kb_value) {
        return diacrs_uc->kbdiacruc[count].result;
      }
    }
#else
    if (diacrs == NULL) {
      if ((diacrs = malloc(sizeof(*diacrs))) == NULL) { /* XXX Leaked */
        goto end;
      }
      ioctl(STDIN_FILENO, KDGKBDIACR, diacrs);
#if 0
      {
        u_int count;
        for (count = 0; count < diacrs.kb_cnt; count++) {
          bl_debug_printf("%x %x %x\n", diacrs.kbdiacr[count].diacr,
                          diacrs.kbdiacr[count].base,
                          diacrs.kbdiacr[count].result);
        }
      }
#endif
    }

    for (count = 0; count < diacrs->kb_cnt; count++) {
      if (diacrs->kbdiacr[count].diacr == diacr &&
          diacrs->kbdiacr[count].base == kb_value) {
        return diacrs->kbdiacr[count].result;
      }
    }
#endif
  }

end:
  return kb_value;
}

static int kcode_to_ksym(int kcode, u_short kval, int *state) {
  if (kcode == KEY_ENTER || kcode == KEY_KPENTER) {
    /* KDGKBENT returns '\n'(0x0a) */
    return 0x0d;
  } else if (kcode == KEY_BACKSPACE) {
    /* KDGKBDENT returns 0x7f */
    return 0x08;
  } else if (kcode <= KEY_SLASH || kcode == KEY_SPACE || kcode == KEY_YEN || kcode == KEY_RO) {
    if (!have_keymap) {
      /* If have_keymap is false, kval is undefined value. (See receive_key_event()) */
      return kcode;
    }

    if ((*state) & (ShiftMask | Mod2Mask)) {
      struct kbentry ent;

      ent.kb_table = 0;
      if ((*state) & ShiftMask) {
        ent.kb_table |= K_SHIFTTAB;
      }
      if ((*state) & Mod2Mask) { /* ALTGR */
        ent.kb_table |= K_ALTTAB;
      }

      ent.kb_index = kcode;

      if (ioctl(STDIN_FILENO, KDGKBENT, &ent) == 0) {
#ifdef __DEBUG
        bl_debug_printf("KDGKBENT idx %x val %x tbl %x\n",
                        ent.kb_index, ent.kb_value, ent.kb_table);
#endif
        kval = ent.kb_value;
      }
    }

    if (kval != K_HOLE && kval != K_NOSUCHMAP) {
      static int dead = -1;

      if (kval >= 0x1000) {
        /* See ui_window_get_str() in ui_window.c */
        kval = (0xf000 - (kval & 0xf000)) + (kval & 0xfff) + 0x1000;
        dead = -1;
      } else {
        if (KTYP(kval) == KT_DEAD
#ifdef KT_DEAD2
            || KTYP(kval) == KT_DEAD2
#endif
            ) {
          dead = kval & 0xff;

          return 0;
        } else {
          int orig_kb_value = kval;

          kval &= 0xff;

          if (dead != -1) {
            kval = get_accented_char(kval, dead);
            if (kval >= 0x100) {
              /* See ui_window_get_str() in ui_window.c */
              kval += 0x1000;
            }
            dead = -1;
          }

          if ((*state) & Mod2Mask) {
            struct kbentry ent2;

            ent2.kb_table = 0;
            ent2.kb_index = kcode;

            /*
             * e.g.) German keyboard
             *       Alt + ( -> [
             *       ent2.kb_value = (, orig_kb_value = [
             */
            if (ioctl(STDIN_FILENO, KDGKBENT, &ent2) == 0 && orig_kb_value != ent2.kb_value) {
              *state &= ~ModMask;
            }
          }
        }
      }

#ifdef __DEBUG
      bl_debug_printf("-> val %x\n", kval);
#endif

#if 1
      /* XXX linux returns KEY_GRAVE for HankakuZenkaku key. */
      if (kcode == KEY_GRAVE && kval == '\x1b') {
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

      return kval;
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

          for (idx = 0; idx < BL_ARRAY_SIZE(mouse_names); idx++) {
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
  get_event_device_num_intern(kbd, mouse, "/sys/class/input/event%d/device/name");

  if (*kbd == -1 || *mouse == -1) {
    int k;
    int m;

    /*
     * XXX
     * /sys/class/input/input%d/event%d
     * => Note that The first %d and the second %d might not be the same number.
     */
    get_event_device_num_intern(&k, &m, "/sys/class/input/input%d/name");

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

static void convert_input_num(int *input_num, u_int size, const char *str) {
  char *p;

  do {
    if ((p = strchr(str, ','))) {
      *(p++) = '\0';
    }

    if ('0' <= *str && *str <= '9') {
      *(input_num++) = atoi(str);
    } else {
      break;
    }
  } while (--size > 0 && (str = p));
}

#ifdef USE_KMSDRM
#include <dirent.h>

static void close_kmsdrm_dumb(struct drm_dumb *dumb) {
  struct drm_mode_destroy_dumb destroy_req;

  if (dumb->fb != NULL) {
    munmap(dumb->fb, _display.smem_len);
  }

  if (dumb->fb_id > 0) {
    drmModeRmFB(_display.fb_fd, dumb->fb_id);
  }

  memset(&destroy_req, 0, sizeof(destroy_req));
  destroy_req.handle = dumb->create_handle;
  drmIoctl(_display.fb_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
}

static int open_kmsdrm_dumb(struct drm_dumb *dumb, struct drm_mode_create_dumb *create_req) {
  if (drmIoctl(_display.fb_fd, DRM_IOCTL_MODE_CREATE_DUMB, create_req) < 0) {
    return 0;
  }
  dumb->create_handle = create_req->handle;

  if (drmModeAddFB(_display.fb_fd, create_req->width, create_req->height,
                   create_req->bpp >= 24 ? 24 : create_req->bpp,
                   create_req->bpp, create_req->pitch, create_req->handle,
                   &dumb->fb_id) >= 0) {
    struct drm_mode_map_dumb map_req;

    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = create_req->handle;
    if (drmIoctl(_display.fb_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) >= 0) {
      u_char *fb = mmap(0, create_req->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        _display.fb_fd, map_req.offset);
      if (fb != MAP_FAILED) {
        dumb->fb = fb;

        return 1;
      }
    }
  }

  close_kmsdrm_dumb(dumb);

  return 0;
}

static int waiting_for_flip;

static void page_flip(void) {
  static int flip;

  memcpy(_display.drm_dumb[flip].fb, _display.fb, _display.smem_len);
  drmModePageFlip(_display.fb_fd, _display.drm_resource->crtcs[0] /* crtc_id */,
                  _display.drm_dumb[flip].fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
  waiting_for_flip = 1;
  _display.drm_damaged = 0;
  flip ^= 1;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec,
                              void *data) {
  waiting_for_flip = 0;

  if (_display.drm_damaged) {
    page_flip();
  }
}

static void process_drm_event(void) {
  static drmEventContext drm_event;

  if (drm_event.version == 0) {
    drm_event.version = 2;
    drm_event.page_flip_handler = page_flip_handler;
  }
  drmHandleEvent(_display.fb_fd, &drm_event);
}

/* ui_event_source.h */
int ui_event_source_add_fd(int fd, void (*handler)(void));

static int open_kmsdrm(char *dev) {
  drmModeModeInfo mode;
  struct drm_mode_create_dumb create_req;
  uint32_t crtc_id;
  int count;

  if (dev == NULL) {
    DIR *dir;

    if ((dir = opendir("/dev/dri")) != NULL) {
      struct dirent *ent;

      while (1) {
        if ((ent = readdir(dir)) == NULL) {
          closedir(dir);

          return 0;
        }

        if (strncmp(ent->d_name, "card", 4) == 0) {
          if ((dev = alloca(9 + strlen(ent->d_name) + 1)) == NULL) {
            closedir(dir);

            return 0;
          }
          sprintf(dev, "/dev/dri/%s", ent->d_name);

          break;
        }
      }

      closedir(dir);
    }
  }

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open(dev, O_RDWR | O_CLOEXEC);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev);

    return 0;
  }

  _display.drm_resource = drmModeGetResources(_display.fb_fd);
  if (_display.drm_resource == NULL) {
    goto error1;
  }

  for (count = 0; count < _display.drm_resource->count_connectors; count++) {
    _display.drm_connector = drmModeGetConnector(_display.fb_fd,
                                                 _display.drm_resource->connectors[count]);
    if (_display.drm_connector->connection == DRM_MODE_CONNECTED &&
        _display.drm_connector->count_modes > 0) {
      break;
    }
    drmModeFreeConnector(_display.drm_connector);
    _display.drm_connector = NULL;
  }

  if (_display.drm_connector == NULL) {
    goto error2;
  }

  mode = _display.drm_connector->modes[0];
  bl_msg_printf("Resolution: %dx%d @ %dHz\n", mode.hdisplay, mode.vdisplay, mode.vrefresh);

  crtc_id = _display.drm_resource->crtcs[0];
  _display.drm_saved_crtc = drmModeGetCrtc(_display.fb_fd, crtc_id);

  create_req.width = mode.hdisplay;
  create_req.height = mode.vdisplay;
  create_req.bpp = 32; /* ARGB8888 */
  if (!open_kmsdrm_dumb(&_display.drm_dumb[0], &create_req)) {
    goto error3;
  }

  memset(&create_req, 0, sizeof(create_req));
  create_req.width = mode.hdisplay;
  create_req.height = mode.vdisplay;
  create_req.bpp = 32; /* ARGB8888 */
  if (!open_kmsdrm_dumb(&_display.drm_dumb[1], &create_req)) {
    goto error4;
  }

  if ((_display.fb = _display.fb_base = malloc(create_req.size)) == NULL) {
    goto error5;
  }
  _display.smem_len = create_req.size;

  if (drmModeSetCrtc(_display.fb_fd, crtc_id, _display.drm_dumb[1].fb_id, 0, 0,
                     &_display.drm_connector->connector_id, 1, &mode) < 0) {
    goto error6;
  }

  _display.line_length = create_req.pitch;
  _display.xoffset = _display.yoffset = 0;

  _display.width = _disp.width = mode.hdisplay;
  _display.height = _disp.height = mode.vdisplay;

  _display.rgbinfo.r_limit = _display.rgbinfo.g_limit =
    _display.rgbinfo.b_limit = _display.rgbinfo.a_limit = 0;
  _display.rgbinfo.r_offset = 16;
  _display.rgbinfo.g_offset = 8;
  _display.rgbinfo.b_offset = 0;
  _display.rgbinfo.a_offset = 24;

  _disp.depth = 32;
  _display.pixels_per_byte = 1;
  _display.bytes_per_pixel = 4;

  ui_event_source_add_fd(_display.fb_fd, process_drm_event);

  return 1;

error6:
  free(_display.fb);

error5:
  close_kmsdrm_dumb(&_display.drm_dumb[1]);

error4:
  close_kmsdrm_dumb(&_display.drm_dumb[0]);

error3:
  drmModeFreeCrtc(_display.drm_saved_crtc);
  drmModeFreeConnector(_display.drm_connector);

error2:
  drmModeFreeResources(_display.drm_resource);

error1:
  close(_display.fb_fd);

  memset(&_display, 0, sizeof(_display));

  return 0;
}

void ui_display_flush(void) {
  if (_display.drm_resource != NULL) {
    _display.drm_damaged = 1;
    if (!waiting_for_flip) {
      page_flip();
    }
  }
}
#else
#define open_kmsdrm(dev) (0)
#endif

static int open_fb(char *dev) {
  struct fb_fix_screeninfo finfo;
  struct fb_var_screeninfo vinfo;

  if (dev == NULL) {
    dev = "/dev/fb0";
  }

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  _display.fb_fd = open(dev, O_RDWR);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (_display.fb_fd < 0) {
    bl_error_printf("Couldn't open %s.\n", dev);

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

  if ((_display.fb = _display.fb_base = mmap(NULL, (_display.smem_len = finfo.smem_len), PROT_WRITE | PROT_READ,
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
  _display.rgbinfo.a_limit = 8 - vinfo.transp.length;
  _display.rgbinfo.r_offset = vinfo.red.offset;
  _display.rgbinfo.g_offset = vinfo.green.offset;
  _display.rgbinfo.b_offset = vinfo.blue.offset;
  _display.rgbinfo.a_offset = vinfo.transp.offset;

  return 1;

error:
  if (_display.fb) {
    munmap(_display.fb, _display.smem_len);
    _display.fb = _display.fb_base = NULL;
  }

  close(_display.fb_fd);

  return 0;
}

static int open_display(u_int depth) {
  char *dev;
  int kbd_num[] = { -1, -1 };
  int mouse_num[] = { -1, -1 };
  struct termios tm;
  char kbd_type;

  dev = getenv("FRAMEBUFFER");
  if (!open_kmsdrm(dev) && !open_fb(dev)) {
    return 0;
  }

  get_event_device_num(kbd_num, mouse_num);

  if ((dev = getenv("KBD_INPUT_NUM"))) {
    convert_input_num(kbd_num, BL_ARRAY_SIZE(kbd_num), dev);
  }

  if ((dev = getenv("MOUSE_INPUT_NUM"))) {
    convert_input_num(mouse_num, BL_ARRAY_SIZE(mouse_num), dev);
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

  /* Detect if there is kernel keymap translation on the terminal - running on the console */
  if (ioctl(STDIN_FILENO, KDGKBTYPE, &kbd_type) == 0 &&
      (kbd_type == KB_101 || kbd_type == KB_84)) {
    have_keymap = true;
  } else {
    bl_msg_printf("No keymap");
  }

  /* Disable backscrolling of default console. */
  set_use_console_backscroll(0);

  if (kbd_num[0] == -1 || (_display.fd = open_event_device(kbd_num[0], NULL)) == -1) {
#ifdef DEBUG
    bl_debug_printf("KBD1: stdin\n");
#endif
    _display.fd = STDIN_FILENO;
  } else {
#ifdef DEBUG
    bl_debug_printf("KBD1: /dev/input/event%d\n", kbd_num[0]);
#endif
    bl_file_set_cloexec(_display.fd);
  }

  /* K_UNICODE is the default value. */
#if 0
  {
    u_long val = K_UNICODE;
    ioctl(STDIN_FILENO, KDSKBMODE, &val);
  }
#endif

  _disp.display = &_display;

  if ((mouse_num[0] == -1 || (_mouse.fd = open_event_device(mouse_num[0], NULL)) == -1) &&
      (_mouse.fd = open_event_device(0, "/dev/input/mice")) == -1 &&
      (_mouse.fd = open_event_device(0, "/dev/input/mouse0")) == -1) {
    _mouse.fd = -1;
  } else {
#ifdef DEBUG
    bl_debug_printf("MOUSE1: /dev/input/event%d\n", mouse_num[0]);
#endif
    bl_file_set_cloexec(_mouse.fd);
    _mouse.x = _display.width / 2;
    _mouse.y = _display.height / 2;
    _disp_mouse.display = (Display*)&_mouse;
    num_opened_displays = 2;
  }

  if (mouse_num[1] != -1) {
    static InputDevice _mouse2;
    static ui_display_t _disp_mouse2;

    if ((_mouse2.fd = open_event_device(mouse_num[1], NULL)) != -1) {
#ifdef DEBUG
      bl_debug_printf("MOUSE2: /dev/input/event%d\n", mouse_num[1]);
#endif
      _disp_mouse2.display = (Display*)&_mouse2;
      opened_disps[num_opened_displays++] = &_disp_mouse2;
    }
  }

  if (kbd_num[1] != -1) {
    static InputDevice _kbd2;
    static ui_display_t _disp_kbd2;

    if ((_kbd2.fd = open_event_device(kbd_num[1], NULL)) != -1) {
#ifdef DEBUG
      bl_debug_printf("KBD2: /dev/input/event%d\n", kbd_num[1]);
#endif
      _kbd2.is_kbd = 1;
      _disp_kbd2.display = (Display*)&_kbd2;
      opened_disps[num_opened_displays++] = &_disp_kbd2;
    }
  }

  console_id = get_active_console();

  return 1;

}

static int receive_mouse_event(int fd) {
  struct input_event ev;

  if (console_id != get_active_console()) {
    return 0;
  }

  while (read(fd, &ev, sizeof(ev)) > 0) {
    if (ev.type == EV_ABS) {
#ifdef EVIOCGABS
      static int max_abs_x;
      static int max_abs_y;
      static int prev_abs_x = -1;
      static int prev_abs_y = -1;
      int tmp;
#ifdef ABS_MT_TRACKING_ID
      static int mttrackingid;
#endif

      if (max_abs_x == 0 /* || max_abs_y == 0 */) {
        struct input_absinfo info;

        ioctl(fd, EVIOCGABS(ABS_X), &info);
        max_abs_x = info.maximum;

        ioctl(fd, EVIOCGABS(ABS_Y), &info);
        max_abs_y = info.maximum;
      }

#ifdef ABS_MT_TRACKING_ID
      if (ev.code == ABS_MT_TRACKING_ID) {
        if (((int)ev.value) != mttrackingid) {
          mttrackingid = ev.value;
          prev_abs_x = prev_abs_y = -1;
        }
      } else
#endif
      if (ev.code == ABS_PRESSURE) {
        ev.type = EV_KEY;
        ev.code = BTN_LEFT;
        ev.value = 1; /* ButtonPress */
      } else if (ev.code == ABS_X) {
        if (prev_abs_x == -1) {
          prev_abs_x = ev.value;
          continue;
        } else {
          ev.type = EV_REL;
          ev.code = REL_X;
          tmp = (((int)ev.value) - prev_abs_x) * ((int)_display.width) / max_abs_x;
          prev_abs_x = ev.value;
          ev.value = tmp;
        }
      } else if (ev.code == ABS_Y) {
        if (prev_abs_y == -1) {
          prev_abs_y = ev.value;
          continue;
        } else {
          ev.type = EV_REL;
          ev.code = REL_Y;
          tmp = (((int)ev.value) - prev_abs_y) * ((int)_display.height) / max_abs_y;
          prev_abs_y = ev.value;
          ev.value = tmp;
        }
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

      xev.time = ev.input_event_sec * 1000 + ev.input_event_usec / 1000;
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

        ui_window_receive_event(win, (XEvent*)&xev);
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
      xev.time = ev.input_event_sec * 1000 + ev.input_event_usec / 1000;
      xev.state = _mouse.button_state | _display.key_state;

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Button is moved %d x %d y %d btn %d time %d\n", xev.type,
                      xev.x, xev.y, xev.state, xev.time);
#endif

      win = get_window(xev.x, xev.y);
      xev.x -= win->x;
      xev.y -= win->y;

      ui_window_receive_event(win, (XEvent*)&xev);

      save_hidden_region();
      draw_mouse_cursor();
    }
  }

  return 1;
}

static int receive_key_event(int fd) {
  if (fd == STDIN_FILENO) {
    return receive_stdin_key_event();
  } else {
    struct input_event ev;

    if (console_id != get_active_console()) {
      return 0;
    }

    while (read(fd, &ev, sizeof(ev)) > 0) {
      if (ev.type == EV_KEY && ev.code < 0x100 /* Key event is less than 0x100 */) {
        bool shift, caps, ctrl, alt, altgr, num;
        struct kbentry ent;

        ent.kb_table = 0;
        ent.kb_index = ev.code;

        if (have_keymap && ioctl(STDIN_FILENO, KDGKBENT, &ent) == 0 &&
            ent.kb_value != K_HOLE && ent.kb_value != K_NOSUCHMAP) {
#ifdef __DEBUG
          bl_debug_printf("KDGKBENT idx %x val %x tbl %x\n",
                          ent.kb_index, ent.kb_value, ent.kb_table);
#endif
          shift = ent.kb_value == K_SHIFT;
          caps = ent.kb_value == K_CAPS;
          ctrl = ent.kb_value == K_CTRL;
          alt = ent.kb_value == K_ALT;
          altgr = ent.kb_value == K_ALTGR;
          num = ent.kb_value == K_NUM;
        } else {
          shift = ev.code == KEY_RIGHTSHIFT || ev.code == KEY_LEFTSHIFT;
          caps = ev.code == KEY_CAPSLOCK;
          ctrl = ev.code == KEY_RIGHTCTRL || ev.code == KEY_LEFTCTRL;
          alt = ev.code == KEY_LEFTALT;
          altgr = ev.code == KEY_RIGHTALT;
          num = ev.code == KEY_NUMLOCK;
        }

        if (ev.value == 1 /* Pressed */ || ev.value == 2 /* auto repeat */) {
          if (shift) {
            _display.key_state |= ShiftMask;
          } else if (caps) {
            if (_display.key_state & ShiftMask) {
              _display.key_state &= ~ShiftMask;
            } else {
              _display.key_state |= ShiftMask;
            }
          } else if (ctrl) {
            _display.key_state |= ControlMask;
          } else if (altgr) {
            _display.key_state |= (Mod1Mask|Mod2Mask);
          } else if (alt) {
            _display.key_state |= Mod1Mask;
          } else if (num) {
            _display.lock_state ^= NLKED;
          } else {
            XKeyEvent xev;

            xev.type = KeyPress;
            xev.state = _mouse.button_state | _display.key_state;
            xev.keycode = ev.code;

            if ((xev.ksym = kcode_to_ksym(ev.code, ent.kb_value, &xev.state)) > 0) {
              receive_event_for_multi_roots((XEvent*)&xev);
            }
          }
        } else if (ev.value == 0 /* Released */) {
          if (shift) {
            _display.key_state &= ~ShiftMask;
          } else if (ctrl) {
            _display.key_state &= ~ControlMask;
          } else if (altgr) {
            _display.key_state &= ~(Mod1Mask|Mod2Mask);
          } else if (alt) {
            _display.key_state &= ~Mod1Mask;
          }
        }
      }
    }
  }

  return 1;
}
