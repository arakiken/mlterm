/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_str_parser.h"

#include <string.h> /* memcpy */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "vt_char_encoding.h" /* vt_is_msb_set */
#include "vt_drcs.h"

typedef struct vt_str_parser {
  ef_parser_t parser;

  /*
   * !! Notice !!
   * ef_parser_reset() and ef_parser_mark() don't recognize these members.
   */
  vt_char_t* str;
  u_int left;
  u_int comb_left;

} vt_str_parser_t;

/* --- static functions --- */

static int next_char(ef_parser_t* parser, ef_char_t* ch) {
  vt_str_parser_t* vt_str_parser;
  vt_char_t* vt_ch;
  u_int comb_size;

  vt_str_parser = (vt_str_parser_t*)parser;

  /* hack for ef_parser_reset */
  vt_str_parser->str -= (parser->left - vt_str_parser->left);
  vt_str_parser->left = parser->left;

  while (1) {
    if (vt_str_parser->parser.is_eos) {
      goto err;
    }

    ef_parser_mark(parser);

    /*
     * skipping NULL
     */
    if (!vt_char_is_null(vt_str_parser->str)) {
      break;
    }

    vt_str_parser->left--;
    vt_str_parser->str++;

    if (vt_str_parser->left == 0) {
      vt_str_parser->parser.is_eos = 1;
    }
  }

  vt_ch = vt_str_parser->str;

  if (vt_str_parser->comb_left > 0) {
    vt_char_t* combs;

    if ((combs = vt_get_combining_chars(vt_ch, &comb_size)) == NULL ||
        comb_size < vt_str_parser->comb_left) {
      /* strange ! */

      vt_str_parser->comb_left = 0;

      goto err;
    }

    vt_ch = &combs[comb_size - vt_str_parser->comb_left];

    if (--vt_str_parser->comb_left == 0) {
      vt_str_parser->left--;
      vt_str_parser->str++;
    }
  } else {
    if (vt_get_combining_chars(vt_ch, &comb_size)) {
      vt_str_parser->comb_left = comb_size;
    } else {
      vt_str_parser->left--;
      vt_str_parser->str++;
    }
  }

  ch->cs = vt_char_cs(vt_ch);
  ch->size = CS_SIZE(ch->cs);

  ef_int_to_bytes(ch->ch, ch->size, vt_char_code(vt_ch));

/*
 * Android doesn't support PUA as follows. (tested on Android 4.0)
 *
 * e.g.) UTF8:0x4f88819a (U+10805a)
 * W/dalvikvm( 4527): JNI WARNING: input is not valid Modified UTF-8: illegal
 *start byte 0xf4
 * I/dalvikvm( 4527):   at dalvik.system.NativeStart.run(Native Method)
 * E/dalvikvm( 4527): VM aborting
 */
#ifndef __ANDROID__
  if (!vt_convert_drcs_to_unicode_pua(ch))
#endif
  {
    /* XXX */
    ch->property = 0;

    if (vt_is_msb_set(ch->cs)) {
      UNSET_MSB(ch->ch[0]);
    }
  }

  if (vt_str_parser->left == 0) {
    vt_str_parser->parser.is_eos = 1;
  }

  /* hack for ef_parser_reset */
  parser->left = vt_str_parser->left;

  if (vt_char_is_null(vt_ch)) {
    return next_char(parser, ch);
  }

  return 1;

err:
  /* hack for ef_parser_reset */
  parser->left = vt_str_parser->left;

  return 0;
}

static void init(ef_parser_t* ef_parser) {
  vt_str_parser_t* vt_str_parser;

  vt_str_parser = (vt_str_parser_t*)ef_parser;

  ef_parser_init(ef_parser);

  vt_str_parser->str = NULL;
  vt_str_parser->left = 0;
  vt_str_parser->comb_left = 0;
}

static void set_str(ef_parser_t* ef_parser, u_char* str, size_t size) { /* do nothing */ }

static void delete (ef_parser_t* s) { free(s); }

/* --- global functions --- */

ef_parser_t* vt_str_parser_new(void) {
  vt_str_parser_t* vt_str_parser;

  if ((vt_str_parser = malloc(sizeof(vt_str_parser_t))) == NULL) {
    return NULL;
  }

  init((ef_parser_t*)vt_str_parser);

  vt_str_parser->parser.init = init;
  vt_str_parser->parser.set_str = set_str;
  vt_str_parser->parser.delete = delete;
  vt_str_parser->parser.next_char = next_char;

  return (ef_parser_t*)vt_str_parser;
}

void vt_str_parser_set_str(ef_parser_t* ef_parser, vt_char_t* str, u_int size) {
  vt_str_parser_t* vt_str_parser;

  vt_str_parser = (vt_str_parser_t*)ef_parser;

  vt_str_parser->parser.is_eos = 0;
  vt_str_parser->parser.left = size;

  vt_str_parser->str = str;
  vt_str_parser->left = size;
}
