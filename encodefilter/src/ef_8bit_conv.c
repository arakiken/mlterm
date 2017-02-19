/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_8bit_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_ru_map.h"
#include "ef_ucs4_map.h"

#if 0
#define __DEBUG
#endif

typedef struct ef_iscii_conv {
  ef_conv_t conv;
  ef_charset_t cs;

} ef_iscii_conv_t;

/* --- static functions --- */

static int map_direct(ef_char_t *dst, ef_char_t *src, ef_charset_t to_cs) {
  if (src->cs == KOI8_U && to_cs == KOI8_R) {
    return ef_map_koi8_u_to_koi8_r(dst, src);
  } else if (src->cs == KOI8_R && to_cs == KOI8_U) {
    return ef_map_koi8_r_to_koi8_u(dst, src);
  } else if (src->cs == ISO10646_UCS4_1 && src->ch[0] == 0 && src->ch[1] == 0 && src->ch[2] == 0 &&
             src->ch[3] <= 0x7f) {
    dst->cs = US_ASCII;
    dst->size = 1;
    dst->property = 0;
    dst->ch[0] = src->ch[3];

    return 1;
  }

  return 0;
}

static void remap_unsupported_charset(ef_char_t *ch, ef_charset_t to_cs) {
  ef_char_t c;

  if (ch->cs == to_cs) {
    /* do nothing */
  } else if (map_direct(&c, ch, to_cs)) {
    *ch = c;
  } else if (ef_map_via_ucs(&c, ch, to_cs)) {
    *ch = c;
  }

  if (to_cs == VISCII && ch->cs == US_ASCII) {
    if (ch->ch[0] == 0x02 || ch->ch[0] == 0x05 || ch->ch[0] == 0x06 || ch->ch[0] == 0x14 ||
        ch->ch[0] == 0x19 || ch->ch[0] == 0x1e) {
      ch->cs = VISCII;
    }
  }
}

static size_t convert_to_intern(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser, ef_charset_t to_cs) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, to_cs);

    if (to_cs == ch.cs || ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];

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

static size_t convert_to_koi8_r(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, KOI8_R);
}

static size_t convert_to_koi8_u(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, KOI8_U);
}

static size_t convert_to_koi8_t(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, KOI8_T);
}

static size_t convert_to_georgian_ps(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                     ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, GEORGIAN_PS);
}

static size_t convert_to_cp1250(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1250);
}

static size_t convert_to_cp1251(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1251);
}

static size_t convert_to_cp1252(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1252);
}

static size_t convert_to_cp1253(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1253);
}

static size_t convert_to_cp1254(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1254);
}

static size_t convert_to_cp1255(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1255);
}

static size_t convert_to_cp1256(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1256);
}

static size_t convert_to_cp1257(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1257);
}

static size_t convert_to_cp1258(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP1258);
}

static size_t convert_to_cp874(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, CP874);
}

static size_t convert_to_viscii(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, VISCII);
}

static size_t convert_to_iscii(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  return convert_to_intern(conv, dst, dst_size, parser, ((ef_iscii_conv_t*)conv)->cs);
}

static void conv_init(ef_conv_t *conv) {}

static void conv_delete(ef_conv_t *conv) { free(conv); }

static ef_conv_t *iscii_conv_new(ef_charset_t cs) {
  ef_iscii_conv_t *iscii_conv;

  if ((iscii_conv = malloc(sizeof(ef_iscii_conv_t))) == NULL) {
    return NULL;
  }

  iscii_conv->conv.convert = convert_to_iscii;
  iscii_conv->conv.init = conv_init;
  iscii_conv->conv.delete = conv_delete;
  iscii_conv->conv.illegal_char = NULL;
  iscii_conv->cs = cs;

  return &iscii_conv->conv;
}

/* --- global functions --- */

ef_conv_t *ef_koi8_r_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_koi8_r;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_koi8_u_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_koi8_u;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_koi8_t_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_koi8_t;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_georgian_ps_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_georgian_ps;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1250_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1250;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1251_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1251;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1252_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1252;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1253_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1253;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1254_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1254;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1255_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1255;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1256_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1256;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1257_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1257;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp1258_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp1258;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_cp874_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_cp874;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_viscii_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_viscii;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_iscii_assamese_conv_new(void) { return iscii_conv_new(ISCII_ASSAMESE); }

ef_conv_t *ef_iscii_bengali_conv_new(void) { return iscii_conv_new(ISCII_BENGALI); }

ef_conv_t *ef_iscii_gujarati_conv_new(void) { return iscii_conv_new(ISCII_GUJARATI); }

ef_conv_t *ef_iscii_hindi_conv_new(void) { return iscii_conv_new(ISCII_HINDI); }

ef_conv_t *ef_iscii_kannada_conv_new(void) { return iscii_conv_new(ISCII_KANNADA); }

ef_conv_t *ef_iscii_malayalam_conv_new(void) { return iscii_conv_new(ISCII_MALAYALAM); }

ef_conv_t *ef_iscii_oriya_conv_new(void) { return iscii_conv_new(ISCII_ORIYA); }

ef_conv_t *ef_iscii_punjabi_conv_new(void) { return iscii_conv_new(ISCII_PUNJABI); }

ef_conv_t *ef_iscii_tamil_conv_new(void) { return iscii_conv_new(ISCII_TAMIL); }

ef_conv_t *ef_iscii_telugu_conv_new(void) { return iscii_conv_new(ISCII_TELUGU); }
