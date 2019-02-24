/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_iso2022cn_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_intern.h"
#include "ef_iso2022_conv.h"
#include "ef_ucs4_map.h"
#include "ef_zh_cn_map.h"
#include "ef_zh_tw_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_zh_cn(&c, ch)) {
      *ch = c;
    } else if (ef_map_ucs4_to_zh_tw(&c, ch)) {
      *ch = c;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);
}

static size_t convert_to_iso2022cn(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                   ef_parser_t *parser) {
  ef_iso2022_conv_t *iso2022_conv;
  size_t filled_size;
  ef_char_t ch;
  int count;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch);

    if (ch.cs == *iso2022_conv->gl) {
      if (filled_size + ch.size > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      if (ch.cs == US_ASCII && ch.ch[0] == '\n') {
        /* reset */

        iso2022_conv->g1 = UNKNOWN_CS;
        iso2022_conv->g2 = UNKNOWN_CS;
      }
    } else if (ch.cs == CNS11643_1992_2) {
      /* single shifted */

      if (iso2022_conv->g2 != CNS11643_1992_2) {
        if (filled_size + ch.size + 6 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ESC;
        *(dst++) = MB_CS;
        *(dst++) = CS94_TO_G2;
        *(dst++) = CS94MB_FT(CNS11643_1992_2);
        *(dst++) = ESC;
        *(dst++) = SS2_7;

        filled_size += 6;

        iso2022_conv->g2 = CNS11643_1992_2;
      } else {
        if (filled_size + ch.size + 2 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ESC;
        *(dst++) = SS2_7;

        filled_size += 2;
      }
    } else if (ch.cs == CNS11643_1992_1 || ch.cs == GB2312_80) {
      if (iso2022_conv->g1 != ch.cs) {
        if (filled_size + ch.size + 5 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = ESC;
        *(dst++) = MB_CS;
        *(dst++) = CS94_TO_G1;
        *(dst++) = CS94MB_FT(ch.cs);
        *(dst++) = LS1;

        filled_size += 5;

        iso2022_conv->g1 = ch.cs;
      } else {
        if (filled_size + ch.size + 1 > dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }

        *(dst++) = LS1;

        filled_size++;
      }

      iso2022_conv->gl = &iso2022_conv->g1;
    } else if (ch.cs == US_ASCII) {
      if (filled_size + ch.size + 1 > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = LS0;

      filled_size++;

      if (ch.ch[0] == '\n') {
        /* reset */

        iso2022_conv->g1 = UNKNOWN_CS;
        iso2022_conv->g2 = UNKNOWN_CS;
      }

      iso2022_conv->gl = &iso2022_conv->g0;
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

    for (count = 0; count < ch.size; count++) {
      *(dst++) = ch.ch[count];
    }

    filled_size += ch.size;
  }

  return filled_size;
}

static void conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = NULL;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = UNKNOWN_CS;
  iso2022_conv->g2 = UNKNOWN_CS;
}

static void conv_destroy(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_iso2022cn_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_iso2022cn;
  iso2022_conv->conv.init = conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}
