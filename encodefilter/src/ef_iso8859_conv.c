/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_iso8859_conv.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_viet_map.h"
#include "ef_ru_map.h"
#include "ef_ucs4_iso8859.h"
#include "ef_ucs4_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch, ef_charset_t gr_cs) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_cs(&c, ch, gr_cs)) {
      *ch = c;

      return;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);
}

static size_t convert_to_iso8859(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                 ef_parser_t *parser) {
  ef_iso2022_conv_t *iso2022_conv;
  size_t filled_size;
  ef_char_t ch;

  iso2022_conv = (ef_iso2022_conv_t *)conv;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, iso2022_conv->g1);

    if (ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];
      filled_size++;
    } else if (ch.cs == iso2022_conv->g1) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = SET_MSB(ch.ch[0]);
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

static void conv_init_intern(ef_conv_t *conv, ef_charset_t g1) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t *)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = g1;
  iso2022_conv->g2 = UNKNOWN_CS;
  iso2022_conv->g3 = UNKNOWN_CS;
}

static void conv_init_iso8859_1(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_1_R); }

static void conv_init_iso8859_2(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_2_R); }

static void conv_init_iso8859_3(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_3_R); }

static void conv_init_iso8859_4(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_4_R); }

static void conv_init_iso8859_5(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_5_R); }

static void conv_init_iso8859_6(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_6_R); }

static void conv_init_iso8859_7(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_7_R); }

static void conv_init_iso8859_8(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_8_R); }

static void conv_init_iso8859_9(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_9_R); }

static void conv_init_iso8859_10(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_10_R); }

static void conv_init_tis620_2533(ef_conv_t *conv) { conv_init_intern(conv, TIS620_2533); }

static void conv_init_iso8859_13(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_13_R); }

static void conv_init_iso8859_14(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_14_R); }

static void conv_init_iso8859_15(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_15_R); }

static void conv_init_iso8859_16(ef_conv_t *conv) { conv_init_intern(conv, ISO8859_16_R); }

static void conv_init_tcvn5712_3_1993(ef_conv_t *conv) { conv_init_intern(conv, TCVN5712_3_1993); }

static void conv_destroy(ef_conv_t *conv) { free(conv); }

static ef_conv_t *iso8859_conv_new(void (*init)(ef_conv_t *)) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  (*init)((ef_conv_t *)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_iso8859;
  iso2022_conv->conv.init = init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t *)iso2022_conv;
}

/* --- global functions --- */

ef_conv_t *ef_iso8859_1_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_1); }

ef_conv_t *ef_iso8859_2_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_2); }

ef_conv_t *ef_iso8859_3_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_3); }

ef_conv_t *ef_iso8859_4_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_4); }

ef_conv_t *ef_iso8859_5_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_5); }

ef_conv_t *ef_iso8859_6_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_6); }

ef_conv_t *ef_iso8859_7_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_7); }

ef_conv_t *ef_iso8859_8_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_8); }

ef_conv_t *ef_iso8859_9_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_9); }

ef_conv_t *ef_iso8859_10_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_10); }

ef_conv_t *ef_tis620_2533_conv_new(void) { return iso8859_conv_new(conv_init_tis620_2533); }

ef_conv_t *ef_iso8859_13_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_13); }

ef_conv_t *ef_iso8859_14_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_14); }

ef_conv_t *ef_iso8859_15_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_15); }

ef_conv_t *ef_iso8859_16_conv_new(void) { return iso8859_conv_new(conv_init_iso8859_16); }

ef_conv_t *ef_tcvn5712_3_1993_conv_new(void) {
  return iso8859_conv_new(conv_init_tcvn5712_3_1993);
}
