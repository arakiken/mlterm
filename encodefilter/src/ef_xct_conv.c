/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_xct_conv.h"

#include <string.h> /* strcasecmp */
#include <pobl/bl_mem.h>
#include <pobl/bl_locale.h> /* bl_get_codeset() */
#include <pobl/bl_debug.h>

#include "ef_locale_ucs4_map.h"
#include "ef_ucs4_map.h"
#include "ef_zh_tw_map.h"
#include "ef_zh_cn_map.h"
#include "ef_iso2022_conv.h"
#include "ef_iso2022_intern.h"

/* --- static functions --- */

static void remap_unsupported_charset(ef_char_t *ch) {
  ef_char_t c;

  if (ch->cs == ISO10646_UCS4_1) {
    if (!ef_map_locale_ucs4_to(&c, ch)) {
      return;
    }

    *ch = c;
  }

  if (strcasecmp(bl_get_codeset(), "GBK") == 0) {
    if (ch->cs == GB2312_80) {
      if (ef_map_gb2312_80_to_gbk(&c, ch)) {
        *ch = c;
      }

      return;
    } else if (ch->cs == GBK) {
      return;
    }
  } else {
    if (ch->cs == GBK) {
      if (ef_map_gbk_to_gb2312_80(&c, ch)) {
        *ch = c;
      }

      return;
    } else if (ch->cs == GB2312_80) {
      return;
    }
  }

  if (strcasecmp(bl_get_codeset(), "BIG5") == 0 || strcasecmp(bl_get_codeset(), "BIG5HKSCS") == 0) {
    if (ch->cs == CNS11643_1992_1) {
      if (ef_map_cns11643_1992_1_to_big5(&c, ch)) {
        *ch = c;
      }

      return;
    } else if (ch->cs == CNS11643_1992_2) {
      if (ef_map_cns11643_1992_2_to_big5(&c, ch)) {
        *ch = c;
      }

      return;
    } else if (ch->cs == BIG5) {
      return;
    }
  } else {
    if (ch->cs == BIG5) {
      if (ef_map_big5_to_cns11643_1992(&c, ch)) {
        *ch = c;
      }

      return;
    } else if (ch->cs == CNS11643_1992_1 || ch->cs == CNS11643_1992_2) {
      return;
    }
  }

  ef_iso2022_remap_unsupported_charset(ch);
}

static size_t convert_to_xct_intern(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                    ef_parser_t *parser, int big5_buggy) {
  size_t filled_size;
  ef_char_t ch;
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  filled_size = 0;
  while (ef_parser_next_char(parser, &ch)) {
    int count;

    remap_unsupported_charset(&ch);

    if (IS_CS94SB(ch.cs) || IS_CS94MB(ch.cs)) {
      if (ch.cs != iso2022_conv->g0) {
        if (IS_CS94SB(ch.cs)) {
          if (filled_size + ch.size + 2 >= dst_size) {
            ef_parser_full_reset(parser);

            return filled_size;
          }

          *(dst++) = '\x1b';
          *(dst++) = '(';
          *(dst++) = CS94SB_FT(ch.cs);

          filled_size += 3;
        } else {
          if (filled_size + ch.size + 3 >= dst_size) {
            ef_parser_full_reset(parser);

            return filled_size;
          }

          *(dst++) = '\x1b';
          *(dst++) = '$';
          *(dst++) = '(';
          *(dst++) = CS94MB_FT(ch.cs);

          filled_size += 4;
        }

        iso2022_conv->g0 = ch.cs;
      } else {
        if (filled_size + ch.size - 1 >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }
      }

      for (count = 0; count < ch.size; count++) {
        *(dst++) = ch.ch[count];
      }

      filled_size += ch.size;
    } else if (IS_CS96SB(ch.cs) || IS_CS96MB(ch.cs)) {
      if (ch.cs != iso2022_conv->g1) {
        if (IS_CS96SB(ch.cs)) {
          if (filled_size + ch.size + 2 >= dst_size) {
            ef_parser_full_reset(parser);

            return filled_size;
          }

          *(dst++) = '\x1b';
          *(dst++) = '-';
          *(dst++) = CS96SB_FT(ch.cs);

          filled_size += 3;
        } else {
          if (filled_size + ch.size + 3 >= dst_size) {
            ef_parser_full_reset(parser);

            return filled_size;
          }

          *(dst++) = '\x1b';
          *(dst++) = '$';
          *(dst++) = '-';
          *(dst++) = CS96MB_FT(ch.cs);

          filled_size += 4;
        }

        iso2022_conv->g1 = ch.cs;
      } else {
        if (filled_size + ch.size - 1 >= dst_size) {
          ef_parser_full_reset(parser);

          return filled_size;
        }
      }

      for (count = 0; count < ch.size; count++) {
        *(dst++) = MAP_TO_GR(ch.ch[count]);
      }

      filled_size += ch.size;
    }
    /*
     * Non-Standard Character Sets
     */
    else if (ch.cs == BIG5 || ch.cs == HKSCS || ch.cs == GBK) {
      char *prefix;

      if (ch.cs == BIG5 || ch.cs == HKSCS) {
        /*
         * !! Notice !!
         * Big5 CTEXT implementation of XFree86 4.1.0 or before is very BUGGY!
         */
        if (big5_buggy) {
          prefix =
              "\x1b\x25\x2f\x32\x80\x89"
              "BIG5-0"
              "\x02\x80\x89"
              "BIG5-0"
              "\x02";

          iso2022_conv->g0 = BIG5;
          iso2022_conv->g1 = BIG5;
        } else {
          prefix =
              "\x1b\x25\x2f\x32\x80\x89"
              "big5-0"
              "\x02";
        }
      } else /* if( ch.cs == GBK) */
      {
        prefix =
            "\x1b\x25\x2f\x32\x80\x88"
            "gbk-0"
            "\x02";
      }

      if (filled_size + strlen(prefix) + ch.size > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      memcpy(dst, prefix, strlen(prefix));
      dst += strlen(prefix);

      *(dst++) = ch.ch[0];
      *(dst++) = ch.ch[1];

      filled_size += (strlen(prefix) + 2);
    } else if (IS_ISCII(ch.cs) || ch.cs == KOI8_R || ch.cs == KOI8_U || ch.cs == VISCII) {
      char *prefix;

      if (IS_ISCII(ch.cs)) {
        prefix =
            "\x1b\x25\x2f\x31\x80\x8b"
            "iscii-dev"
            "\x02";
      } else if (ch.cs == KOI8_R) {
        prefix =
            "\x1b\x25\x2f\x31\x80\x88"
            "koi8-r"
            "\x02";
      } else if (ch.cs == KOI8_U) {
        prefix =
            "\x1b\x25\x2f\x31\x80\x88"
            "koi8-u"
            "\x02";
      } else /* if( ch.cs == VISCII) */
      {
        prefix =
            "\x1b\x25\x2f\x31\x80\x8d"
            "viscii1.1-1"
            "\x02";
      }

      if (filled_size + strlen(prefix) + ch.size > dst_size) {
        ef_parser_full_reset(parser);

        return filled_size;
      }

      memcpy(dst, prefix, strlen(prefix));
      dst += strlen(prefix);

      *(dst++) = ch.ch[0];

      filled_size += (strlen(prefix) + 1);
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

static size_t convert_to_xct(ef_conv_t *conv, u_char *dst, size_t dst_size, ef_parser_t *parser) {
  return convert_to_xct_intern(conv, dst, dst_size, parser, 0);
}

static size_t convert_to_xct_big5_buggy(ef_conv_t *conv, u_char *dst, size_t dst_size,
                                        ef_parser_t *parser) {
  return convert_to_xct_intern(conv, dst, dst_size, parser, 1);
}

static void xct_conv_init(ef_conv_t *conv) {
  ef_iso2022_conv_t *iso2022_conv;

  iso2022_conv = (ef_iso2022_conv_t*)conv;

  iso2022_conv->gl = &iso2022_conv->g0;
  iso2022_conv->gr = &iso2022_conv->g1;
  iso2022_conv->g0 = US_ASCII;
  iso2022_conv->g1 = ISO8859_1_R;
  iso2022_conv->g2 = UNKNOWN_CS;
  iso2022_conv->g3 = UNKNOWN_CS;
}

static void conv_destroy(ef_conv_t *conv) { free(conv); }

/* --- global functions --- */

ef_conv_t *ef_xct_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  xct_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_xct;
  iso2022_conv->conv.init = xct_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}

ef_conv_t *ef_xct_big5_buggy_conv_new(void) {
  ef_iso2022_conv_t *iso2022_conv;

  if ((iso2022_conv = malloc(sizeof(ef_iso2022_conv_t))) == NULL) {
    return NULL;
  }

  xct_conv_init((ef_conv_t*)iso2022_conv);

  iso2022_conv->conv.convert = convert_to_xct_big5_buggy;
  iso2022_conv->conv.init = xct_conv_init;
  iso2022_conv->conv.destroy = conv_destroy;
  iso2022_conv->conv.illegal_char = NULL;

  return (ef_conv_t*)iso2022_conv;
}
