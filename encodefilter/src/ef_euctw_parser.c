/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_euctw_parser.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>

#include "ef_iso2022_parser.h"

#if 0
#define __DEBUG
#endif

/* --- static functions --- */

static int euctw_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  if (ef_iso2022_parser_next_char(parser, ch) == 0) {
    return 0;
  }

  if (ch->cs == CNS11643_1992_EUCTW_G2) {
    if (ch->ch[0] == 0xa2) {
      ch->cs = CNS11643_1992_2;
    } else if (ch->ch[0] == 0xa3) {
      ch->cs = CNS11643_1992_3;
    } else if (ch->ch[0] == 0xa4) {
      ch->cs = CNS11643_1992_4;
    } else if (ch->ch[0] == 0xa5) {
      ch->cs = CNS11643_1992_5;
    } else if (ch->ch[0] == 0xa6) {
      ch->cs = CNS11643_1992_6;
    } else if (ch->ch[0] == 0xa7) {
      ch->cs = CNS11643_1992_7;
    } else {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " %x is illegal euctw G2 tag.\n", ch->ch[0]);
#endif

      ef_parser_reset(parser);

      return 0;
    }

    ch->ch[0] = ch->ch[1];
    ch->ch[1] = ch->ch[2];
    ch->size = 2;
    ch->property = 0;
  }

  return 1;
}

static void euctw_parser_init(ef_parser_t* parser) {
  ef_iso2022_parser_t* iso2022_parser;

  ef_parser_init(parser);

  iso2022_parser = (ef_iso2022_parser_t*)parser;

  iso2022_parser->g0 = US_ASCII;
  iso2022_parser->g1 = CNS11643_1992_1;
  iso2022_parser->g2 = CNS11643_1992_EUCTW_G2;
  iso2022_parser->g3 = UNKNOWN_CS;

  iso2022_parser->gl = &iso2022_parser->g0;
  iso2022_parser->gr = &iso2022_parser->g1;

  iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

  iso2022_parser->is_single_shifted = 0;
}

/* --- global functions --- */

ef_parser_t* ef_euctw_parser_new(void) {
  ef_iso2022_parser_t* iso2022_parser;

  if ((iso2022_parser = ef_iso2022_parser_new()) == NULL) {
    return NULL;
  }

  euctw_parser_init((ef_parser_t*)iso2022_parser);

  /* override */
  iso2022_parser->parser.init = euctw_parser_init;
  iso2022_parser->parser.next_char = euctw_parser_next_char;

  return (ef_parser_t*)iso2022_parser;
}
