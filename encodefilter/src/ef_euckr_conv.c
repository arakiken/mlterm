/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euckr_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_ko_kr_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch, int is_uhc) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_ko_kr(&c, ch)) {
      *ch = c;
    }
  }

  if (is_uhc) {
    if (ch->cs == ISO10646_UCS4_1) {
      return;
    }

    if (ch->cs == JOHAB) {
      if (!ef_map_johab_to_uhc(&c, ch)) {
        return;
      }

      *ch = c;
    }

    if (ef_map_ksc5601_1987_to_uhc(&c, ch)) {
      *ch = c;
    }
  } else {
    ef_iso2022_remap_unsupported_charset(ch);
  }
}

static size_t convert_to_euckr_intern(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                      ef_parser_t *parser, int is_uhc) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, is_uhc);

    if (ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = *ch.ch;

      filled_size++;
    } else if ((!is_uhc) && ch.cs == KSC5601_1987) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);

      filled_size += 2;
    } else if (is_uhc && ch.cs == UHC) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];
      *(dst++) = ch.ch[1];

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
    }
  }

  return filled_size;
}

static size_t convert_to_euckr(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  return convert_to_euckr_intern(conv, dst, dst_size, parser, 0);
}

static size_t convert_to_uhc(ef_conv_t *conv, u_char *dst, size_t dst_size, ef_parser_t *parser) {
  return convert_to_euckr_intern(conv, dst, dst_size, parser, 1);
}

static void euckr_conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = KSC5601_1987;
  iso2022_conv->g2 = UNKNOWN_CS;
  iso2022_conv->g3 = UNKNOWN_CS;
}

static void uhc_conv_init(ef_conv_t *conv) {}

static void conv_destroy(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_euckr_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  euckr_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_euckr;
  iso2022_conv->conv.init = euckr_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}

ef_conv_t *ef_uhc_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_uhc;
  conv->init = uhc_conv_init;
  conv->destroy = conv_destroy;
  conv->illegal_char = NULL;

  return conv;
}
