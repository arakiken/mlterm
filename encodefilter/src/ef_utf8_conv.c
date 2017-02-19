/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_utf8_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"

/* --- static functions --- */

static int remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs != US_ASCII && ch->cs != ISO10646_UCS4_1 /* && ch->cs != ISO10646_UCS2_1 */) {
    if (ef_map_to_ucs4(&c, ch)) {
      *ch = c;
    } else {
      return 0;
    }
  }

  return 1;
}

static size_t convert_to_utf8(ef_conv_t *conv, u_char *dst, size_t dst_size,
                              ef_parser_t *parser) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    /*
     * utf8 encoding
     */
    if (remap_unsupported_charset(&ch)) {
      u_int32_t ucs_ch;

      ucs_ch = ef_char_to_int(&ch);

      /* ucs_ch is unsigned */
      if (/* 0x00 <= ucs_ch && */ ucs_ch <= 0x7f) {
        /* encoded to 8 bit */

        if (filled_size + 1 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ucs_ch;
        filled_size++;
      } else if (ucs_ch <= 0x07ff) {
        /* encoded to 16bit */

        if (filled_size + 2 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ((ucs_ch >> 6) & 0xff) | 0xc0;
        *(dst++) = (ucs_ch & 0x3f) | 0x80;
        filled_size += 2;
      } else if (ucs_ch <= 0xffff) {
        if (filled_size + 3 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ((ucs_ch >> 12) & 0x0f) | 0xe0;
        *(dst++) = ((ucs_ch >> 6) & 0x3f) | 0x80;
        *(dst++) = (ucs_ch & 0x3f) | 0x80;
        filled_size += 3;
      } else if (ucs_ch <= 0x1fffff) {
        if (filled_size + 4 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ((ucs_ch >> 18) & 0x07) | 0xf0;
        *(dst++) = ((ucs_ch >> 12) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 6) & 0x3f) | 0x80;
        *(dst++) = (ucs_ch & 0x3f) | 0x80;
        filled_size += 4;
      } else if (ucs_ch <= 0x03ffffff) {
        if (filled_size + 5 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ((ucs_ch >> 24) & 0x03) | 0xf8;
        *(dst++) = ((ucs_ch >> 18) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 12) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 6) & 0x3f) | 0x80;
        *(dst++) = (ucs_ch & 0x3f) | 0x80;
        filled_size += 5;
      } else if (ucs_ch <= 0x7fffffff) {
        if (filled_size + 6 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ((ucs_ch >> 30) & 0x01) | 0xfc;
        *(dst++) = ((ucs_ch >> 24) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 18) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 12) & 0x3f) | 0x80;
        *(dst++) = ((ucs_ch >> 6) & 0x3f) | 0x80;
        *(dst++) = (ucs_ch & 0x3f) | 0x80;
        filled_size += 6;
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " strange ucs4 character %x\n", ucs_ch);
#endif

        if (filled_size >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ' ';
        filled_size++;
      }
    } else if (conv->illegal_char) {
      size_t size;
      int is_full;

      size = (*conv->illegal_char)(conv, dst, dst_size - filled_size, &is_full, &ch);
      if (is_full) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      dst += size;
      filled_size += size;
    }
  }

  return filled_size;
}

static void conv_init(ef_conv_t *conv) {}

static void conv_delete(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_utf8_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_utf8;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}
