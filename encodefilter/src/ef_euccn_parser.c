/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euccn_parser.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>

#include "ef_iso2022_parser.h"
#include "ef_gb18030_2000_intern.h"
#include "ef_ucs_property.h"

#if 0
#define __DEBUG
#endif

/* --- static functions --- */

static int gbk_parser_next_char_intern(ef_parser_t *parser, ef_char_t *ch, int is_gb18030) {
  if (parser->is_eos) {
    return 0;
  }

  ef_parser_mark(parser);

  if (/* 0x00 <= *parser->str && */ *parser->str <= 0x80) {
    ch->ch[0] = *parser->str;
    ch->cs = US_ASCII;
    ch->size = 1;
    ch->property = 0;

    ef_parser_increment(parser);

    return 1;
  } else {
    u_char bytes[4];
    u_char ucs4[4];

    if (is_gb18030) {
      if (0x81 <= *parser->str && *parser->str <= 0xa0) {
        bytes[0] = *parser->str;

        if (ef_parser_increment(parser) == 0) {
          goto shortage;
        }

        if (0x30 <= *parser->str && *parser->str <= 0x39) {
          goto is_4_bytes;
        }
      } else if (0xa1 <= *parser->str && *parser->str <= 0xfe) {
        bytes[0] = *parser->str;

        if (ef_parser_increment(parser) == 0) {
          goto shortage;
        }

        if (0x30 <= *parser->str && *parser->str <= 0x39) {
          goto is_4_bytes;
        }
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " illegal GBK format. [%x ...]\n", *parser->str);
#endif

        goto error;
      }
    } else {
      bytes[0] = *parser->str;

      if (ef_parser_increment(parser) == 0) {
        goto shortage;
      }
    }

    ch->ch[0] = bytes[0];

    if (*parser->str < 0x40) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " illegal GBK format. [%.2x%.2x ...]\n", bytes[0], *parser->str);
#endif

      goto error;
    }

    ch->ch[1] = *parser->str;
    ch->size = 2;
    ch->cs = GBK;
    ch->property = 0;

    ef_parser_increment(parser);

    return 1;

  is_4_bytes:
    bytes[1] = *parser->str;

    if (ef_parser_increment(parser) == 0) {
      goto shortage;
    }

    if (*parser->str < 0x81 || 0xfe < *parser->str) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " illegal GBK format. [%.2x%.2x%.2x ...]\n", bytes[0], bytes[1],
                     *parser->str);
#endif

      goto error;
    }

    bytes[2] = *parser->str;

    if (ef_parser_increment(parser) == 0) {
      goto shortage;
    }

    if (*parser->str < 0x30 || 0x39 < *parser->str) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " illegal GBK format. [%.2x%.2x%.2x%.2x]\n", bytes[0], bytes[1],
                     bytes[2], *parser->str);
#endif

      goto error;
    }

    bytes[3] = *parser->str;

    ef_parser_increment(parser);

    if (ef_decode_gb18030_2000_to_ucs4(ucs4, bytes) == 0) {
      goto error;
    }

    memcpy(ch->ch, ucs4, 4);
    ch->size = 4;
    ch->cs = ISO10646_UCS4_1;
    ch->property = ef_get_ucs_property(ef_bytes_to_int(ucs4, 4));

    return 1;
  }

error:
shortage:
  ef_parser_reset(parser);

  return 0;
}

static int gbk_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  return gbk_parser_next_char_intern(parser, ch, 0);
}

static int gb18030_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  return gbk_parser_next_char_intern(parser, ch, 1);
}

static void euccn_parser_init_intern(ef_parser_t *parser, ef_charset_t g1_cs) {
  ef_iso2022_parser_t *iso2022_parser;

  ef_parser_init(parser);

  iso2022_parser = (ef_iso2022_parser_t *)parser;

  iso2022_parser->g0 = US_ASCII;
  iso2022_parser->g1 = g1_cs;
  iso2022_parser->g2 = UNKNOWN_CS;
  iso2022_parser->g3 = UNKNOWN_CS;

  iso2022_parser->gl = &iso2022_parser->g0;
  iso2022_parser->gr = &iso2022_parser->g1;

  iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

  iso2022_parser->is_single_shifted = 0;
}

static void euccn_parser_init(ef_parser_t *parser) { euccn_parser_init_intern(parser, GB2312_80); }

/*
 * shared by gbk and gbk18030_2000
 */
static void gbk_parser_init(ef_parser_t *parser) { euccn_parser_init_intern(parser, GBK); }

static ef_parser_t *gbk_parser_new(void (*init)(struct ef_parser *),
                                    int (*next_char)(struct ef_parser *, ef_char_t *)) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  (*init)((ef_parser_t *)iso2022_parser);

  /* override */
  iso2022_parser->parser.init = init;
  iso2022_parser->parser.next_char = next_char;

  return (ef_parser_t *)iso2022_parser;
}

/* --- global functions --- */

ef_parser_t *ef_euccn_parser_new(void) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  euccn_parser_init((ef_parser_t *)iso2022_parser);

  /* override */
  iso2022_parser->parser.init = euccn_parser_init;

  return (ef_parser_t *)iso2022_parser;
}

ef_parser_t *ef_gbk_parser_new(void) {
  return gbk_parser_new(gbk_parser_init, gbk_parser_next_char);
}

ef_parser_t *ef_gb18030_2000_parser_new(void) {
  return gbk_parser_new(gbk_parser_init, gb18030_parser_next_char);
}
