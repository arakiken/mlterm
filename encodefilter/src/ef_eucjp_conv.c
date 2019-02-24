/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_eucjp_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_ja_jp_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch, ef_charset_t g1, ef_charset_t g3) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_ja_jp(&c, ch)) {
      *ch = c;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);

  /*
   * various gaiji chars => jis
   */
  if (ch->cs == JISC6226_1978_NEC_EXT) {
    if (!ef_map_jisx0208_nec_ext_to_jisx0208_1983(&c, ch) &&
        !ef_map_jisx0208_nec_ext_to_jisx0212_1990(&c, ch)) {
      return;
    }

    *ch = c;
  } else if (ch->cs == JISC6226_1978_NECIBM_EXT) {
    if (!ef_map_jisx0208_necibm_ext_to_jisx0208_1983(&c, ch) &&
        !ef_map_jisx0208_necibm_ext_to_jisx0212_1990(&c, ch)) {
      return;
    }

    *ch = c;
  } else if (ch->cs == SJIS_IBM_EXT) {
    if (!ef_map_sjis_ibm_ext_to_jisx0208_1983(&c, ch) &&
        !ef_map_sjis_ibm_ext_to_jisx0212_1990(&c, ch)) {
      return;
    }

    *ch = c;
  } else if (ch->cs == JISX0208_1983_MAC_EXT) {
    if (!ef_map_jisx0208_mac_ext_to_jisx0208_1983(&c, ch) &&
        !ef_map_jisx0208_mac_ext_to_jisx0212_1990(&c, ch)) {
      return;
    }

    *ch = c;
  }

  /*
   * conversion between JIS charsets.
   */
  if (ch->cs == JISC6226_1978) {
    /*
     * we ef_eucjp_parser don't support JISC6226_1978.
     * If you want to use JISC6226_1978 , use iso2022(jp).
     *
     * XXX
     * 22 characters are swapped between 1978 and 1983.
     * so , we should reswap these here , but for the time being ,
     * we do nothing.
     */

    ch->cs = JISX0208_1983;
  } else if (g1 == JISX0208_1983 && ch->cs == JISX0213_2000_1) {
    if (ef_map_jisx0213_2000_1_to_jisx0208_1983(&c, ch)) {
      *ch = c;
    }
  } else if (g1 == JISX0213_2000_1 && ch->cs == JISX0208_1983) {
    if (ef_map_jisx0208_1983_to_jisx0213_2000_1(&c, ch)) {
      *ch = c;
    }
  } else if (g3 == JISX0212_1990 && ch->cs == JISX0213_2000_2) {
    if (ef_map_jisx0213_2000_2_to_jisx0212_1990(&c, ch)) {
      *ch = c;
    }
  } else if (g3 == JISX0213_2000_2 && ch->cs == JISX0212_1990) {
    if (ef_map_jisx0212_1990_to_jisx0213_2000_2(&c, ch)) {
      *ch = c;
    }
  }
}

static size_t convert_to_eucjp(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  size_t filled_size;
  ef_char_t ch;
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, iso2022_conv->g1, iso2022_conv->g3);

    if (ch.cs == US_ASCII || ch.cs == JISX0201_ROMAN) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = *ch.ch;

      filled_size++;
    } else if (ch.cs == iso2022_conv->g1) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);

      filled_size += 2;
    } else if (ch.cs == JISX0201_KATA) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = SS2;
      *(dst++) = SET_MSB(*ch.ch);

      filled_size += 2;
    } else if (ch.cs == iso2022_conv->g3) {
      if (filled_size + 2 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = SS3;
      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);

      filled_size += 3;
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

static void eucjp_conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = JISX0208_1983;
  iso2022_conv->g2 = JISX0201_KATA;
  iso2022_conv->g3 = JISX0212_1990;
}

static void eucjisx0213_conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = JISX0213_2000_1;
  iso2022_conv->g2 = JISX0201_KATA;
  iso2022_conv->g3 = JISX0213_2000_2;
}

static void conv_destroy(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_eucjp_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  eucjp_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_eucjp;
  iso2022_conv->conv.init = eucjp_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}

ef_conv_t *ef_eucjisx0213_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  eucjisx0213_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_eucjp;
  iso2022_conv->conv.init = eucjisx0213_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}
