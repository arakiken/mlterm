/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_PARSER_H__
#define __EF_PARSER_H__

#include <pobl/bl_types.h> /* size_t */

#include "ef_char.h"

typedef struct ef_parser {
  /* private */
  u_char *str;
  size_t marked_left;
  size_t left;

  /* public */
  int is_eos;

  /* public */
  void (*init)(struct ef_parser *);
  void (*set_str)(struct ef_parser *, u_char *str, size_t size);
  void (*destroy)(struct ef_parser *);
  int (*next_char)(struct ef_parser *, ef_char_t *);

} ef_parser_t;

#define ef_parser_increment(parser) __ef_parser_increment((ef_parser_t *)parser)

#define ef_parser_n_increment(parser, n) __ef_parser_n_increment((ef_parser_t *)parser, n)

#define ef_parser_reset(parser) __ef_parser_reset((ef_parser_t *)parser)

#define ef_parser_full_reset(parser) __ef_parser_full_reset((ef_parser_t *)parser)

#define ef_parser_mark(parser) __ef_parser_mark((ef_parser_t *)parser)

void ef_parser_init(ef_parser_t *parser);

size_t __ef_parser_increment(ef_parser_t *parser);

size_t __ef_parser_n_increment(ef_parser_t *parser, size_t n);

void __ef_parser_mark(ef_parser_t *parser);

void __ef_parser_reset(ef_parser_t *parser);

void __ef_parser_full_reset(ef_parser_t *parser);

int ef_parser_next_char(ef_parser_t *parser, ef_char_t *ch);

#endif
