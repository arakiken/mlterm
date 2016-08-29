/*
 *	$Id$
 */

#include "ef_codepoint_parser.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

typedef struct cp_parser {
  ef_parser_t parser;

  ef_charset_t cs;
  size_t cp_size;

} cp_parser_t;

/* --- static functions --- */

static void cp_parser_init(ef_parser_t* parser) {
  cp_parser_t* cp_parser;

  cp_parser = (cp_parser_t*)parser;

  ef_parser_init(parser);
  cp_parser->cs = UNKNOWN_CS;
  cp_parser->cp_size = 1;
}

static void cp_parser_set_str(ef_parser_t* parser, u_char* str,
                              size_t size /* size(max 16bit) | cs << 16 */
                              ) {
  ef_charset_t cs;
  cp_parser_t* cp_parser;

  cp_parser = (cp_parser_t*)parser;

  cs = (ef_charset_t)((size >> 16) & 0xffff);

  cp_parser->parser.str = str;
  cp_parser->parser.left = size & 0xffff;
  cp_parser->parser.marked_left = 0;
  cp_parser->parser.is_eos = 0;
  cp_parser->cs = cs;

  if (cs == ISO10646_UCS4_1) {
    cp_parser->cp_size = 4;
  } else if (IS_FULLWIDTH_CS(cs) || cs == ISO10646_UCS2_1) {
    cp_parser->cp_size = 2;
  } else {
    cp_parser->cp_size = 1;
  }
}

static void cp_parser_delete(ef_parser_t* parser) { free(parser); }

static int cp_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  cp_parser_t* cp_parser;
  size_t count;

  cp_parser = (cp_parser_t*)parser;

  if (cp_parser->parser.is_eos) {
    return 0;
  }

  if (cp_parser->parser.left < cp_parser->cp_size) {
    cp_parser->parser.is_eos = 1;

    return 0;
  }

  for (count = 0; count < cp_parser->cp_size; count++) {
    ch->ch[count] = cp_parser->parser.str[count];
  }

  ef_parser_n_increment(cp_parser, count);

  ch->size = count;
  ch->cs = cp_parser->cs;
  ch->property = 0;

  return 1;
}

/* --- global functions --- */

ef_parser_t* ef_codepoint_parser_new(void) {
  cp_parser_t* cp_parser;

  if ((cp_parser = malloc(sizeof(cp_parser_t))) == NULL) {
    return NULL;
  }

  cp_parser_init(&cp_parser->parser);

  cp_parser->parser.init = cp_parser_init;
  cp_parser->parser.set_str = cp_parser_set_str;
  cp_parser->parser.delete = cp_parser_delete;
  cp_parser->parser.next_char = cp_parser_next_char;

  return &cp_parser->parser;
}
