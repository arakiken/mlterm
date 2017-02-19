/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euctw_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_zh_tw_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_zh_tw(&c, ch)) {
      *ch = c;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);
}

static size_t convert_to_euctw(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch);

    if (ch.cs == CNS11643_1992_1) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);

      filled_size += 2;
    } else if (ch.cs == CNS11643_1992_2) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa2;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == CNS11643_1992_3) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa3;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == CNS11643_1992_4) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa4;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == CNS11643_1992_5) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa5;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == CNS11643_1992_6) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa6;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == CNS11643_1992_7) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = 0xa7;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);
    } else if (ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = *ch.ch;

      filled_size++;
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

static void conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = CNS11643_1992_EUCTW_G2;
  iso2022_conv->g2 = UNKNOWN_CS;
  iso2022_conv->g3 = UNKNOWN_CS;
}

static void conv_delete(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_euctw_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_euctw;
  iso2022_conv->conv.init = conv_init;
  iso2022_conv->conv.delete = conv_delete;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}
