/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_ISO2022_PARSER_H__
#define __EF_ISO2022_PARSER_H__

#include <pobl/bl_types.h> /* size_t */

#include "ef_parser.h"

/*
 * If enacs=\E(B\E)0,smacs=^N,rmacs=^O, G1 of current encoding is unexpectedly
 * changed
 * by enacs. DECSP_HACK is a hack to avoid this problem. (If
 * enacs=,smacs=\E(0,rmacs=\E(B,
 * no problem.)
 * (enacs:initialize, smacs: start decsp, rmacs: end decsp)
 */
#if 1
#define DECSP_HACK
#endif

typedef struct ef_iso2022_parser {
  ef_parser_t parser;

  ef_charset_t *gl;
  ef_charset_t *gr;

  ef_charset_t g0;
  ef_charset_t g1;
  ef_charset_t g2;
  ef_charset_t g3;

  ef_charset_t non_iso2022_cs;

#ifdef DECSP_HACK
  int8_t g1_is_decsp;
#endif

  int8_t is_single_shifted;

  int (*non_iso2022_is_started)(struct ef_iso2022_parser *);
  int (*next_non_iso2022_byte)(struct ef_iso2022_parser *, ef_char_t *);

} ef_iso2022_parser_t;

ef_iso2022_parser_t *ef_iso2022_parser_new(void);

int ef_iso2022_parser_init_func(ef_iso2022_parser_t *iso2022_parser);

void ef_iso2022_parser_set_str(ef_parser_t *parser, u_char *str, size_t size);

void ef_iso2022_parser_delete(ef_parser_t *parser);

int ef_iso2022_parser_next_char(ef_parser_t *parser, ef_char_t *ch);

#endif
