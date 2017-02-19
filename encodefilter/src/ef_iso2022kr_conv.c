/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_iso2022kr_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_ko_kr_map.h"

typedef struct ef_iso2022kr_conv {
  ef_iso2022_conv_t iso2022_conv;
  int is_designated;

} ef_iso2022kr_conv_t;

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_ko_kr(&c, ch)) {
      *ch = c;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);
}

static size_t convert_to_iso2022kr(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                   ef_parser_t *parser) {
  ef_iso2022kr_conv_t *iso2022kr_conv;
  size_t filled_size;
  ef_char_t ch;

  iso2022kr_conv = (ef_iso2022kr_conv_t*)conv;

  filled_size = 0;

  if (!iso2022kr_conv->is_designated) {
    if (dst_size < 4) {
      return 0;
    }

    *(dst++) = ESC;
    *(dst++) = MB_CS;
    *(dst++) = CS94_TO_G1;
    *(dst++) = CS94MB_FT(KSC5601_1987);

    filled_size += 4;

    iso2022kr_conv->is_designated = 1;
  }

  while (ef_parser_next_char(parser, &ch)) {
    int count;

    remap_unsupported_charset(&ch);

    if (ch.cs == *iso2022kr_conv->iso2022_conv.gl) {
      if (filled_size + ch.size > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }
    } else {
      iso2022kr_conv->iso2022_conv.g0 = ch.cs;

      if (ch.cs == KSC5601_1987) {
        if (filled_size + ch.size >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = LS1;

        filled_size++;

        iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g1;
      } else if (ch.cs == US_ASCII) {
        if (filled_size + ch.size >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = LS0;

        filled_size++;

        iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g0;
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
  ef_iso2022kr_conv_t *iso2022kr_conv;

  iso2022kr_conv = (ef_iso2022kr_conv_t*)conv;

  iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g0;
  iso2022kr_conv->iso2022_conv.gr = NULL;
  iso2022kr_conv->iso2022_conv.g0 = US_ASCII;
  iso2022kr_conv->iso2022_conv.g1 = UNKNOWN_CS;
  iso2022kr_conv->iso2022_conv.g2 = UNKNOWN_CS;
  iso2022kr_conv->iso2022_conv.g3 = UNKNOWN_CS;

  iso2022kr_conv->is_designated = 0;
}

static void conv_delete(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_iso2022kr_conv_new(void) {
  ef_iso2022kr_conv_t *iso2022kr_conv;

  if ((iso2022kr_conv = malloc(sizeof(ef_iso2022kr_conv_t))) == NULL) {
    return NULL;
  }

  conv_init((ef_conv_t*)iso2022kr_conv);

  iso2022kr_conv->iso2022_conv.conv.convert = convert_to_iso2022kr;
  iso2022kr_conv->iso2022_conv.conv.init = conv_init;
  iso2022kr_conv->iso2022_conv.conv.delete = conv_delete;
  iso2022kr_conv->iso2022_conv.conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022kr_conv;
}
