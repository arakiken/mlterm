/*
 *	$Id$
 */

#include "ef_utf16_parser.h"

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>

#include "ef_ucs_property.h"

typedef struct ef_utf16_parser {
  ef_parser_t parser;
  int is_big_endian;

} ef_utf16_parser_t;

/* --- static functions --- */

static void utf16_parser_init(ef_parser_t* parser) {
  ef_parser_init(parser);

  ((ef_utf16_parser_t*)parser)->is_big_endian = 1;
}

static void utf16le_parser_init(ef_parser_t* parser) {
  ef_parser_init(parser);

  ((ef_utf16_parser_t*)parser)->is_big_endian = 0;
}

static void utf16_parser_set_str(ef_parser_t* parser, u_char* str, size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

static void utf16_parser_delete(ef_parser_t* parser) { free(parser); }

static int utf16_parser_next_char(ef_parser_t* parser, ef_char_t* ucs4_ch) {
  ef_utf16_parser_t* utf16_parser;

  if (parser->is_eos) {
    return 0;
  }

  ef_parser_mark(parser);

  if (parser->left < 2) {
    parser->is_eos = 1;

    return 0;
  }

  utf16_parser = (ef_utf16_parser_t*)parser;

  if (memcmp(parser->str, "\xfe\xff", 2) == 0) {
    utf16_parser->is_big_endian = 1;

    ef_parser_n_increment(parser, 2);

    return utf16_parser_next_char(parser, ucs4_ch);
  } else if (memcmp(parser->str, "\xff\xfe", 2) == 0) {
    utf16_parser->is_big_endian = 0;

    ef_parser_n_increment(parser, 2);

    return utf16_parser_next_char(parser, ucs4_ch);
  } else {
    u_char ch[2];
    u_int32_t ucs4;

    if (utf16_parser->is_big_endian) {
      ch[0] = parser->str[0];
      ch[1] = parser->str[1];
    } else {
      ch[0] = parser->str[1];
      ch[1] = parser->str[0];
    }

    if (0xd8 <= ch[0] && ch[0] <= 0xdb) {
      /* surrogate pair */

      u_char ch2[2];

      if (parser->left < 4) {
        parser->is_eos = 1;

        return 0;
      }

      if (utf16_parser->is_big_endian) {
        ch2[0] = parser->str[2];
        ch2[1] = parser->str[3];
      } else {
        ch2[0] = parser->str[3];
        ch2[1] = parser->str[2];
      }

      if (ch2[0] < 0xdc || 0xdf < ch2[0]) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " illegal UTF-16 surrogate-pair format.\n");
#endif

        goto error;
      }

      ucs4 = ((ch[0] - 0xd8) * 0x100 * 0x400 + ch[1] * 0x400 + (ch2[0] - 0xdc) * 0x100 + ch2[1]) +
             0x10000;

#ifdef DEBUG
      if (ucs4 < 0x10000 || 0x10ffff < ucs4) {
        bl_warn_printf(BL_DEBUG_TAG " illegal UTF-16 surrogate-pair format.\n");

        goto error;
      }
#endif

      ef_int_to_bytes(ucs4_ch->ch, 4, ucs4);

      ef_parser_n_increment(parser, 4);
    } else {
      ef_parser_n_increment(parser, 2);

      if ((ucs4 = ef_bytes_to_int(ch, 2)) <= 0x7f) {
        ucs4_ch->ch[0] = ucs4;
        ucs4_ch->cs = US_ASCII;
        ucs4_ch->size = 1;
        ucs4_ch->property = 0;

        return 1;
      }

      ucs4_ch->ch[0] = 0x0;
      ucs4_ch->ch[1] = 0x0;
      ucs4_ch->ch[2] = ch[0];
      ucs4_ch->ch[3] = ch[1];
    }

    ucs4_ch->cs = ISO10646_UCS4_1;
    ucs4_ch->size = 4;
    ucs4_ch->property = ef_get_ucs_property(ucs4);

    return 1;
  }

error:
  ef_parser_reset(parser);

  return 0;
}

/* --- global functions --- */

ef_parser_t* ef_utf16_parser_new(void) {
  ef_utf16_parser_t* utf16_parser;

  if ((utf16_parser = malloc(sizeof(ef_utf16_parser_t))) == NULL) {
    return NULL;
  }

  utf16_parser_init((ef_parser_t*)utf16_parser);

  utf16_parser->parser.init = utf16_parser_init;
  utf16_parser->parser.set_str = utf16_parser_set_str;
  utf16_parser->parser.delete = utf16_parser_delete;
  utf16_parser->parser.next_char = utf16_parser_next_char;

  return (ef_parser_t*)utf16_parser;
}

ef_parser_t* ef_utf16le_parser_new(void) {
  ef_utf16_parser_t* utf16_parser;

  if ((utf16_parser = malloc(sizeof(ef_utf16_parser_t))) == NULL) {
    return NULL;
  }

  utf16le_parser_init((ef_parser_t*)utf16_parser);

  utf16_parser->parser.init = utf16le_parser_init;
  utf16_parser->parser.set_str = utf16_parser_set_str;
  utf16_parser->parser.delete = utf16_parser_delete;
  utf16_parser->parser.next_char = utf16_parser_next_char;

  return (ef_parser_t*)utf16_parser;
}
