/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef USE_BRLAPI

#include "ui_brltty.h"

#include <string.h>
#include <fcntl.h>
#include <brlapi.h>
#include <brltty/config.h> /* SIZEOF_WCHAR_T_STR */
#include <pobl/bl_def.h> /* WORDS_BIGENDIAN */
#include <pobl/bl_types.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_privilege.h> /* bl_priv_change_e(u|g)id */
#include <pobl/bl_str.h>
#include <pobl/bl_unistd.h>
#include <mef/ef_utf8_conv.h>
#include <mef/ef_utf16_conv.h>
#include <mef/ef_utf32_conv.h>

#include "vt_str_parser.h"
#include "vt_term.h"
#include "ui_event_source.h"

/* --- static variables --- */

static vt_term_t *focus_term;
static int brltty_fd = -1;
static ef_parser_t *parser;
static ef_conv_t *conv;
static ef_conv_t *wconv;
static u_int display_cols;
static u_int display_rows;
static int viewport_row;
static int viewport_col;

/* --- static functions --- */

static void write_line_to_display(vt_line_t *line, int cursor /* The first position is 1 not 0 */) {
  u_char *buf;

  if (cursor > 0) {
    if (viewport_col >= cursor) {
      cursor = BRLAPI_CURSOR_OFF;
    } else {
      cursor -= viewport_col;
    }
  }

  if (viewport_col >= vt_line_get_num_filled_chars_except_sp(line)) {
    brlapi_writeText(cursor, "");
    return;
  }

  (*parser->init)(parser);
  vt_str_parser_set_str(parser, line->chars + viewport_col,
                        vt_line_get_num_filled_chars_except_sp(line) - viewport_col);
  (*wconv->init)(conv);

  if ((buf = alloca(display_cols * sizeof(wchar_t) + 4))) {
    ssize_t len;

    if ((len = (*wconv->convert)(wconv, buf, display_cols * sizeof(wchar_t), parser)) > 0) {
      memset(buf + len, 0, 4);
      brlapi_writeWText(cursor, buf);
    }
  }
}

static void speak(const u_char *msg, size_t len) {
  const char *path;

  if ((path = getenv("BRLTTY_SPEECH_INPUT"))) {
    int fd;

    if ((fd = open(path, O_WRONLY|O_NONBLOCK, 0200)) != -1) {
#ifdef DEBUG
      char *buf = alloca(len + 1);
      memcpy(buf, msg, len);
      buf[len] = '\0';
      bl_debug_printf("SPEECH: %s %x\n", buf, *buf);
#endif
      write(fd, msg, len);
      close(fd);
    }
  }
}

static void speak_line(vt_line_t *line, int full) {
  u_char *buf;
  int beg;
  u_int cols;

  cols = vt_line_get_num_filled_chars_except_sp(line);

  if (full) {
    if (cols == 0) {
      return;
    }

    beg = 0;
  } else {
    if ((beg = viewport_col) >= cols) {
      return;
    }

    if ((cols -= viewport_col) >= display_cols) {
      cols = display_cols;
    }
  }

  (*parser->init)(parser);
  vt_str_parser_set_str(parser, line->chars + beg, cols);
  (*conv->init)(conv);

  if ((buf = alloca(display_cols * UTF_MAX_SIZE))) {
    ssize_t len;

    if ((len = (*conv->convert)(conv, buf, display_cols * UTF_MAX_SIZE, parser)) > 0) {
      speak(buf, len);
    }
  }
}

static void read_key(void) {
  brlapi_keyCode_t key;

  if (brltty_fd < 0 || focus_term == NULL) {
    return;
  }

  if (brlapi_readKey(1, &key) > 0) {
    brlapi_expandedKeyCode_t ekey;
#ifdef DEBUG
    brlapi_describedKeyCode_t dkey;
#endif

    brlapi_expandKeyCode(key, &ekey);
#ifdef DEBUG
    bl_debug_printf("Keycode %x: type %u, command %u, argument %u, flags %u\n",
                    key, ekey.type, ekey.command, ekey.argument, ekey.flags);
    brlapi_describeKeyCode(key, &dkey);
    bl_debug_printf("type %s, command %s, argument %u, flags %u\n",
                    dkey.type, dkey.command, dkey.argument, dkey.flags);
#endif

    if (ekey.command) {
      vt_line_t *line;

      if (ekey.command == BRLAPI_KEY_CMD_TOP || ekey.command == BRLAPI_KEY_CMD_TOP_LEFT) {
        viewport_row = 0;
      } else if (ekey.command == BRLAPI_KEY_CMD_BOT || ekey.command == BRLAPI_KEY_CMD_BOT_LEFT) {
        viewport_row = vt_term_get_rows(focus_term) - 1;
      } else if (ekey.command == BRLAPI_KEY_CMD_LNUP) {
        viewport_row--;
      } else if (ekey.command == BRLAPI_KEY_CMD_LNDN) {
        viewport_row++;
      } else if (ekey.command == BRLAPI_KEY_CMD_CHRRT) {
        viewport_col++;
      } else if (ekey.command == BRLAPI_KEY_CMD_CHRLT) {
        viewport_col--;
      } else if (ekey.command == BRLAPI_KEY_CMD_FWINRT) {
        viewport_col ++;
      } else if (ekey.command == BRLAPI_KEY_CMD_FWINLT) {
        viewport_col --;
      } else if (ekey.command == BRLAPI_KEY_CMD_FWINRTSKIP) {
        viewport_col += display_cols;
      } else if (ekey.command == BRLAPI_KEY_CMD_FWINLTSKIP) {
        viewport_col -= display_cols;
      } else if (ekey.command == BRLAPI_KEY_CMD_HOME) {
        viewport_row = vt_term_cursor_row(focus_term);
        viewport_col = 0;
      } else if (ekey.command == BRLAPI_KEY_CMD_SAY_LINE ||
                 ekey.command == BRLAPI_KEY_CMD_SPEAK_CURR_LINE) {
        if ((line = vt_term_get_line(focus_term, viewport_row))) {
          speak_line(line, 1);
        }

        return;
      } else if (ekey.command == BRLAPI_KEY_CMD_SAY_ABOVE) {
        int row;

        for (row = 0; row <= viewport_row; row++) {
          if ((line = vt_term_get_line(focus_term, row))) {
            speak_line(line, 1);
          }
        }

        return;
      } else if (ekey.command == BRLAPI_KEY_CMD_SAY_BELOW) {
        int row;

        for (row = viewport_row; row < vt_term_get_rows(focus_term); row++) {
          if ((line = vt_term_get_line(focus_term, row))) {
            speak_line(line, 1);
          }
        }

        return;
      } else {
        return;
      }

      if (viewport_col < 0) {
        viewport_col = 0;
      } else if (viewport_col >= vt_term_get_cols(focus_term)) {
        viewport_col = vt_term_get_cols(focus_term) - 1;
      }

      if (viewport_row < 0) {
        viewport_row = 0;
      } else if (viewport_row >= vt_term_get_rows(focus_term)) {
        viewport_row = vt_term_get_rows(focus_term) - 1;
      }

      if ((line = vt_term_get_line(focus_term, viewport_row))) {
        vt_line_set_modified_all(line);
        write_line_to_display(line,
                              viewport_row == vt_term_cursor_row(focus_term) ?
                                vt_term_cursor_char_index(focus_term) + 1 : BRLAPI_CURSOR_OFF);
        speak_line(line, 0);
      }
    } else if (ekey.argument < 0xff) {
      u_char c = ekey.argument;
      vt_term_write(focus_term, &c, 1);
    }
  }
}

static void viewport_home(void) {
  viewport_row = vt_term_cursor_row(focus_term);
  if (display_cols <= vt_term_cursor_char_index(focus_term)) {
    viewport_col = vt_term_cursor_char_index(focus_term) - display_cols + 1;
  } else {
    viewport_col = 0;
  }
}

/* --- global functions --- */

int ui_brltty_init(void) {
  char name[BRLAPI_MAXNAMELENGTH+1];
  brlapi_connectionSettings_t set = { NULL, NULL };

  bl_priv_restore_euid();
  bl_priv_restore_egid();

  brltty_fd = brlapi_openConnection(&set, &set);

  bl_priv_change_euid(bl_getuid());
  bl_priv_change_egid(bl_getgid());

  if (brltty_fd < 0) {
    bl_warn_printf("Failed to connect to brltty (%s)\n", brlapi_strerror(&brlapi_error));

    return -1;
  }

  if (brlapi_getDriverName(name, sizeof(name)) < 0) {
#ifdef DEBUG
    bl_debug_printf("brlapi_getDriverName failed (%s)\n", brlapi_strerror(&brlapi_error));
#endif
  }
#ifdef DEBUG
  else {
    bl_debug_printf("Driver name: %s\n", name);
  }
#endif

  if (brlapi_getDisplaySize(&display_cols, &display_rows) < 0) {
    bl_warn_printf("Illegal brltty display size (%s)\n", brlapi_strerror(&brlapi_error));
    goto error;
  } else {
#ifdef DEBUG
    bl_debug_printf("Braille display line %d column %d\n",
                    display_rows, display_cols);
#endif

    if (display_cols == 0 || display_rows == 0) {
      bl_warn_printf("Illegal brltty display size (%s)\n", brlapi_strerror(&brlapi_error));
      goto error;
    }
  }

  /* Try entering raw mode, immediately go out from raw mode */
  if (brlapi_enterRawMode(name) < 0) {
#ifdef DEBUG
    bl_debug_printf("brlapi_enterRawMode failed (%s)\n", brlapi_strerror(&brlapi_error));
#endif
  } else {
    brlapi_leaveRawMode();
  }

  if (brlapi_enterTtyMode(0 /* BRLAPI_TTY_DEFAULT */, NULL) < 0) {
    bl_warn_printf("Failed to enter tty mode (%s)\n", brlapi_strerror(&brlapi_error));
    goto error;
  }

  if (!parser) {
    parser = vt_str_parser_new();
  }

  if (!conv) {
    conv = ef_utf8_conv_new();
  }

  if (!wconv) {
#ifdef SIZEOF_WCHAR_T_STR
    if (strcmp(SIZEOF_WCHAR_T_STR, "4") == 0) {
#ifdef WORDS_BIGENDIAN
      wconv = ef_utf32_conv_new();
#else
      wconv = ef_utf32le_conv_new();
#endif
    } else
#endif
    {
#ifdef WORDS_BIGENDIAN
      wconv = ef_utf16_conv_new();
#else
      wconv = ef_utf16le_conv_new();
#endif
    }
  }

#ifdef DEBUG
  bl_debug_printf("Connect to brltty successfully.\n");
#endif

  ui_event_source_add_fd(brltty_fd, read_key);

  return brltty_fd;

error:
  brlapi_closeConnection();
  brltty_fd = -1;

  return -1;
}

int ui_brltty_final(void) {
  if (brltty_fd >= 0) {
    ui_event_source_remove_fd(brltty_fd);

    brlapi_leaveTtyMode();
    brlapi_closeConnection();
    (*parser->delete)(parser);
    (*conv->delete)(conv);
    (*wconv->delete)(wconv);
  }

  return brltty_fd;
}

void ui_brltty_focus(vt_term_t *term) {
  if (focus_term == NULL) {
    speak("m l term started", 16);
  }

  focus_term = term;
}

void ui_brltty_write(void) {
  vt_line_t *line;

  if (brltty_fd < 0 || focus_term == NULL) {
    return;
  }

  line = vt_term_get_cursor_line(focus_term);

  if (!vt_line_is_modified(line) && viewport_row == vt_term_cursor_row(focus_term)) {
    return;
  }

  viewport_home();
  write_line_to_display(line, vt_term_cursor_char_index(focus_term) + 1);
}

void ui_brltty_speak_key(KeySym ksym, const u_char *kstr, size_t len) {
  const char *path;

  if ((path = getenv("BRLTTY_SPEECH_INPUT"))) {
    int fd;

    if ((fd = open(path, O_WRONLY|O_NONBLOCK, 0200)) != -1) {
      char *str;

      if (ksym == XK_BackSpace) {
        str = "backspace";
      } else if (ksym == XK_Delete) {
        str = "delete";
      } else if (ksym == ' ') {
        str = "space";
      } else if (ksym == XK_Return) {
        str = "return";
      } else if (ksym == XK_Tab) {
        str = "tab";
      } else if (ksym == XK_Insert) {
        str = "insert";
      } else if (ksym == XK_Home) {
        str = "home";
      } else if (ksym == XK_End) {
        str = "end";
      } else if (ksym == XK_F1) {
        str = "f1";
      } else if (ksym == XK_F2) {
        str = "f2";
      } else if (ksym == XK_F3) {
        str = "f3";
      } else if (ksym == XK_F4) {
        str = "f4";
      } else if (ksym == XK_Prior) {
        str = "page up";
      } else if (ksym == XK_Next) {
        str = "page down";
      } else if (ksym == XK_Up) {
        str = "up";
      } else if (ksym == XK_Down) {
        str = "down";
      } else if (ksym == XK_Right) {
        str = "right";
      } else if (ksym == XK_Left) {
        str = "left";
      } else {
        str = NULL;
      }

      if (str) {
        len = strlen(str);
      } else {
        str = kstr;
      }

      write(fd, str, len);
      close(fd);
    }
  }
}

#endif
