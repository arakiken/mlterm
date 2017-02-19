/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euckr_parser.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>

#include "ef_iso2022_parser.h"

#if 0
#define __DEBUG
#endif

/* --- static functions --- */

static int uhc_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  if (parser->is_eos) {
    return 0;
  }

  ef_parser_mark(parser);

  if (/* 0x00 <= *parser->str && */ *parser->str <= 0x80) {
    ch->ch[0] = *parser->str;
    ch->cs = US_ASCII;
    ch->size = 1;
  } else {
    ch->ch[0] = *parser->str;

    if (ef_parser_increment(parser) == 0) {
      ef_parser_reset(parser);

      return 0;
    }

    ch->ch[1] = *parser->str;
    ch->size = 2;
    ch->cs = UHC;
  }

  ch->property = 0;

  ef_parser_increment(parser);

  return 1;
}

static void euckr_parser_init_intern(ef_parser_t *parser, ef_charset_t g1_cs) {
  ef_iso2022_parser_t *iso2022_parser;

  ef_parser_init(parser);

  iso2022_parser = (ef_iso2022_parser_t*)parser;

  iso2022_parser->g0 = US_ASCII;
  iso2022_parser->g1 = g1_cs;
  iso2022_parser->g2 = UNKNOWN_CS;
  iso2022_parser->g3 = UNKNOWN_CS;

  iso2022_parser->gl = &iso2022_parser->g0;
  iso2022_parser->gr = &iso2022_parser->g1;

  iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

  iso2022_parser->is_single_shifted = 0;
}

static void euckr_parser_init(ef_parser_t *parser) {
  euckr_parser_init_intern(parser, KSC5601_1987);
}

static void uhc_parser_init(ef_parser_t *parser) { euckr_parser_init_intern(parser, UHC); }

/* --- global functions --- */

ef_parser_t *ef_euckr_parser_new(void) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  euckr_parser_init((ef_parser_t*)iso2022_parser);

  /* override */
  iso2022_parser->parser.init = euckr_parser_init;

  return (ef_parser_t*)iso2022_parser;
}

ef_parser_t *ef_uhc_parser_new(void) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  uhc_parser_init((ef_parser_t*)iso2022_parser);

  /* override */
  iso2022_parser->parser.init = uhc_parser_init;
  iso2022_parser->parser.next_char = uhc_parser_next_char;

  return (ef_parser_t*)iso2022_parser;
}
