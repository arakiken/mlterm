/*
 *	$Id$
 */

#include "ef_str_parser.h"

#include <stdio.h>

/* --- static functions --- */

static void set_str(ef_parser_t* parser, u_char* str, /* ef_char_t* */
                    size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

static void delete (ef_parser_t* parser) {}

static int next_char(ef_parser_t* parser, ef_char_t* ch) {
  if (parser->is_eos) {
    return 0;
  }

  *ch = ((ef_char_t*)parser->str)[0];
  ef_parser_n_increment(parser, sizeof(ef_char_t));

  return 1;
}

/* --- static variables --- */

static ef_parser_t parser = {
    NULL, 0, 0, 0, ef_parser_init, set_str, delete, next_char,
};

/* --- global functions --- */

ef_parser_t* ef_str_parser_init(ef_char_t* src, u_int src_len) {
  (*parser.init)(&parser);
  (*parser.set_str)(&parser, (u_char*)src, src_len * sizeof(ef_char_t));

  return &parser;
}
