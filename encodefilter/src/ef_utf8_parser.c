/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_utf8_parser.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_ucs_property.h"

/* --- static functions --- */

static int utf8_parser_next_char(ef_parser_t *utf8_parser, ef_char_t *ucs4_ch) {
  u_char *utf8_ch;
  u_int32_t ucs4_int;
  size_t bytes;

  if (utf8_parser->is_eos) {
    return 0;
  }

  ef_parser_mark(utf8_parser);

  utf8_ch = utf8_parser->str;

  if ((utf8_ch[0] & 0xc0) == 0x80) {
    goto utf8_err;
  } else if ((utf8_ch[0] & 0x80) == 0) {
    /* 0x00 - 0x7f */

    ucs4_ch->ch[0] = utf8_ch[0];
    ef_parser_n_increment(utf8_parser, 1);

    ucs4_ch->size = 1;
    ucs4_ch->cs = US_ASCII;
    ucs4_ch->property = 0;

    return 1;
  } else if ((utf8_ch[0] & 0xe0) == 0xc0) {
    bytes = 2;
    if (utf8_parser->left < bytes) {
      utf8_parser->is_eos = 1;

      return 0;
    }

    if (utf8_ch[1] < 0x80) {
      goto utf8_err;
    }

    ucs4_int = (((utf8_ch[0] & 0x1f) << 6) & 0xffffffc0) | (utf8_ch[1] & 0x3f);

    if (ucs4_int < 0x80) {
      goto utf8_err;
    }
  } else if ((utf8_ch[0] & 0xf0) == 0xe0) {
    bytes = 3;
    if (utf8_parser->left < bytes) {
      utf8_parser->is_eos = 1;

      return 0;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80) {
      goto utf8_err;
    }

    ucs4_int = (((utf8_ch[0] & 0x0f) << 12) & 0xffff000) |
               (((utf8_ch[1] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[2] & 0x3f);

    if (ucs4_int < 0x800) {
      goto utf8_err;
    }
  } else if ((utf8_ch[0] & 0xf8) == 0xf0) {
    bytes = 4;
    if (utf8_parser->left < bytes) {
      utf8_parser->is_eos = 1;

      return 0;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80) {
      goto utf8_err;
    }

    ucs4_int = (((utf8_ch[0] & 0x07) << 18) & 0xfffc0000) |
               (((utf8_ch[1] & 0x3f) << 12) & 0xffff000) |
               (((utf8_ch[2] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[3] & 0x3f);

    if (ucs4_int < 0x10000) {
      goto utf8_err;
    }
  } else if ((utf8_ch[0] & 0xfc) == 0xf8) {
    bytes = 5;
    if (utf8_parser->left < bytes) {
      utf8_parser->is_eos = 1;

      return 0;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80 || utf8_ch[4] < 0x80) {
      goto utf8_err;
    }

    ucs4_int = (((utf8_ch[0] & 0x03) << 24) & 0xff000000) |
               (((utf8_ch[1] & 0x3f) << 18) & 0xfffc0000) |
               (((utf8_ch[2] & 0x3f) << 12) & 0xffff000) |
               (((utf8_ch[3] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[4] & 0x3f);

    if (ucs4_int < 0x200000) {
      goto utf8_err;
    }
  } else if ((utf8_ch[0] & 0xfe) == 0xfc) {
    bytes = 6;
    if (utf8_parser->left < bytes) {
      utf8_parser->is_eos = 1;

      return 0;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80 || utf8_ch[4] < 0x80 ||
        utf8_ch[5] < 0x80) {
      goto utf8_err;
    }

    ucs4_int =
        (((utf8_ch[0] & 0x01 << 30) & 0xc0000000)) | (((utf8_ch[1] & 0x3f) << 24) & 0xff000000) |
        (((utf8_ch[2] & 0x3f) << 18) & 0xfffc0000) | (((utf8_ch[3] & 0x3f) << 12) & 0xffff000) |
        (((utf8_ch[4] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[4] & 0x3f);

    if (ucs4_int < 0x4000000) {
      goto utf8_err;
    }
  } else {
    goto utf8_err;
  }

  ef_int_to_bytes(ucs4_ch->ch, 4, ucs4_int);

  ef_parser_n_increment(utf8_parser, bytes);

  ucs4_ch->size = 4;
  ucs4_ch->cs = ISO10646_UCS4_1;
  ucs4_ch->property = ef_get_ucs_property(ucs4_int);

  return 1;

utf8_err:
#ifdef DEBUG
  bl_warn_printf(BL_DEBUG_TAG " illegal utf8 sequence [0x%.2x ...].\n", utf8_ch[0]);
#endif

  ef_parser_reset(utf8_parser);

  return 0;
}

static void utf8_parser_set_str(ef_parser_t *utf8_parser, u_char *str, size_t size) {
  utf8_parser->str = str;
  utf8_parser->left = size;
  utf8_parser->marked_left = 0;
  utf8_parser->is_eos = 0;
}

static void utf8_parser_delete(ef_parser_t *s) { free(s); }

/* --- global functions --- */

ef_parser_t *ef_utf8_parser_new(void) {
  ef_parser_t *utf8_parser;

  if ((utf8_parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(utf8_parser);

  utf8_parser->init = ef_parser_init;
  utf8_parser->set_str = utf8_parser_set_str;
  utf8_parser->delete = utf8_parser_delete;
  utf8_parser->next_char = utf8_parser_next_char;

  return utf8_parser;
}
