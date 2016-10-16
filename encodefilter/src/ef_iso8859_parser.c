/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_iso8859_parser.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>

#include "ef_iso2022_parser.h"

/* --- static functions --- */

static void iso8859_parser_init(ef_parser_t *parser, ef_charset_t gr_cs) {
  ef_iso2022_parser_t *iso2022_parser;

  ef_parser_init(parser);

  iso2022_parser = (ef_iso2022_parser_t *)parser;

  iso2022_parser->g0 = US_ASCII;
  iso2022_parser->g1 = gr_cs;
  iso2022_parser->g2 = UNKNOWN_CS;
  iso2022_parser->g3 = UNKNOWN_CS;

  iso2022_parser->gl = &iso2022_parser->g0;
  iso2022_parser->gr = &iso2022_parser->g1;

  iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

  iso2022_parser->is_single_shifted = 0;
}

static void iso8859_1_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_1_R);
}

static void iso8859_2_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_2_R);
}

static void iso8859_3_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_3_R);
}

static void iso8859_4_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_4_R);
}

static void iso8859_5_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_5_R);
}

static void iso8859_6_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_6_R);
}

static void iso8859_7_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_7_R);
}

static void iso8859_8_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_8_R);
}

static void iso8859_9_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_9_R);
}

static void iso8859_10_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_10_R);
}

static void tis620_2533_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, TIS620_2533);
}

static void iso8859_13_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_13_R);
}

static void iso8859_14_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_14_R);
}

static void iso8859_15_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_15_R);
}

static void iso8859_16_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, ISO8859_16_R);
}

static void tcvn5712_3_1993_parser_init(ef_parser_t *parser) {
  iso8859_parser_init(parser, TCVN5712_3_1993);
}

static ef_parser_t *iso8859_parser_new(void (*init)(struct ef_parser *)) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  (*init)((ef_parser_t *)iso2022_parser);

  /* overwrite */
  iso2022_parser->parser.init = init;

  return (ef_parser_t *)iso2022_parser;
}

/* --- global functions --- */

ef_parser_t *ef_iso8859_1_parser_new(void) { return iso8859_parser_new(iso8859_1_parser_init); }

ef_parser_t *ef_iso8859_2_parser_new(void) { return iso8859_parser_new(iso8859_2_parser_init); }

ef_parser_t *ef_iso8859_3_parser_new(void) { return iso8859_parser_new(iso8859_3_parser_init); }

ef_parser_t *ef_iso8859_4_parser_new(void) { return iso8859_parser_new(iso8859_4_parser_init); }

ef_parser_t *ef_iso8859_5_parser_new(void) { return iso8859_parser_new(iso8859_5_parser_init); }

ef_parser_t *ef_iso8859_6_parser_new(void) { return iso8859_parser_new(iso8859_6_parser_init); }

ef_parser_t *ef_iso8859_7_parser_new(void) { return iso8859_parser_new(iso8859_7_parser_init); }

ef_parser_t *ef_iso8859_8_parser_new(void) { return iso8859_parser_new(iso8859_8_parser_init); }

ef_parser_t *ef_iso8859_9_parser_new(void) { return iso8859_parser_new(iso8859_9_parser_init); }

ef_parser_t *ef_iso8859_10_parser_new(void) { return iso8859_parser_new(iso8859_10_parser_init); }

ef_parser_t *ef_tis620_2533_parser_new(void) {
  return iso8859_parser_new(tis620_2533_parser_init);
}

ef_parser_t *ef_iso8859_13_parser_new(void) { return iso8859_parser_new(iso8859_13_parser_init); }

ef_parser_t *ef_iso8859_14_parser_new(void) { return iso8859_parser_new(iso8859_14_parser_init); }

ef_parser_t *ef_iso8859_15_parser_new(void) { return iso8859_parser_new(iso8859_15_parser_init); }

ef_parser_t *ef_iso8859_16_parser_new(void) { return iso8859_parser_new(iso8859_16_parser_init); }

ef_parser_t *ef_tcvn5712_3_1993_parser_new(void) {
  return iso8859_parser_new(tcvn5712_3_1993_parser_init);
}
