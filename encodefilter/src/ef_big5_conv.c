/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_big5_conv.h"

#include <string.h> /* strncmp */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_locale.h>
#include <pobl/bl_util.h> /* K_MIN */

#include "ef_zh_tw_map.h"
#include "ef_zh_hk_map.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t* ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    char* locale;

    locale = bl_get_locale();

    if (strncmp(locale, "zh_HK", 5) == 0) {
      if (!ef_map_ucs4_to_zh_hk(&c, ch)) {
        return;
      }
    } else {
      if (!ef_map_ucs4_to_zh_tw(&c, ch)) {
        return;
      }
    }

    *ch = c;
  }

  if (ch->cs == CNS11643_1992_1) {
    if (ef_map_cns11643_1992_1_to_big5(&c, ch)) {
      *ch = c;
    }
  } else if (ch->cs == CNS11643_1992_2) {
    if (ef_map_cns11643_1992_2_to_big5(&c, ch)) {
      *ch = c;
    }
  }
}

static size_t convert_to_big5(ef_conv_t* conv, u_char* dst, size_t dst_size,
                              ef_parser_t* parser) {
  size_t filled_size;
  ef_char_t ch;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    remap_unsupported_charset(&ch);

    if (ch.cs == BIG5 || ch.cs == HKSCS) {
      if (filled_size + 1 >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];
      *(dst++) = ch.ch[1];

      filled_size += 2;
    } else if (ch.cs == US_ASCII) {
      if (filled_size >= dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      *(dst++) = ch.ch[0];

      filled_size++;
    } else if (conv->illegal_char) {
      size_t size;
      int is_full;

      size = (*conv->illegal_char)(conv, dst, dst_size - filled_size, &is_full, &ch);
      if (is_full) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      dst += size;
      filled_size += size;
    }
  }

  return filled_size;
}

static void conv_init(ef_conv_t* conv) {}

static void conv_delete(ef_conv_t* conv) { free(conv); }

/* --- global functions --- */

ef_conv_t* ef_big5_conv_new(void) {
  ef_conv_t* conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_big5;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}

ef_conv_t* ef_big5hkscs_conv_new(void) {
  ef_conv_t* conv;

  if ((conv = malloc(sizeof(ef_conv_t))) == NULL) {
    return NULL;
  }

  conv->convert = convert_to_big5;
  conv->init = conv_init;
  conv->delete = conv_delete;
  conv->illegal_char = NULL;

  return conv;
}
