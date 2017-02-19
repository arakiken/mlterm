/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_parser.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>

/* --- global functions --- */

void ef_parser_init(ef_parser_t *parser) {
  parser->str = NULL;
  parser->marked_left = 0;
  parser->left = 0;

  parser->is_eos = 0;
}

size_t __ef_parser_increment(ef_parser_t *parser) {
  if (parser->left <= 1) {
    parser->str += parser->left;
    parser->left = 0;
    parser->is_eos = 1;
  } else {
    parser->str++;
    parser->left--;
  }

  return parser->left;
}

size_t __ef_parser_n_increment(ef_parser_t *parser, size_t n) {
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

void __ef_parser_mark(ef_parser_t *parser) { parser->marked_left = parser->left; }

void __ef_parser_reset(ef_parser_t *parser) {
  parser->str -= (parser->marked_left - parser->left);
  parser->left = parser->marked_left;
}

void __ef_parser_full_reset(ef_parser_t *parser) {
  if (parser->is_eos && parser->marked_left > parser->left) {
    parser->is_eos = 0;
  }

  __ef_parser_reset(parser);
}

/*
 * short cut function. (ignoring error)
 */
int ef_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  while (1) {
#if 0
    /*
     * just to be sure...
     * ef_parser_mark() should be called inside [encoding]_next_char()
     * function.
     */
    ef_parser_mark(parser);
#endif

    if ((*parser->next_char)(parser, ch)) {
      return 1;
    } else if (parser->is_eos ||
               /* parser->next_char() returns error and skip to next char */
               ef_parser_increment(parser) == 0) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " parser reached the end of string.\n");
#endif

      return 0;
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " parser->next_char() returns error , continuing...\n");
    }
#endif
  }
}
