/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_str_parser.h"

#include <stdio.h>

/* --- static functions --- */

/* Same as ef_parser_init */
static void parser_init(ef_parser_t *parser) {
  parser->str = NULL;
  parser->marked_left = 0;
  parser->left = 0;

  parser->is_eos = 0;
}

/* Same as ef_parser_n_increment */
static size_t parser_n_increment(ef_parser_t *parser, size_t n) {
  if (parser->left <= n) {
    parser->str += parser->left;
    parser->left = 0;
    parser->is_eos = 1;
  } else {
    parser->str += n;
    parser->left -= n;
  }

  return parser->left;
}

static void set_str(ef_parser_t *parser, u_char *str, /* ef_char_t* */
                    size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

static void delete (ef_parser_t *parser) {}

static int next_char(ef_parser_t *parser, ef_char_t *ch) {
  if (parser->is_eos) {
    return 0;
  }

  *ch = ((ef_char_t*)parser->str)[0];
  parser_n_increment(parser, sizeof(ef_char_t));

  return 1;
}

/* --- static variables --- */

static ef_parser_t parser = {
    NULL, 0, 0, 0, parser_init, set_str, delete, next_char,
};

/* --- global functions --- */

ef_parser_t *ef_str_parser_init(ef_char_t *src, u_int src_len) {
  (*parser.init)(&parser);
  (*parser.set_str)(&parser, (u_char*)src, src_len * sizeof(ef_char_t));

  return &parser;
}
