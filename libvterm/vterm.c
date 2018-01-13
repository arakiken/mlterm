/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <vterm.h>

#include <stdio.h> /* sprintf */
#include <string.h>
#include <stdlib.h> /* getenv */
#include <unistd.h> /* STDOUT_FILENO */
#include <sys/ioctl.h> /* TIOCGWINSZ */
#include <errno.h>
#include <pobl/bl_def.h> /* WORDS_BIGENDIGN */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <mef/ef_utf32_parser.h>
#include <mef/ef_utf8_conv.h>
#include <mef/ef_utf8_parser.h>
#include <mef/ef_ucs4_map.h>
#include <vt_pty_intern.h>
#include <mlterm.h>

#define SIXEL_1BPP
#include "../common/c_sixel.c"

typedef struct VTerm {
  vt_term_t *term;
  vt_pty_ptr_t pty;
  vt_pty_event_listener_t pty_listener;
  vt_config_event_listener_t config_listener;
  vt_screen_event_listener_t screen_listener;
  vt_xterm_event_listener_t xterm_listener;

  VTermColor default_fg;
  VTermColor default_bg;

  int mouse_col;
  int mouse_row;
  int mouse_button;

  u_char drcs_charset;
  u_char drcs_plane; /* '0'(94) or '1'(96) */
  u_int col_width;
  u_int line_height;

  const VTermScreenCallbacks *vterm_screen_cb;
  void *vterm_screen_cbdata;
} VTerm;

/* --- static functions --- */

static int final(vt_pty_ptr_t pty) { return 1; }

static ssize_t pty_write(vt_pty_ptr_t pty, u_char* buf, size_t len) { return 0; }

static ssize_t pty_read(vt_pty_ptr_t pty, u_char* buf, size_t len) { return 0; }

static int set_winsize(vt_pty_ptr_t pty, u_int cols, u_int rows, u_int width, u_int height) {
  return 1;
}

static vt_pty_ptr_t loopback_pty(void) {
  vt_pty_ptr_t pty;

  if ((pty = calloc(1, sizeof(vt_pty_t)))) {
    pty->master = pty->slave = -1;
    pty->child_pid = -1;

    pty->write = pty_write;
    pty->read = pty_read;
    pty->final = final;
    pty->set_winsize = set_winsize;

    vt_config_menu_init(&pty->config_menu);
  }

  return pty;
}

static void pty_closed(void *p) {
  ((VTerm*)p)->term = NULL;
}

static void set_config(void *p, char *dev, char *key, char *value) {
  VTerm *vterm;

  vterm = p;

  if (vt_term_set_config(vterm->term, key, value)) {
    /* do nothing */
  } else if (strcmp(key, "encoding") == 0) {
    vt_char_encoding_t encoding;

    if ((encoding = vt_get_char_encoding(value)) != VT_UNKNOWN_ENCODING) {
      vt_term_change_encoding(vterm->term, encoding);
    }
  }
}

static void get_config(void *p, char *dev, char *key, int to_menu) {
  VTerm *vterm = p;
  vt_term_t *term;
  char *value;

  if (dev) {
    if (!(term = vt_get_term(dev))) {
      return;
    }
  } else {
    term = vterm->term;
  }

  if (vt_term_get_config(term, vterm->term, key, /* to_menu */ 0, NULL)) {
    return;
  }

  value = NULL;

  if (strcmp(key, "pty_list") == 0) {
    value = vt_get_pty_list();
  }

  if (!value) {
    vt_term_response_config(vterm->term, "error", NULL, to_menu);
  } else {
    vt_term_response_config(vterm->term, key, value, to_menu);
  }
}

static VTermPos get_cursor_pos(vt_term_t *term) {
  VTermPos pos;

  pos.col = vt_term_cursor_col(term);
  pos.row = vt_term_cursor_row(term);

  return pos;
}

static void update_screen(VTerm *vterm) {
  int row;

  for (row = 0; row < vt_term_get_rows(vterm->term); row++) {
    vt_line_t *line;

    if ((line = vt_term_get_line_in_screen(vterm->term, row)) && vt_line_is_modified(line)) {
      VTermRect r;
      r.start_row = r.end_row = row;
      r.start_col = line->change_beg_col;
      r.end_col = line->change_end_col;

#ifdef __DEBUG
      bl_debug_printf("MODIFIED %d-%d at %d\n", r.start_col, r.end_col, r.end_row);
#endif

      (*vterm->vterm_screen_cb->damage)(r, vterm->vterm_screen_cbdata);

      vt_line_set_updated(line);
    }
  }
}

static void get_cell_size(VTerm *vterm) {
#ifdef TIOCGWINSZ
  struct winsize ws;

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_xpixel > 0 &&
      ws.ws_col > 0 && ws.ws_ypixel > 0 && ws.ws_row > 0) {
    vterm->col_width = ws.ws_xpixel / ws.ws_col;
    vterm->line_height = ws.ws_ypixel / ws.ws_row;
    bl_msg_printf("Cell size is %dx%d\n", vterm->col_width, vterm->line_height);

    return;
  }
#endif

  bl_msg_printf("Regard cell size as 8x16\n");
  vterm->col_width = 8;
  vterm->line_height = 16;
}

static void write_to_stdout(u_char *buf, size_t len) {
  ssize_t written;

  while (1) {
    if ((written = write(STDOUT_FILENO, buf, len)) < 0) {
      if (errno == EAGAIN) {
        usleep(100); /* 0.1 msec */
      } else {
        return;
      }
    } else if (written == len) {
      return;
    } else {
      buf += written;
      len -= written;
    }
  }
}

static inline void switch_94_96_cs(VTerm *vterm) {
  if (vterm->drcs_plane == '0') {
    vterm->drcs_plane = '1';
  } else {
    vterm->drcs_plane = '0';
  }
  vterm->drcs_charset = '0';
}

static vt_char_t *xterm_get_picture_data(void *p, char *file_path,
                                         int *num_cols, /* If *num_cols > 0, ignored. */
                                         int *num_rows, /* If *num_rows > 0, ignored. */
                                         int *num_cols_small /* set only if drcs_sixel is 1. */,
                                         int *num_rows_small /* set only if drcs_sixel is 1. */,
                                         u_int32_t **sixel_palette, int drcs_sixel) {
  static int old_drcs_sixel = -1;
  VTerm *vterm = p;
  u_int width;
  u_int height;
  FILE *fp;
  u_char data[1024];
  u_char *data_p;
  u_char *all_data;
  size_t len;
  size_t skipped_len;
  int x, y;
  vt_char_t *buf;
  u_int buf_size;
  char seq[24 + 4 /*%dx2*/ + 2 /*%cx2*/ + 1]; /* \x1b[?8800h\x1bP1;0;0;%d;1;3;%d;%c{ %c */

  if (strcasecmp(file_path + strlen(file_path) - 4, ".six") != 0 || /* accepts sixel alone */
      !(fp = fopen(file_path, "r"))) {
    return NULL;
  }

  len = fread(data, 1, sizeof(data) - 1, fp);
  if (len < 2) {
    goto error_closing_fp;
  }

  data[len] = '\0';

  if (*data == 0x90) {
    data_p = data + 1;
  } else if (strncmp(data, "\x1bP", 2) == 0) {
    data_p = data + 2;
  } else {
    goto error_closing_fp;
  }

  while ('0' <= *data_p && *data_p <= ';') { data_p++; }

  if (*data_p != 'q') {
    goto error_closing_fp;
  }
  data_p ++;

  if (old_drcs_sixel == -1) {
    const char *env = getenv("DRCS_SIXEL");
    if (env && strcmp(env, "old") == 0) {
      old_drcs_sixel = 1;
      write_to_stdout("\x1b]5379;old_drcs_sixel=true\x07", 27); /* for mlterm */
    } else {
      old_drcs_sixel = 0;
      write_to_stdout("\x1b]5379;old_drcs_sixel=false\x07", 28); /* for mlterm */
    }
  }

  if (sscanf(data_p, "\"%d;%d;%d;%d", &x, &y, &width, &height) != 4 ||
      width == 0 || height == 0) {
    struct stat st;
    u_char *picture;

    fstat(fileno(fp), &st);

    if (!(all_data = malloc(st.st_size + 1))) {
      goto error_closing_fp;
    }

    memcpy(all_data, data, len);
    len += fread(all_data + len, 1, st.st_size - len, fp);
    all_data[len] = '\0';

    if (!(picture = load_sixel_from_data_1bpp(all_data, &width, &height))) {
      free(all_data);

      goto error_closing_fp;
    }

    free(picture);

    skipped_len = (data_p - data); /* skip DCS P ... q */
    data_p = all_data + skipped_len;
  } else {
    /* "%d;%d;%d;%d */

    if (old_drcs_sixel) {
      /* skip "X;X;X;X (rlogin 2.23.0 doesn't recognize it) */
      data_p++;
      while ('0' <= *data_p && *data_p <= ';') { data_p++; }
    }

    skipped_len = (data_p - data);
    all_data = NULL;
  }

  len -= skipped_len;

  if (vterm->drcs_charset == '\0') {
    vterm->drcs_charset = '0';
    vterm->drcs_plane = '0';
    get_cell_size(vterm);

    /* Pcmw >= 5 in DECDLD */
    if (vterm->col_width < 5 || 99 < vterm->col_width || 99 < vterm->line_height) {
      free(all_data);

      goto error_closing_fp;
    }
  }

  if (old_drcs_sixel) {
    /* compatible with old rlogin (2.23.0 or before) */
    *num_cols = width / vterm->col_width;
    *num_rows = height / vterm->line_height;
  } else {
    *num_cols = (width + vterm->col_width - 1) / vterm->col_width;
    *num_rows = (height + vterm->line_height - 1) / vterm->line_height;
  }

  buf_size = (*num_cols) * (*num_rows);

#if 0
  /*
   * XXX
   * The way of drcs_charset increment from 0x7e character set may be different
   * between terminal emulators.
   */
  if (vterm->drcs_charset > '0' &&
      (buf_size + 0x5f) / 0x60 > 0x7e - vterm->drcs_charset + 1) {
    switch_94_96_cs(vterm);
  }
#endif

  sprintf(seq, "\x1b[?8800h\x1bP1;0;0;%d;1;3;%d;%c{ %c",
          vterm->col_width, vterm->line_height, vterm->drcs_plane, vterm->drcs_charset);
  write_to_stdout(seq, strlen(seq));
  while (1) {
    write_to_stdout(data_p, len);
    if ((len = fread(data, 1, sizeof(data), fp)) == 0) {
      break;
    }
    data_p = data;
  }

  free(all_data);
  fclose(fp);

#if 0
  bl_debug_printf("Image cols %d/%d=%d rows %d/%d=%d\n",
                  width, vterm->col_width, *num_cols, height, vterm->line_height, *num_rows);
#endif

  if ((buf = vt_str_new(buf_size))) {
    vt_char_t *buf_p;
    int col;
    int row;
    u_int code;

    code = 0x100020 + (vterm->drcs_plane == '1' ? 0x80 : 0) + vterm->drcs_charset * 0x100;

    buf_p = buf;
    for (row = 0; row < *num_rows; row++) {
      for (col = 0; col < *num_cols; col++) {
#if 0
        /* for old rlogin */
        if (code == 0x20) {
          vt_char_copy(buf_p++, vt_sp_ch());
        } else
#endif
        {
          /* pua is ambiguous width but always regarded as halfwidth. */
          vt_char_set(buf_p++, code++, ISO10646_UCS4_1, 0 /* fullwidth */, 0 /* comb */,
                      VT_FG_COLOR, VT_BG_COLOR, 0 /* bold */, 0 /* italic */, 0 /* line_style */,
                      0 /* blinking */, 0 /* protected */);
          if ((code & 0x7f) == 0x0) {
#if 0
            /* for old rlogin */
            code = 0x20;
#else
            if (vterm->drcs_charset == 0x7e) {
              switch_94_96_cs(vterm);
            } else {
              vterm->drcs_charset++;
            }

            code = 0x100020 + (vterm->drcs_plane == '1' ? 0x80 : 0) +
                   vterm->drcs_charset * 0x100;
#endif
          }
        }
      }
    }

    if (vterm->drcs_charset == 0x7e) {
      switch_94_96_cs(vterm);
    } else {
      vterm->drcs_charset++;
    }

    return buf;
  }

  return NULL;

error_closing_fp:
  fclose(fp);

  return NULL;
}

static void xterm_bel(void *p) {
  VTerm *vterm = p;

  (*vterm->vterm_screen_cb->bell)(vterm->vterm_screen_cbdata);
}

static void line_scrolled_out(void *p) {
  VTerm *vterm = p;
  VTermScreenCell *cells;
  u_int cols = vt_term_get_cols(vterm->term);

  if ((cells = alloca(sizeof(VTermScreenCell) * cols))) {
    VTermPos pos;

    /*
     * Note that scrolled out line hasn't been added to vt_logs_t yet here.
     * (see receive_scrolled_out_line() in vt_screen.c)
     */
    pos.row = 0;
    for (pos.col = 0; pos.col < cols; pos.col++) {
      vterm_screen_get_cell(vterm, pos, cells + pos.col);
    }

    (*vterm->vterm_screen_cb->sb_pushline)(cols, cells, vterm->vterm_screen_cbdata);
  }
}

/* --- global functions --- */

VTerm *vterm_new(int rows, int cols) {
  VTerm *vterm;

  if (!(vterm = calloc(1, sizeof(VTerm)))) {
    return NULL;
  }

  vterm->pty_listener.self = vterm;
  vterm->pty_listener.closed = pty_closed;
#if 0
  vterm->pty_listener.show_config = show_config;
#endif

  vterm->config_listener.self = vterm;
  vterm->config_listener.set = set_config;
  vterm->config_listener.get = get_config;
#if 0
  vterm->config_listener.exec = exec_cmd;
#endif

  vterm->screen_listener.self = vterm;
  vterm->screen_listener.line_scrolled_out = line_scrolled_out;

  vterm->xterm_listener.self = vterm;
#if 0
  vterm->xterm_listener.resize = resize;
  vterm->xterm_listener.set_mouse_report = xterm_set_mouse_report;
  /*
   * XXX
   * If xterm_get_rgb is implemented, "#0;9;X;X;X" is prepended to sixel sequence
   * stored in file_path of xterm_get_picture_data, so the way of skipping "\"%d;%d;%d;%d"
   * in xterm_get_picture_data should be fixed.
   */
  vterm->xterm_listener.get_rgb = xterm_get_rgb;
#endif
  vterm->xterm_listener.bel = xterm_bel;
  vterm->xterm_listener.get_picture_data = xterm_get_picture_data;

  vterm->pty = loopback_pty();
  vterm->term = mlterm_open(NULL, NULL, cols, rows, 1, NULL, NULL, &vterm->xterm_listener,
                            &vterm->config_listener, &vterm->screen_listener,
                            &vterm->pty_listener, 0);
#if 1
  /* col_size_of_width_a is forcibly 1 if locale is ja_XX.XX */
  vterm->term->parser->col_size_of_width_a = 1;
#endif
  vt_term_plug_pty(vterm->term, vterm->pty);

  vterm->default_fg.red = vterm->default_fg.green = vterm->default_fg.blue = 240;
  vterm->default_bg.red = vterm->default_bg.green = vterm->default_bg.blue = 0;

  return vterm;
}

VTerm *vterm_new_with_allocator(int rows, int cols, VTermAllocatorFunctions *funcs,
                                void *allocdata) { return NULL; }

void vterm_free(VTerm* vterm) {
  vt_destroy_term(vterm->term);
  free(vterm);

#if 0
  mlterm_final();
  bl_mem_dump_all();
#endif
}

void vterm_get_size(const VTerm *vterm, int *rowsp, int *colsp) {
  *rowsp = vt_term_get_rows(vterm->term);
  *colsp = vt_term_get_cols(vterm->term);
}

void vterm_set_size(VTerm *vterm, int rows, int cols) {
#ifdef __DEBUG
  bl_debug_printf("Resize screen to cols %d rows %d\n", cols, rows);
#endif

  vt_term_resize(vterm->term, cols, rows, 0, 0);
}

int vterm_get_utf8(const VTerm *vterm) {
  if (vt_term_get_encoding(vterm->term) == VT_UTF8) {
    return 1;
  } else {
    return 0;
  }
}

void vterm_set_utf8(VTerm *vterm, int is_utf8) {
  vt_char_encoding_t encoding;

  if (is_utf8) {
    encoding = VT_UTF8;
  } else {
    encoding = vt_get_char_encoding("auto");
  }

  vt_term_change_encoding(vterm->term, encoding);
}

size_t vterm_input_write(VTerm *vterm, const char *bytes, size_t len) {
  VTermPos oldpos = get_cursor_pos(vterm->term);

  vt_term_write_loopback(vterm->term, bytes, len);

  if (vterm->vterm_screen_cb) {
    (*vterm->vterm_screen_cb->movecursor)(get_cursor_pos(vterm->term), oldpos,
                                          vt_term_is_visible_cursor(vterm->term),
                                          vterm->vterm_screen_cbdata);
  }

  return len;
}

size_t vterm_output_get_buffer_size(const VTerm *vterm) {
  return vterm->pty->size;
}

size_t vterm_output_get_buffer_current(const VTerm *vterm) {
  return vterm->pty->size;
}

size_t vterm_output_get_buffer_remaining(const VTerm *vterm) {
  return vterm->pty->left;
}

size_t vterm_output_read(VTerm *vterm, char *buffer, size_t len) {
  if (vterm->pty->left == 0) {
    return 0;
  }

  if (len < vterm->pty->left) {
    memcpy(buffer, vterm->pty->buf, len);
    memmove(vterm->pty->buf, vterm->pty->buf + len, vterm->pty->left - len);
    vterm->pty->left -= len;
  } else {
    len = vterm->pty->left;
    vterm->pty->left = 0;
    memcpy(buffer, vterm->pty->buf, len);
  }

  return len;
}

void vterm_keyboard_unichar(VTerm *vterm, uint32_t c, VTermModifier mod) {
  static ef_parser_t *utf32_parser;
  static ef_conv_t *utf8_conv;
  u_char buf[UTF_MAX_SIZE];
  size_t len;

  if (!utf32_parser) {
#ifdef WORDS_BIGENDIAN
    utf32_parser = ef_utf32_parser_new();
#else
    utf32_parser = ef_utf32le_parser_new();
#endif
    utf8_conv = ef_utf8_conv_new();
  }

  (*utf32_parser->init)(utf32_parser);
  (*utf32_parser->set_str)(utf32_parser, &c, 4);
  (*utf8_conv->init)(utf8_conv);
  len = (*utf8_conv->convert)(utf8_conv, buf, sizeof(buf), utf32_parser);

  vt_term_write(vterm->term, buf, len);
}

void vterm_keyboard_key(VTerm *vterm, VTermKey key, VTermModifier mod) {
  u_char buf[1];
  vt_special_key_t mlkey;

  if (key == VTERM_KEY_ENTER) {
    buf[0] = '\r';
  } else if (key == VTERM_KEY_TAB) {
    buf[0] = '\t';
  } else {
    if (VTERM_KEY_FUNCTION_0 + 1 <= key && key <= VTERM_KEY_FUNCTION_0 + 37) {
      mlkey = SPKEY_F1 + (key - VTERM_KEY_FUNCTION_0 - 1);
    } else {
      switch(key) {
      case VTERM_KEY_BACKSPACE: mlkey = SPKEY_BACKSPACE; break;
      case VTERM_KEY_ESCAPE: mlkey = SPKEY_ESCAPE; break;
      case VTERM_KEY_UP: mlkey = SPKEY_UP; break;
      case VTERM_KEY_DOWN: mlkey = SPKEY_DOWN; break;
      case VTERM_KEY_LEFT: mlkey = SPKEY_LEFT; break;
      case VTERM_KEY_RIGHT: mlkey = SPKEY_RIGHT; break;
      case VTERM_KEY_INS: mlkey = SPKEY_INSERT; break;
      case VTERM_KEY_DEL: mlkey = SPKEY_DELETE; break;
      case VTERM_KEY_HOME: mlkey = SPKEY_HOME; break;
      case VTERM_KEY_END: mlkey = SPKEY_END; break;
      case VTERM_KEY_PAGEUP: mlkey = SPKEY_PRIOR; break;
      case VTERM_KEY_PAGEDOWN: mlkey = SPKEY_NEXT; break;
      case VTERM_KEY_KP_0: /* mlkey = SPKEY_KP_0; */ buf[0] = '0'; goto next_step;
      case VTERM_KEY_KP_1: /* mlkey = SPKEY_KP_1; */ buf[0] = '1'; goto next_step;
      case VTERM_KEY_KP_2: /* mlkey = SPKEY_KP_2; */ buf[0] = '2'; goto next_step;
      case VTERM_KEY_KP_3: /* mlkey = SPKEY_KP_3; */ buf[0] = '3'; goto next_step;
      case VTERM_KEY_KP_4: /* mlkey = SPKEY_KP_4; */ buf[0] = '4'; goto next_step;
      case VTERM_KEY_KP_5: /* mlkey = SPKEY_KP_5; */ buf[0] = '5'; goto next_step;
      case VTERM_KEY_KP_6: /* mlkey = SPKEY_KP_6; */ buf[0] = '6'; goto next_step;
      case VTERM_KEY_KP_7: /* mlkey = SPKEY_KP_7; */ buf[0] = '7'; goto next_step;
      case VTERM_KEY_KP_8: /* mlkey = SPKEY_KP_8; */ buf[0] = '8'; goto next_step;
      case VTERM_KEY_KP_9: /* mlkey = SPKEY_KP_9; */ buf[0] = '9'; goto next_step;
      case VTERM_KEY_KP_MULT: mlkey = SPKEY_KP_MULTIPLY; break;
      case VTERM_KEY_KP_PLUS: mlkey = SPKEY_KP_ADD; break;
      case VTERM_KEY_KP_COMMA: /* mlkey = SPKEY_KP_COMMA; */ buf[0] = ','; goto next_step;
      case VTERM_KEY_KP_MINUS: mlkey = SPKEY_KP_SUBTRACT; break;
      case VTERM_KEY_KP_PERIOD: mlkey = SPKEY_KP_DELETE; break;
      case VTERM_KEY_KP_DIVIDE: mlkey = SPKEY_KP_DIVIDE; break;
      case VTERM_KEY_KP_ENTER: mlkey = SPKEY_KP_SEPARATOR; break;
      case VTERM_KEY_KP_EQUAL: /* mlkey = SPKEY_KP_EQUAL; */ buf[0] = '='; goto next_step;
      default: return;
      }
    }

    if (vt_term_write_special_key(vterm->term, mlkey, mod, 0)) {
      return;
    }
  }

next_step:
  vt_term_write(vterm->term, buf, 1);
}

void vterm_keyboard_start_paste(VTerm *vterm) {}

void vterm_keyboard_end_paste(VTerm *vterm) {}

void vterm_mouse_move(VTerm *vterm, int row, int col, VTermModifier mod) {
#if 0
  bl_debug_printf("MOUSE col %d row %d\n", col, row);
#endif

  vterm->mouse_col = col;
  vterm->mouse_row = row;

  if (vt_term_get_mouse_report_mode(vterm->term) >= BUTTON_EVENT_MOUSE_REPORT) {
    vt_term_report_mouse_tracking(vterm->term, col + 1, row + 1, vterm->mouse_button,
                                  vterm->mouse_button ? 1 : 0, mod, 0);
  }
}

void vterm_mouse_button(VTerm *vterm, int button, bool pressed, VTermModifier mod) {
#if 0
  bl_debug_printf("MOUSE button %d %s\n", button, pressed ? "pressed" : "released");
#endif

  if (pressed) {
    vterm->mouse_button = button;
  } else {
    vterm->mouse_button = 0;
  }

  if (vt_term_get_mouse_report_mode(vterm->term)) {
    vt_term_report_mouse_tracking(vterm->term, vterm->mouse_col + 1, vterm->mouse_row + 1,
                                  button, !pressed, mod, 0);
  }
}

void vterm_parser_set_callbacks(VTerm *vterm, const VTermParserCallbacks *callbacks, void *user) {
  bl_warn_printf("vterm_parser_set_callbacks() is not supported.\n");
}

void *vterm_parser_get_cbdata(VTerm *vterm) { return NULL; }

VTermState *vterm_obtain_state(VTerm *vterm) { return vterm; }

void vterm_state_set_callbacks(VTermState *state, const VTermStateCallbacks *callbacks,
                                void *user) {
  bl_warn_printf("vterm_state_set_callbacks() is not supported.\n");
}

void *vterm_state_get_cbdata(VTermState *state) { return NULL; }

void vterm_state_set_unrecognised_fallbacks(VTermState *state,
                                             const VTermParserCallbacks *fallbacks, void *user) {}

void *vterm_state_get_unrecognised_fbdata(VTermState *state) { return NULL; }

void vterm_state_reset(VTermState *state, int hard) {
  VTerm *vterm = state;

  if (hard) {
    vt_term_write_loopback(vterm->term, "\x1bc", 2); /* RIS */
  } else {
    vt_term_write_loopback(vterm->term, "\x1b[!p", 4); /* DECSTR */
  }
}

void vterm_state_get_cursorpos(const VTermState *state, VTermPos *cursorpos) {
  *cursorpos = get_cursor_pos((VTerm*)state);
}

void vterm_state_get_default_colors(const VTermState *state, VTermColor *default_fg,
                                    VTermColor *default_bg) {
  VTerm *vterm = state;

  *default_fg = vterm->default_fg;
  *default_bg = vterm->default_bg;
}

void vterm_state_get_palette_color(const VTermState *state, int index, VTermColor *col) {
  u_int8_t r, g, b;

  vt_get_color_rgba(index, &r, &g, &b, NULL);
  col->red = r;
  col->green = g;
  col->blue = b;
}

void vterm_state_set_default_colors(VTermState *state, const VTermColor *default_fg,
                                    const VTermColor *default_bg) {
  VTerm *vterm = state;

  vterm->default_fg = *default_fg;
  vterm->default_bg = *default_bg;
}

void vterm_state_set_palette_color(VTermState *state, int index, const VTermColor *col) {
  char rgb[8];
  sprintf(rgb, "#%.2x%.2x%.2x", col->red, col->green, col->blue);
  vt_customize_color_file(vt_get_color_name(index), rgb, 0);
}

void vterm_state_set_bold_highbright(VTermState *state, int bold_is_highbright) {}

int  vterm_state_get_penattr(const VTermState *state, VTermAttr attr, VTermValue *val) {
  /* XXX */
  return 0;
}

int vterm_state_set_termprop(VTermState *state, VTermProp prop, VTermValue *val) {
  /* XXX */
  return 0;
}

const VTermLineInfo *vterm_state_get_lineinfo(const VTermState *state, int row) {
  /* XXX */
  return NULL;
}

VTermScreen *vterm_obtain_screen(VTerm *vterm) { return vterm; }

void  vterm_screen_set_callbacks(VTermScreen *screen, const VTermScreenCallbacks *callbacks,
                                 void *user) {
  VTerm *vterm = screen;

  vterm->vterm_screen_cbdata = user;
  vterm->vterm_screen_cb = callbacks;
}

void *vterm_screen_get_cbdata(VTermScreen *screen) { return NULL; }

void  vterm_screen_set_unrecognised_fallbacks(VTermScreen *screen,
                                              const VTermParserCallbacks *fallbacks, void *user) {}

void *vterm_screen_get_unrecognised_fbdata(VTermScreen *screen) { return NULL; }

void vterm_screen_enable_altscreen(VTermScreen *screen, int altscreen) {}

void vterm_screen_flush_damage(VTermScreen *screen) {
  update_screen(screen);
}

void vterm_screen_set_damage_merge(VTermScreen *screen, VTermDamageSize size) {}

void vterm_screen_reset(VTermScreen *screen, int hard) {
  vterm_state_reset(screen, hard);
  vterm_screen_flush_damage(screen);
}

size_t vterm_screen_get_chars(const VTermScreen *screen, uint32_t *chars, size_t len,
                              const VTermRect rect) {
  /* XXX */
  return 0;
}

size_t vterm_screen_get_text(const VTermScreen *screen, char *str, size_t len,
                             const VTermRect rect) {
  /* XXX */
  return 0;
}

int vterm_screen_get_attrs_extent(const VTermScreen *screen, VTermRect *extent, VTermPos pos,
                                  VTermAttrMask attrs) {
  /* XXX */
  return 0;
}

int vterm_screen_get_cell(const VTermScreen *screen, VTermPos pos, VTermScreenCell *cell) {
  VTerm *vterm = screen;
  vt_line_t *line;
  int col = pos.col;
  int row = pos.row;
  vt_color_t fg = VT_FG_COLOR;
  vt_color_t bg = VT_BG_COLOR;
  u_int8_t r, g, b;

  memset(cell, 0, sizeof(*cell));
  cell->width = 1;

  if ((line = vt_term_get_line(vterm->term, row))) {
    int char_index = vt_convert_col_to_char_index(line, &col, col, 0);
    if (char_index < line->num_filled_chars) {
      vt_font_t font = vt_char_font(line->chars + char_index);
      vt_line_style_t line_style = vt_char_line_style(line->chars + char_index);
      ef_charset_t cs = vt_char_cs(line->chars + char_index);

      fg = vt_char_fg_color(line->chars + char_index);
      bg = vt_char_bg_color(line->chars + char_index);

      /* XXX */
      if (bg == VT_FG_COLOR && fg == VT_BG_COLOR) {
        vt_color_t tmp = fg;
        fg = bg;
        bg = tmp;
        cell->attrs.reverse = 1;
      }

      if (font & FONT_BOLD) {
        cell->attrs.bold = 1;
      }

      if (font & FONT_ITALIC) {
        cell->attrs.italic = 1;
      }

      if (line_style & LS_UNDERLINE) {
        cell->attrs.underline = (line_style & LS_UNDERLINE);
      }

      if (line_style == LS_CROSSED_OUT) {
        cell->attrs.strike = 1;
      }

      if (vt_char_is_blinking(line->chars + char_index)) {
        cell->attrs.blink = 1;
      }

      if (line->size_attr & DOUBLE_WIDTH) {
        cell->attrs.dwl = 1;
      }

      if (line->size_attr >= DOUBLE_HEIGHT_TOP) {
        cell->attrs.dhl = (line->size_attr & DOUBLE_HEIGHT_TOP) ? 1 : 2;
      }

      cell->width = vt_char_cols(line->chars + char_index);

      if (cell->width == 2 && col == 1) {
        cell->chars[0] = (u_int32_t)-1;
        cell->width = 1;
      } else {
        cell->chars[0] = vt_char_code(line->chars + char_index);

        if (cs != US_ASCII && !IS_ISO10646_UCS4(cs)) {
          ef_char_t ch;

          ch.size = CS_SIZE(cs);
          ef_int_to_bytes(ch.ch, ch.size, cell->chars[0]);
          ch.cs = cs;
          ch.property = 0;
          ef_map_to_ucs4(&ch, &ch);
          cell->chars[0] = ef_bytes_to_int(ch.ch, 4);
        }
      }
    }
  }

  if (fg == VT_FG_COLOR) {
    cell->fg = vterm->default_fg;
  } else if (fg == VT_BG_COLOR) {
    cell->fg = vterm->default_bg;
  } else {
    vt_get_color_rgba(fg, &r, &g, &b, NULL);
    cell->fg.red = r;
    cell->fg.green = g;
    cell->fg.blue = b;
  }

  if (bg == VT_FG_COLOR) {
    cell->bg = vterm->default_fg;
  } else if (bg == VT_BG_COLOR) {
    cell->bg = vterm->default_bg;
  } else {
    vt_get_color_rgba(bg, &r, &g, &b, NULL);
    cell->bg.red = r;
    cell->bg.green = g;
    cell->bg.blue = b;
  }

  return 1;
}

int vterm_screen_is_eol(const VTermScreen *screen, VTermPos pos) { return 0; }

VTermValueType vterm_get_attr_type(VTermAttr attr) {
  switch(attr) {
  case VTERM_ATTR_BOLD:       return VTERM_VALUETYPE_BOOL;
  case VTERM_ATTR_UNDERLINE:  return VTERM_VALUETYPE_INT;
  case VTERM_ATTR_ITALIC:     return VTERM_VALUETYPE_BOOL;
  case VTERM_ATTR_BLINK:      return VTERM_VALUETYPE_BOOL;
  case VTERM_ATTR_REVERSE:    return VTERM_VALUETYPE_BOOL;
  case VTERM_ATTR_STRIKE:     return VTERM_VALUETYPE_BOOL;
  case VTERM_ATTR_FONT:       return VTERM_VALUETYPE_INT;
  case VTERM_ATTR_FOREGROUND: return VTERM_VALUETYPE_COLOR;
  case VTERM_ATTR_BACKGROUND: return VTERM_VALUETYPE_COLOR;
  }

  return 0;
}

VTermValueType vterm_get_prop_type(VTermProp prop) {
  switch(prop) {
  case VTERM_PROP_CURSORVISIBLE: return VTERM_VALUETYPE_BOOL;
  case VTERM_PROP_CURSORBLINK:   return VTERM_VALUETYPE_BOOL;
  case VTERM_PROP_ALTSCREEN:     return VTERM_VALUETYPE_BOOL;
  case VTERM_PROP_TITLE:         return VTERM_VALUETYPE_STRING;
  case VTERM_PROP_ICONNAME:      return VTERM_VALUETYPE_STRING;
  case VTERM_PROP_REVERSE:       return VTERM_VALUETYPE_BOOL;
  case VTERM_PROP_CURSORSHAPE:   return VTERM_VALUETYPE_INT;
  case VTERM_PROP_MOUSE:         return VTERM_VALUETYPE_INT;
  }

  return 0;
}

void vterm_scroll_rect(VTermRect rect,
                       int downward,
                       int rightward,
                       int (*moverect)(VTermRect src, VTermRect dest, void *user),
                       int (*eraserect)(VTermRect rect, int selective, void *user),
                       void *user) {}

void vterm_copy_cells(VTermRect dest,
                      VTermRect src,
                      void (*copycell)(VTermPos dest, VTermPos src, void *user),
                      void *user) {
  /* XXX */
}
