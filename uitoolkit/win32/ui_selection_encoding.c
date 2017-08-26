/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_selection_encoding.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_locale.h>
#include <vt_char_encoding.h>
#include <mef/ef_utf16_conv.h>
#include <mef/ef_utf16_parser.h>

/* --- static varaiables --- */

static ef_parser_t *utf_parser; /* leaked */
static ef_parser_t *parser; /* leaked */
static ef_conv_t *utf_conv; /* leaked */
static ef_conv_t *conv; /* leaked */

/* --- static functions --- */

/* See utf8_illegal_char() in vt_char_encoding.c */
static size_t utf16le_illegal_char(ef_conv_t *conv, u_char *dst, size_t dst_size, int *is_full,
                                   ef_char_t *ch) {
  *is_full = 0;

  if (ch->cs == DEC_SPECIAL) {
    u_int16_t utf16;

    if (dst_size < 2) {
      *is_full = 1;
    } else if ((utf16 = vt_convert_decsp_to_ucs(ef_char_to_int(ch)))) {
      memcpy(dst, utf16, 2); /* little endian */
      return 2;
    }
  }

  return 0;
}

/* --- global functions --- */

ef_parser_t *ui_get_selection_parser(int utf) {
  if (utf) {
    if (!utf_parser) {
      if (!(utf_parser = ef_utf16le_parser_new())) {
        return NULL;
      }
    }

    return utf_parser;
  } else {
    if (!parser) {
      if (!(parser = vt_char_encoding_parser_new(vt_get_char_encoding(bl_get_codeset_win32())))) {
        return NULL;
      }
    }

    return parser;
  }
}

ef_conv_t *ui_get_selection_conv(int utf) {
  if (utf) {
    if (!utf_conv) {
      if (!(utf_conv = ef_utf16le_conv_new())) {
        return NULL;
      }
      utf_conv->illegal_char = utf16le_illegal_char;
    }

    return utf_conv;
  } else {
    if (!conv) {
      if (!(conv = vt_char_encoding_conv_new(vt_get_char_encoding(bl_get_codeset_win32())))) {
        return NULL;
      }
    }

    return conv;
  }
}
