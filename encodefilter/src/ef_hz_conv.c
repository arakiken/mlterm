/*
 *	$Id$
 */

#include "ef_hz_conv.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_zh_cn_map.h"

typedef struct ef_hz_conv {
  ef_conv_t conv;
  ef_charset_t cur_cs;

} ef_hz_conv_t;

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (!ef_map_ucs4_to_zh_cn(&c, ch)) {
      return;
    }

    *ch = c;
  }

  if (ch->cs == GBK) {
    if (ef_map_gbk_to_gb2312_80(&c, ch)) {
      *ch = c;
    }
  }
}

static size_t convert_to_hz(ef_conv_t *conv, u_char *dst, size_t dst_size, ef_parser_t *parser) {
  ef_hz_conv_t *hz_conv;
  size_t filled_size;
  ef_char_t ch;
  int count;

  hz_conv = (ef_hz_conv_t *)conv;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch);

    if (ch.ch[0] == '~' && ch.cs == US_ASCII) {
      ch.ch[1] = '~';
      ch.size = 2;
    }

    if (ch.cs == hz_conv->cur_cs) {
      if (filled_size + ch.size - 1 > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }
    } else {
      hz_conv->cur_cs = ch.cs;

      if (ch.cs == GB2312_80) {
        if (filled_size + ch.size + 1 >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = '~';
        *(dst++) = '{';

        filled_size += 2;
      } else if (ch.cs == US_ASCII) {
        if (filled_size + ch.size + 1 >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = '~';
        *(dst++) = '}';

        filled_size += 2;
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

        continue;
      } else {
        continue;
      }
    }

    for (count = 0; count < ch.size; count++) {
      *(dst++) = ch.ch[count];
    }

    filled_size += ch.size;
  }

  return filled_size;
}

static void conv_init(ef_conv_t *conv) {
  ef_hz_conv_t *hz_conv;

  hz_conv = (ef_hz_conv_t *)conv;

  hz_conv->cur_cs = US_ASCII;
}

static void conv_delete(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_hz_conv_new(void) {
  ef_hz_conv_t *hz_conv;

  if ((hz_conv = malloc(sizeof(ef_hz_conv_t))) == NULL) {
    return NULL;
  }

  hz_conv->conv.convert = convert_to_hz;
  hz_conv->conv.init = conv_init;
  hz_conv->conv.delete = conv_delete;
  hz_conv->conv.illegal_char = NULL;

  hz_conv->cur_cs = US_ASCII;

  return (ef_conv_t *)hz_conv;
}
