/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_utf16_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"

typedef struct ef_utf16_conv {
  ef_conv_t conv;
  int is_bof; /* beginning of file */
  int use_bom;

} ef_utf16_conv_t;

/* --- static functions --- */

static size_t convert_to_utf16(ef_conv_t* conv, u_char* dst, size_t dst_size,
                               ef_parser_t* parser) {
  ef_utf16_conv_t* utf16_conv;
  size_t filled_size;
  ef_char_t ch;

  utf16_conv = (ef_utf16_conv_t*)conv;

  filled_size = 0;

  if (utf16_conv->use_bom && utf16_conv->is_bof) {
    if (dst_size < 2) {
      return 0;
    }

    /*
     * mark big endian
     */

    *(dst++) = 0xfe;
    *(dst++) = 0xff;

    filled_size += 2;

    utf16_conv->is_bof = 0;
  }

  while (1) {
    if (!ef_parser_next_char(parser, &ch)) {
      return filled_size;
    }

#if 0
    if (ch.cs == ISO10646_UCS2_1) {
      if (filled_size + 2 > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      (*dst++) = ch.ch[0];
      (*dst++) = ch.ch[1];

      filled_size += 2;
    } else
#endif
        if (ch.cs == US_ASCII) {
      if (filled_size + 2 > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      (*dst++) = '\0';
      (*dst++) = ch.ch[0];

      filled_size += 2;
    } else {
      if (ch.cs != ISO10646_UCS4_1) {
        ef_char_t ucs4_ch;

        if (ef_map_to_ucs4(&ucs4_ch, &ch)) {
          ch = ucs4_ch;
        }
      }

      if (ch.cs != ISO10646_UCS4_1 || ch.ch[0] > 0x0 || ch.ch[1] > 0x10) {
        if (conv->illegal_char) {
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
      } else if (ch.ch[1] == 0x0) {
        /* BMP */

        if (filled_size + 2 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        (*dst++) = ch.ch[2];
        (*dst++) = ch.ch[3];

        filled_size += 2;
      } else /* if( 0x1 <= ch.ch[1] && ch.ch[1] <= 0x10) */
      {
        /* surrogate pair */

        u_int32_t linear;
        u_char c;

        if (filled_size + 4 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        linear = ef_bytes_to_int(ch.ch, 4) - 0x10000;

        c = (u_char)(linear / (0x100 * 0x400));
        linear -= (c * 0x100 * 0x400);
        (*dst++) = c + 0xd8;

        c = (u_char)(linear / 0x400);
        linear -= (c * 0x400);
        (*dst++) = c;

        c = (u_char)(linear / 0x100);
        linear -= (c * 0x100);
        (*dst++) = c + 0xdc;

        (*dst++) = (u_char)linear;

        filled_size += 4;
      }
    }
  }
}

static size_t convert_to_utf16le(ef_conv_t* conv, u_char* dst, size_t dst_size,
                                 ef_parser_t* parser) {
  size_t size;
  int count;

  if ((size = convert_to_utf16(conv, dst, dst_size, parser)) == 0) {
    return 0;
  }

  for (count = 0; count < size - 1; count += 2) {
    u_char c;

    c = dst[count];
    dst[count] = dst[count + 1];
    dst[count + 1] = c;
  }

  return size;
}

static void conv_init(ef_conv_t* conv) {
  ef_utf16_conv_t* utf16_conv;

  utf16_conv = (ef_utf16_conv_t*)conv;

  utf16_conv->is_bof = 1;
}

static void conv_delete(ef_conv_t* conv) { free(conv); }

/* --- global functions --- */

ef_conv_t* ef_utf16_conv_new(void) {
  ef_utf16_conv_t* utf16_conv;

  if ((utf16_conv = malloc(sizeof(ef_utf16_conv_t))) == NULL) {
    return NULL;
  }

  utf16_conv->conv.convert = convert_to_utf16;
  utf16_conv->conv.init = conv_init;
  utf16_conv->conv.delete = conv_delete;
  utf16_conv->conv.illegal_char = NULL;

  utf16_conv->is_bof = 1;
  utf16_conv->use_bom = 0;

  return (ef_conv_t*)utf16_conv;
}

ef_conv_t* ef_utf16le_conv_new(void) {
  ef_utf16_conv_t* utf16_conv;

  if ((utf16_conv = malloc(sizeof(ef_utf16_conv_t))) == NULL) {
    return NULL;
  }

  utf16_conv->conv.convert = convert_to_utf16le;
  utf16_conv->conv.init = conv_init;
  utf16_conv->conv.delete = conv_delete;
  utf16_conv->conv.illegal_char = NULL;

  utf16_conv->is_bof = 1;
  utf16_conv->use_bom = 0;

  return (ef_conv_t*)utf16_conv;
}

int ef_utf16_conv_use_bom(ef_conv_t* conv) {
  ef_utf16_conv_t* utf16_conv;

  utf16_conv = (ef_utf16_conv_t*)conv;

  utf16_conv->use_bom = 1;

  return 1;
}
