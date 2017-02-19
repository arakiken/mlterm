/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_johab_parser.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

/* --- static functions --- */

static int johab_parser_next_char(ef_parser_t *johab_parser, ef_char_t *ch) {
  if (johab_parser->is_eos) {
    return 0;
  }

  ef_parser_mark(johab_parser);

  if (/* 0x0 <= *johab_parser->str && */ *johab_parser->str <= 0x7f) {
    ch->ch[0] = *johab_parser->str;
    ch->size = 1;
    ch->cs = US_ASCII;
  } else if ((0xd8 <= *johab_parser->str && *johab_parser->str <= 0xde) ||
             (0xe0 <= *johab_parser->str && *johab_parser->str <= 0xf9)) {
    /* KSX1001 except Hangul */

    u_char byte1;
    u_char byte2;

    byte1 = *johab_parser->str;

    if (ef_parser_increment(johab_parser) == 0) {
      goto shortage;
    }

    byte2 = *johab_parser->str;

#ifdef __DEBUG
    bl_debug_printf("0x%.2x%.2x -> ", byte1, byte2);
#endif

    if (byte2 <= 0xa0) {
      if (byte1 == 0xd8) {
        ch->ch[0] = 0x29 + 0x20;
      } else {
        if (byte1 <= 0xde) {
          ch->ch[0] = byte1 * 2 - 0x1b1 + 0x20;
        } else /* if( 0xe0 <= byte1) */
        {
          ch->ch[0] = byte1 * 2 - 0x196 + 0x20;
        }
      }

      if (byte2 <= 0x7e) {
        ch->ch[1] = byte2 - 0x30 + 0x20;
      } else {
        ch->ch[1] = byte2 - 0x42 + 0x20;
      }
    } else {
      if (byte1 == 0xd8) {
        ch->ch[0] = 0x5e + 0x20;
      } else {
        if (byte1 <= 0xde) {
          ch->ch[0] = byte1 * 2 - 0x1b0 + 0x20;
        } else /* if( 0xe0 <= byte1) */
        {
          ch->ch[0] = byte1 * 2 - 0x195 + 0x20;
        }
      }

      ch->ch[1] = byte2 - 0xa0 + 0x20;
    }

#ifdef __DEBUG
    bl_debug_printf("0x%.2x%.2x\n", ch->ch[0], ch->ch[1]);
#endif

    ch->size = 2;
    ch->cs = KSC5601_1987;
  } else {
    ch->ch[0] = *johab_parser->str;

    if (ef_parser_increment(johab_parser) == 0) {
      goto shortage;
    }

    ch->ch[1] = *johab_parser->str;
    ch->size = 2;
    ch->cs = JOHAB;
  }

  ch->property = 0;

  ef_parser_increment(johab_parser);

  return 1;

shortage:
  ef_parser_reset(johab_parser);

  return 0;
}

static void johab_parser_set_str(ef_parser_t *johab_parser, u_char *str, size_t size) {
  johab_parser->str = str;
  johab_parser->left = size;
  johab_parser->marked_left = 0;
  johab_parser->is_eos = 0;
}

static void johab_parser_delete(ef_parser_t *s) { free(s); }

/* --- global functions --- */

ef_parser_t *ef_johab_parser_new(void) {
  ef_parser_t *johab_parser;

  if ((johab_parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(johab_parser);

  johab_parser->init = ef_parser_init;
  johab_parser->next_char = johab_parser_next_char;
  johab_parser->set_str = johab_parser_set_str;
  johab_parser->delete = johab_parser_delete;

  return johab_parser;
}
