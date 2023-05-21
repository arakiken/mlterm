/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_copymode.h"

#include <pobl/bl_mem.h> /* alloca, calloc */
#include <pobl/bl_str.h> /* strdup */
#include <vt_char_encoding.h>
#include <vt_str_parser.h>
#include <vt_str.h>

/* --- static functions --- */

static void add_char_to_pattern(vt_char_t *ch, u_int code,
                                ef_charset_t cs, ef_property_t prop) {
  vt_char_init(ch);
  vt_char_set(ch, code, cs, ((prop & EF_FULLWIDTH) | IS_FULLWIDTH_CS(cs)) ? 1 : 0,
              (prop & EF_AWIDTH) ? 1 : 0, (prop & EF_COMBINING) ? 1 : 0,
              VT_BG_COLOR, VT_FG_COLOR, 0, 0, 0, 0, 0);

  /* XXX Use vt_char_combine() */
}


/* --- global functions --- */

ui_copymode_t *ui_copymode_new(int char_index, int row) {
  ui_copymode_t *copymode;

  if ((copymode = calloc(1, sizeof(ui_copymode_t))) == NULL) {
    return NULL;
  }

  copymode->cursor_char_index = char_index;
  copymode->cursor_row = row;

  return copymode;
}

void ui_copymode_destroy(ui_copymode_t *copymode) {
  vt_str_final(copymode->pattern, copymode->pattern_len);
  free(copymode);
}

void ui_copymode_pattern_start_edit(ui_copymode_t *copymode, int backward) {
  copymode->pattern_editing = backward ? 2 : 1;
  if (copymode->pattern_len == 0) {
    copymode->pattern_len = 1;
    add_char_to_pattern(copymode->pattern,
                        backward ? '?' : '/', US_ASCII, 0);
  }
}

void ui_copymode_pattern_cancel_edit(ui_copymode_t *copymode) {
  vt_str_final(copymode->pattern, copymode->pattern_len);
  copymode->pattern_len = 0;
  copymode->pattern_editing = 0;
}

void ui_copymode_pattern_add_str(ui_copymode_t *copymode, u_char *str, size_t len,
                                 ef_parser_t *parser) {
  if (parser) {
    ef_char_t ch;

    (*parser->init)(parser);
    (*parser->set_str)(parser, str, len);

    while ((*parser->next_char)(parser, &ch)) {
      if (copymode->pattern_len < MAX_COPYMODE_PATTERN_LEN) {
        add_char_to_pattern(&copymode->pattern[copymode->pattern_len++],
                            ef_char_to_int(&ch), ch.cs, ch.property);
      }
    }
  } else {
    size_t count;

    for (count = 0; count < len; count++) {
      if (copymode->pattern_len < MAX_COPYMODE_PATTERN_LEN) {
        add_char_to_pattern(&copymode->pattern[copymode->pattern_len++],
                            str[count], US_ASCII, 0);
      }
    }
  }
}

int ui_copymode_pattern_delete(ui_copymode_t *copymode) {
  vt_char_final(&copymode->pattern[--copymode->pattern_len]);
  if (copymode->pattern_len == 0) {
    copymode->pattern_editing = 0;

    return 0;
  }

  return 1;
}

u_char *ui_copymode_get_utf8_pattern(ui_copymode_t *copymode) {
  if (copymode->pattern_len > 1) {
    size_t len = (copymode->pattern_len - 1) * UTF_MAX_SIZE;
    u_char *buf;

    if ((buf = alloca(len + 1))) {
      ef_parser_t *parser = vt_str_parser_new();
      ef_conv_t *conv = vt_char_encoding_conv_new(VT_UTF8);

      (*parser->init)(parser);
      vt_str_parser_set_str(parser, copymode->pattern + 1, copymode->pattern_len - 1);
      (*conv->init)(conv);

      len = (*conv->convert)(conv, buf, len, parser);
      buf[len] = '\0';

      (*parser->destroy)(parser);
      (*conv->destroy)(conv);

      return strdup(buf);
    }
  }

  return NULL;
}
