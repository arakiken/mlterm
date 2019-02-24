/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euccn_conv.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_zh_cn_map.h"
#include "ef_gb18030_2000_intern.h"

typedef enum euccn_encoding {
  EUCCN_NORMAL,
  EUCCN_GBK,
  EUCCN_GB18030_2000

} enccn_encoding_t;

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch, enccn_encoding_t encoding) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (ef_map_ucs4_to_zh_cn(&c, ch)) {
      *ch = c;
    }
  }

  if (encoding == EUCCN_NORMAL) {
    ef_iso2022_remap_unsupported_charset(ch);
  } else {
    if (ch->cs == ISO10646_UCS4_1) {
      return;
    }

    if (ch->cs == GB2312_80) {
      if (ef_map_gb2312_80_to_gbk(&c, ch)) {
        *ch = c;
      }
    }
  }
}

static size_t convert_to_euccn_intern(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                      ef_parser_t *parser, enccn_encoding_t encoding) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch, encoding);

    if (ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = *ch.ch;

      filled_size++;
    } else if (encoding == EUCCN_NORMAL && ch.cs == GB2312_80) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = MAP_TO_GR(ch.ch[0]);
      *(dst++) = MAP_TO_GR(ch.ch[1]);

      filled_size += 2;
    } else if ((encoding == EUCCN_GBK || encoding == EUCCN_GB18030_2000) && ch.cs == GBK) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];
      *(dst++) = ch.ch[1];

      filled_size += 2;
    } else if (encoding == EUCCN_GB18030_2000 && ch.cs == ISO10646_UCS4_1) {
      u_char gb18030[4];

      if (filled_size + 3 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      if (ef_encode_ucs4_to_gb18030_2000(gb18030, ch.ch) == 0) {
        continue;
      }

      *(dst++) = gb18030[0];
      *(dst++) = gb18030[1];
      *(dst++) = gb18030[2];
      *(dst++) = gb18030[3];

      filled_size += 4;
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

static size_t convert_to_euccn(ef_conv_t *conv, u_char *dst, size_t dst_size,
                               ef_parser_t *parser) {
  return convert_to_euccn_intern(conv, dst, dst_size, parser, EUCCN_NORMAL);
}

static size_t convert_to_gbk(ef_conv_t *conv, u_char *dst, size_t dst_size, ef_parser_t *parser) {
  return convert_to_euccn_intern(conv, dst, dst_size, parser, EUCCN_GBK);
}

static size_t convert_to_gb18030_2000(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                      ef_parser_t *parser) {
  return convert_to_euccn_intern(conv, dst, dst_size, parser, EUCCN_GB18030_2000);
}

static void euccn_conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = GB2312_80;
  iso2022_conv->g2 = UNKNOWN_CS;
  iso2022_conv->g3 = UNKNOWN_CS;
}

static void conv_init(ef_conv_t *conv) {}

static void conv_destroy(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_euccn_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  euccn_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_euccn;
  iso2022_conv->conv.init = euccn_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}

ef_conv_t *ef_gbk_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_gbk;
  conv->init = conv_init;
  conv->destroy = conv_destroy;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t *ef_gb18030_2000_conv_new(void) {
  ef_conv_t *conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_gb18030_2000;
  conv->init = conv_init;
  conv->destroy = conv_destroy;
  conv->illegal_char = NULL;

  return conv;
}
