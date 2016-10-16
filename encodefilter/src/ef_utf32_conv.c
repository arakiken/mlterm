/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_utf32_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>

#include "ef_ucs4_map.h"

typedef struct ef_utf32_conv {
  ef_conv_t conv;
  int is_bof; /* beginning of file */

} ef_utf32_conv_t;

/* --- static functions --- */

static size_t convert_to_utf32(ef_conv_t* conv, u_char* dst, size_t dst_size,
                               ef_parser_t* parser) {
  ef_utf32_conv_t* utf32_conv;
  size_t filled_size;
  ef_char_t ch;

  utf32_conv = (ef_utf32_conv_t*)conv;

  filled_size = 0;

  if (utf32_conv->is_bof) {
    if (dst_size < 4) {
      return 0;
    }

    /*
     * mark big endian
     */

    *(dst++) = 0x0;
    *(dst++) = 0x0;
    *(dst++) = 0xfe;
    *(dst++) = 0xff;

    filled_size += 4;

    utf32_conv->is_bof = 0;
  }

  while (filled_size + 4 <= dst_size) {
    if (!ef_parser_next_char(parser, &ch)) {
      return filled_size;
    }

    if (ch.cs == US_ASCII) {
      dst[0] = 0x0;
      dst[1] = 0x0;
      dst[2] = 0x0;
      dst[3] = ch.ch[0];
    }
#if 0
    else if (ch.cs == ISO10646_UCS2_1) {
      dst[0] = 0x0;
      dst[1] = 0x0;
      dst[2] = ch.ch[0];
      dst[3] = ch.ch[1];
    }
#endif
    else if (ch.cs == ISO10646_UCS4_1) {
      dst[0] = ch.ch[0];
      dst[1] = ch.ch[1];
      dst[2] = ch.ch[2];
      dst[3] = ch.ch[3];
    } else {
      ef_char_t ucs4_ch;

      if (ef_map_to_ucs4(&ucs4_ch, &ch)) {
        memcpy(dst, ucs4_ch.ch, 4);
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

    dst += 4;
    filled_size += 4;
  }

  return filled_size;
}

static void conv_init(ef_conv_t* conv) {
  ef_utf32_conv_t* utf32_conv;

  utf32_conv = (ef_utf32_conv_t*)conv;

  utf32_conv->is_bof = 1;
}

static void conv_delete(ef_conv_t* conv) { free(conv); }

/* --- global functions --- */

ef_conv_t* ef_utf32_conv_new(void) {
  ef_utf32_conv_t* utf32_conv;

  if ((utf32_conv = malloc(sizeof(ef_utf32_conv_t))) == NULL) {
    return NULL;
  }

  utf32_conv->conv.convert = convert_to_utf32;
  utf32_conv->conv.init = conv_init;
  utf32_conv->conv.delete = conv_delete;
  utf32_conv->conv.illegal_char = NULL;

  utf32_conv->is_bof = 1;

  return (ef_conv_t*)utf32_conv;
}
