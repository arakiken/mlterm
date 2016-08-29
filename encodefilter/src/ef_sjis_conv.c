/*
 *	$Id$
 */

#include "ef_sjis_conv.h"

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>

#include "ef_iso2022_intern.h"
#include "ef_ja_jp_map.h"
#include "ef_sjis_env.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t* ch, int is_sjisx0213) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (!ef_map_ucs4_to_ja_jp(&c, ch)) {
      return;
    }

    *ch = c;
  }

  if (ch->cs == JISC6226_1978) {
    /*
     * we ef_sjis_parser don't support JISC6226_1978.
     * If you want to use JISC6226_1978 , use iso2022(jp).
     *
     * XXX
     * 22 characters are swapped between 1978 and 1983.
     * so , we should reswap these here , but for the time being ,
     * we do nothing.
     */

    ch->cs = JISX0208_1983;
  } else if (ch->cs == JISX0212_1990) {
    if (is_sjisx0213) {
      if (ef_map_jisx0212_1990_to_jisx0213_2000_2(&c, ch)) {
        *ch = c;
      }
    } else {
      if (ef_get_sjis_input_type() == MICROSOFT_CS) {
        if (ef_map_jisx0212_1990_to_jisx0208_nec_ext(&c, ch) ||
            ef_map_jisx0212_1990_to_jisx0208_necibm_ext(&c, ch) ||
            ef_map_jisx0212_1990_to_sjis_ibm_ext(&c, ch)) {
          *ch = c;
        }
      } else if (ef_get_sjis_input_type() == APPLE_CS) {
        if (ef_map_jisx0212_1990_to_jisx0208_mac_ext(&c, ch)) {
          *ch = c;
        }
      }
    }
  } else if (is_sjisx0213 && ch->cs == JISC6226_1978_NEC_EXT) {
    if (ef_map_jisx0208_nec_ext_to_jisx0213_2000(&c, ch)) {
      *ch = c;
    }
  } else if (is_sjisx0213 && ch->cs == JISC6226_1978_NECIBM_EXT) {
    if (ef_map_jisx0208_necibm_ext_to_jisx0213_2000(&c, ch)) {
      *ch = c;
    }
  } else if (is_sjisx0213 && ch->cs == SJIS_IBM_EXT) {
    if (ef_map_sjis_ibm_ext_to_jisx0213_2000(&c, ch)) {
      *ch = c;
    }
  } else if (is_sjisx0213 && ch->cs == JISX0208_1983_MAC_EXT) {
    if (ef_map_jisx0208_mac_ext_to_jisx0213_2000(&c, ch)) {
      *ch = c;
    }
  } else if (is_sjisx0213 && ch->cs == JISX0208_1983) {
    if (ef_map_jisx0208_1983_to_jisx0213_2000_1(&c, ch)) {
      *ch = c;
    }
  } else if (!is_sjisx0213 && ch->cs == JISX0213_2000_1) {
    if (ef_map_jisx0213_2000_1_to_jisx0208_1983(&c, ch)) {
      *ch = c;
    }
  } else if (!is_sjisx0213 && ch->cs == JISX0213_2000_2) {
    if (ef_get_sjis_input_type() == MICROSOFT_CS) {
      if (ef_map_jisx0213_2000_2_to_jisx0208_nec_ext(&c, ch) ||
          ef_map_jisx0213_2000_2_to_jisx0208_necibm_ext(&c, ch) ||
          ef_map_jisx0213_2000_2_to_sjis_ibm_ext(&c, ch)) {
        *ch = c;
      }
    } else if (ef_get_sjis_input_type() == APPLE_CS) {
      if (ef_map_jisx0213_2000_2_to_jisx0208_mac_ext(&c, ch)) {
        *ch = c;
      }
    }
  }
}

static int map_jisx0208_1983_to_sjis(u_char* dst, /* 2bytes */
                                     u_char* src  /* 2bytes */
                                     ) {
  int high;
  int low;

  high = *src;
  low = *(src + 1);

  if ((high & 0x01) == 1) {
    low += 0x1f;
  } else {
    low += 0x7d;
  }

  if (low >= 0x7f) {
    low++;
  }

  high = (high - 0x21) / 2 + 0x81;

  if (high > 0x9f) {
    high += 0x40;
  }

  *dst = high;
  *(dst + 1) = low;

  return 1;
}

static int map_jisx0213_2000_to_sjis(u_char* dst, /* 2bytes */
                                     u_char* src, /* 2bytes */
                                     int map      /* map of jisx0213(1 or 2) */
                                     ) {
  int high;
  int low;

  high = *src;
  low = *(src + 1);

  if (high % 2 == 1) {
    if (low <= 0x5f) {
      low += 0x1f;
    } else if (/* 0x60 <= low && */ low <= 0x7e) {
      low += 0x20;
    }
  } else {
    low += 0x7e;
  }

  if (map == 1) {
    if (high <= 0x5e) {
      high = (high + 0xe1) / 2;
    } else if (/* 0x5f <= high && */ high <= 0x7e) {
      high = (high + 0x161) / 2;
    } else {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " 0x%.2x is illegal upper byte of jisx0213.\n", high);
#endif

      return 0;
    }
  } else if (map == 2) {
    if (high == 0x21 || high == 0x23 || high == 0x24 || high == 0x25 || high == 0x28 ||
        high == 0x2c || high == 0x2d || high == 0x2e || high == 0x2f) {
      high = (high + 0x1bf) / 2 - ((high - 0x20) / 8) * 3;
    } else if (0x6e <= high && high <= 0x7e) {
      high = (high + 0x17b) / 2;
    } else {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " 0x%.2x is illegal higher byte of jisx0213.\n", high);
#endif

      return 0;
    }
  } else {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %d is illegal map of jisx0213.\n", map);
#endif

    return 0;
  }

  *dst = high;
  *(dst + 1) = low;

  return 1;
}

static size_t convert_to_sjis_intern(ef_conv_t* conv, u_char* dst, size_t dst_size,
                                     ef_parser_t* parser, int is_sjisx0213) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, is_sjisx0213);

    /*
     * encoding the following characterset to shift-jis.
     */

    if (is_sjisx0213 == 1 && ch.cs == JISX0213_2000_1) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      if (map_jisx0213_2000_to_sjis(dst, ch.ch, 1) == 0) {
        /* ignoring these two bytes. */

        continue;
      }

      dst += 2;
      filled_size += 2;
    } else if (is_sjisx0213 == 1 && ch.cs == JISX0213_2000_2) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      if (map_jisx0213_2000_to_sjis(dst, ch.ch, 2) == 0) {
        /* ignoring these two bytes. */

        continue;
      }

      dst += 2;
      filled_size += 2;
    } else if (is_sjisx0213 == 0 &&
               (ch.cs == JISX0208_1983 || ch.cs == JISC6226_1978_NEC_EXT ||
                ch.cs == JISC6226_1978_NECIBM_EXT || ch.cs == JISX0208_1983_MAC_EXT)) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      if (map_jisx0208_1983_to_sjis(dst, ch.ch) == 0) {
        /* ignoring these two bytes. */

        continue;
      }

      dst += 2;
      filled_size += 2;
    } else if (is_sjisx0213 == 0 && ch.cs == SJIS_IBM_EXT) {
      if (filled_size + 2 > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];
      *(dst++) = ch.ch[1];
    } else if (ch.cs == US_ASCII || ch.cs == JISX0201_ROMAN) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = *ch.ch;

      filled_size++;
    } else if (ch.cs == JISX0201_KATA) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = MAP_TO_GR(*ch.ch);

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

static size_t convert_to_sjis(ef_conv_t* conv, u_char* dst, size_t dst_size,
                              ef_parser_t* parser) {
  return convert_to_sjis_intern(conv, dst, dst_size, parser, 0);
}

static size_t convert_to_sjisx0213(ef_conv_t* conv, u_char* dst, size_t dst_size,
                                   ef_parser_t* parser) {
  return convert_to_sjis_intern(conv, dst, dst_size, parser, 1);
}

static void conv_init(ef_conv_t* conv) {}

static void conv_delete(ef_conv_t* conv) { free(conv); }

/* --- global functions --- */

ef_conv_t* ef_sjis_conv_new(void) {
  ef_conv_t* conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_sjis;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t* ef_sjisx0213_conv_new(void) {
  ef_conv_t* conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_sjisx0213;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}
