/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_iso2022_parser.h"

#include <string.h> /* strncmp/memset */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "ef_iso2022_intern.h"
#include "ef_jisx0208_1983_property.h"
#include "ef_jisx0213_2000_property.h"

#define IS_C0(c) ((u_char)c <= 0x1f)
#define IS_C1(c) (0x80 <= (u_char)c && (u_char)c <= 0x9f)
#define IS_GL(c) (0x20 <= (u_char)c && (u_char)c <= 0x7f) /* msb is 0 */
#define IS_GR(c) (0xa0 <= (u_char)c && (u_char)c <= 0xff) /* msb is 1 */
#define IS_INTERMEDIATE(c) (0x20 <= (u_char)c && (u_char)c <= 0x2f)
#define IS_FT(c) ((0x30 <= ((u_char)c) && ((u_char)c) <= 0x7e))
#define IS_ESCAPE(c) ((((u_char)c) & 0x7e) == 0x0e || ((u_char)c) == 0x1b)

/* --- static functions --- */

inline static size_t get_cs_bytelen(ef_charset_t cs) {
  if (IS_CS94SB(cs) || IS_CS96SB(cs)) {
    return 1;
  } else if (IS_CS94MB(cs) || IS_CS96MB(cs)) {
    if (cs == CNS11643_1992_EUCTW_G2) {
      return 3;
    } else {
      /*
       * XXX
       * there may be more exceptions ...
       */

      return 2;
    }
  } else if (cs == ISO10646_UCS2_1) {
    return 2;
  } else if (cs == ISO10646_UCS4_1) {
    return 4;
  }
  /*
   * for XCTEXT extensions.
   */
  else if (cs == BIG5 || cs == GBK) {
    return 2;
  } else if (IS_ISCII(cs) || cs == KOI8_R || cs == KOI8_U || cs == VISCII) {
    return 1;
  }

  return 0;
}

inline static ef_charset_t get_charset(u_char ft,                 /* 0x30 - 0x7f */
                                        int is_mb, int glyph_size, /* 94 or 96 */
                                        int rev) {
  ef_charset_t cs;

  if (glyph_size == 94) {
    if (is_mb) {
      cs = CS94MB_ID(ft);
    } else {
      cs = CS94SB_ID(ft);
    }
  } else if (glyph_size == 96) {
    if (is_mb) {
      cs = CS96MB_ID(ft);
    } else {
      cs = CS96SB_ID(ft);
    }
  } else {
    return UNKNOWN_CS;
  }

  if (rev == 0) {
    return cs;
  } else if (rev == 1) {
    return CS_REVISION_1(cs);
  } else {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " unsupported charset revision.\n");
#endif

    return UNKNOWN_CS;
  }
}

static int parse_escape(ef_iso2022_parser_t *iso2022_parser,
                        ef_char_t *ch /* if single shifted ch->cs is set ,
                                          otherwise this is not touched. */
                        ) {
  ef_parser_mark(iso2022_parser);

  if (*iso2022_parser->parser.str == SS2) {
    ch->cs = iso2022_parser->g2;
    iso2022_parser->is_single_shifted = 1;
  } else if (*iso2022_parser->parser.str == SS3) {
    ch->cs = iso2022_parser->g3;
    iso2022_parser->is_single_shifted = 1;
  } else if (*iso2022_parser->parser.str == LS0) {
    iso2022_parser->gl = &iso2022_parser->g0;
  } else if (*iso2022_parser->parser.str == LS1) {
#ifdef DECSP_HACK
    static ef_charset_t decsp = DEC_SPECIAL;

    if (iso2022_parser->g1_is_decsp) {
      iso2022_parser->gl = &decsp;
    } else
#endif
    {
      iso2022_parser->gl = &iso2022_parser->g1;
    }
  } else if (*iso2022_parser->parser.str == ESC) {
    if (ef_parser_increment(iso2022_parser) == 0) {
      /* we reach eos */

      goto reset;
    }

    if (*iso2022_parser->parser.str == SS2_7) {
      ch->cs = iso2022_parser->g2;
      iso2022_parser->is_single_shifted = 1;
    } else if (*iso2022_parser->parser.str == SS3_7) {
      ch->cs = iso2022_parser->g3;
      iso2022_parser->is_single_shifted = 1;
    } else {
      if (*iso2022_parser->parser.str == LS2) {
        iso2022_parser->gl = &iso2022_parser->g2;
      } else if (*iso2022_parser->parser.str == LS3) {
        iso2022_parser->gl = &iso2022_parser->g3;
      } else if (*iso2022_parser->parser.str == LS1R) {
        iso2022_parser->gr = &iso2022_parser->g1;
      } else if (*iso2022_parser->parser.str == LS2R) {
        iso2022_parser->gr = &iso2022_parser->g2;
      } else if (*iso2022_parser->parser.str == LS3R) {
        iso2022_parser->gr = &iso2022_parser->g3;
      } else if (*iso2022_parser->parser.str == NON_ISO2022_CS) {
        int is_class_2;
        ef_charset_t cs;

        if (ef_parser_increment(iso2022_parser) == 0) {
          /* we reach eos */

          goto reset;
        }

        if (*iso2022_parser->parser.str == NON_ISO2022_CS_2) {
          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos. */

            goto reset;
          }

          is_class_2 = 1;
        } else {
          is_class_2 = 0;
        }

        if (!IS_FT(*iso2022_parser->parser.str)) {
#ifdef DEBUG
          bl_warn_printf(BL_DEBUG_TAG " illegal ft (ESC - % - I - %x)\n",
                         *iso2022_parser->parser.str);
#endif

          ef_parser_increment(iso2022_parser);

          return 0;
        }

        if (is_class_2) {
          cs = NON_ISO2022_2_ID(*iso2022_parser->parser.str);
        } else {
          cs = NON_ISO2022_1_ID(*iso2022_parser->parser.str);
        }

        if (ef_parser_increment(iso2022_parser) == 0) {
          /* we reach eos */

          goto reset;
        }

        if (iso2022_parser->non_iso2022_is_started) {
          iso2022_parser->non_iso2022_cs = cs;

          return (*iso2022_parser->non_iso2022_is_started)(iso2022_parser);
        } else {
          /* ignored */
        }
      } else if (IS_INTERMEDIATE(*iso2022_parser->parser.str)) {
        int is_mb;
        int rev;
        u_char to_GN;
        u_char ft;

        if (*iso2022_parser->parser.str == CS_REV) {
          /* ESC - 2/6 - Ft ESC - I - Ft */

          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos */

            goto reset;
          }

          if (REV_NUM(*iso2022_parser->parser.str) == 1) {
            rev = 1;
          } else {
#ifdef DEBUG
            bl_warn_printf(BL_DEBUG_TAG " charset revisions except 1 is not supported.\n");
#endif

            ef_parser_increment(iso2022_parser);

            return 0;
          }

          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos. */

            goto reset;
          }

          if (*iso2022_parser->parser.str != ESC) {
#ifdef DEBUG
            bl_warn_printf(BL_DEBUG_TAG " ESC & Ft should follow ESC I Ft.\n");
#endif

            ef_parser_increment(iso2022_parser);

            return 0;
          }

          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos. */

            goto reset;
          }
        } else {
          rev = 0;
        }

        if (*iso2022_parser->parser.str == MB_CS) {
          is_mb = 1;

          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos */

            goto reset;
          }
        } else {
          is_mb = 0;
        }

        if (is_mb && IS_FT(*iso2022_parser->parser.str)) {
          /* backward compatibility */

          to_GN = CS94_TO_G0;
          ft = *iso2022_parser->parser.str;
        } else {
          to_GN = *iso2022_parser->parser.str;

          if (ef_parser_increment(iso2022_parser) == 0) {
            /* we reach eos. */

            goto reset;
          }

          if (*iso2022_parser->parser.str == ' ') {
            /* is DRCS */

            if (ef_parser_increment(iso2022_parser) == 0) {
              /* we reach eos */

              goto reset;
            }
          }

          if (!IS_FT(*iso2022_parser->parser.str)) {
#ifdef DEBUG
            bl_warn_printf(BL_DEBUG_TAG " illegal ft(ESC - I - %x %x)\n", to_GN,
                           *iso2022_parser->parser.str);
#endif

            ef_parser_increment(iso2022_parser);

            return 0;
          }

          ft = *iso2022_parser->parser.str;
        }

        if (to_GN == CS94_TO_G0) {
          iso2022_parser->g0 = get_charset(ft, is_mb, 94, rev);
        } else if (to_GN == CS94_TO_G1) {
#ifdef DECSP_HACK
          if (ft == '0' && !is_mb) {
            iso2022_parser->g1_is_decsp = 1;
          } else
#endif
          {
#ifdef DECSP_HACK
            iso2022_parser->g1_is_decsp = 0;
#endif
            iso2022_parser->g1 = get_charset(ft, is_mb, 94, rev);
          }
        } else if (to_GN == CS94_TO_G2) {
          iso2022_parser->g2 = get_charset(ft, is_mb, 94, rev);
        } else if (to_GN == CS94_TO_G3) {
          iso2022_parser->g3 = get_charset(ft, is_mb, 94, rev);
        } else if (to_GN == CS96_TO_G1) {
#ifdef DECSP_HACK
          iso2022_parser->g1_is_decsp = 0;
#endif
          iso2022_parser->g1 = get_charset(ft, is_mb, 96, rev);
        } else if (to_GN == CS96_TO_G2) {
          iso2022_parser->g2 = get_charset(ft, is_mb, 96, rev);
        } else if (to_GN == CS96_TO_G3) {
          iso2022_parser->g3 = get_charset(ft, is_mb, 96, rev);
        } else {
#ifdef DEBUG
          bl_warn_printf(BL_DEBUG_TAG " illegal ISO2022 designation char %c\n", to_GN);
#endif

/* No increment in case VT100 sequence like "ESC # 8" */
#if 0
          ef_parser_increment(iso2022_parser);
#else
          ef_parser_reset(iso2022_parser);
#endif

          return 0;
        }
      } else {
        /* not ISO2022 intermediate char */

        goto reset;
      }
    }
  } else {
    /* error. this is not escape sequence. */

    goto reset;
  }

  ef_parser_increment(iso2022_parser);

  return 1;

reset:
  ef_parser_reset(iso2022_parser);

  return 0;
}

static int next_byte(ef_iso2022_parser_t *iso2022_parser, ef_char_t *ch) {
  if (iso2022_parser->parser.is_eos) {
    ef_parser_reset(iso2022_parser);

    ch->size = 0;

    return 0;
  } else if (IS_NON_ISO2022(iso2022_parser->non_iso2022_cs)) {
    if (iso2022_parser->next_non_iso2022_byte &&
        (*iso2022_parser->next_non_iso2022_byte)(iso2022_parser, ch)) {
      return 1;
    } else {
      iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

      return next_byte(iso2022_parser, ch);
    }
  } else if (IS_ESCAPE(*iso2022_parser->parser.str)) {
    if (!parse_escape(iso2022_parser, ch)) {
      return 0;
    }

    return next_byte(iso2022_parser, ch);
  } else if (iso2022_parser->is_single_shifted) {
    ch->ch[ch->size++] = UNMAP_FROM_GR(*iso2022_parser->parser.str);
  } else {
    if (IS_C0(*iso2022_parser->parser.str) || IS_C1(*iso2022_parser->parser.str)) {
      ch->ch[ch->size++] = *iso2022_parser->parser.str;
      ch->cs = US_ASCII;
    } else if (IS_GL(*(iso2022_parser->parser.str))) {
      if (!iso2022_parser->gl) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " gl is not set.\n");
#endif

        ef_parser_increment(iso2022_parser);

        return next_byte(iso2022_parser, ch);
      }

      ch->ch[ch->size++] = *iso2022_parser->parser.str;

      if ((IS_CS94SB(*iso2022_parser->gl) || IS_CS94MB(*iso2022_parser->gl)) &&
          (*iso2022_parser->parser.str == 0x20 || *iso2022_parser->parser.str == 0x7f)) {
        ch->cs = US_ASCII;
      } else {
        ch->cs = *iso2022_parser->gl;
      }
    } else {
      if (!iso2022_parser->gr) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " gr is not set.\n");
#endif

        ef_parser_increment(iso2022_parser);

        return next_byte(iso2022_parser, ch);
      }

      if ((IS_CS94SB(*iso2022_parser->gr) || IS_CS94MB(*iso2022_parser->gr)) &&
          (*iso2022_parser->parser.str == 0xa0 || *iso2022_parser->parser.str == 0xff)) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " 0xa0/0xff appears in 94CS. skipping...\n");
#endif

        ef_parser_increment(iso2022_parser);

        return next_byte(iso2022_parser, ch);
      } else {
        ch->ch[ch->size++] = UNMAP_FROM_GR(*iso2022_parser->parser.str);

        ch->cs = *iso2022_parser->gr;
      }
    }
  }

  ef_parser_increment(iso2022_parser);

  return 1;
}

static int sub_next_char(ef_iso2022_parser_t *iso2022_parser, ef_char_t *ch) {
  size_t bytelen;
  ef_charset_t cs;

  iso2022_parser->is_single_shifted = 0;
  while (1) {
    /* initialize */
    memset(ch, 0, sizeof(ef_char_t));

    ef_parser_mark(iso2022_parser);

    if (!next_byte(iso2022_parser, ch)) {
      return 0;
    }

    cs = ch->cs;

    if ((bytelen = get_cs_bytelen(cs)) > 0) {
      break;
    }
  }

  while (1) {
    if (ch->size > bytelen) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG
                      " Char size(%d) and char byte len(%d) of cs(%x) is illegal. This"
                      " may cause unexpected error. Parsing the sequence stopped.\n",
                      ch->size, bytelen, cs);
#endif

      return 0;
    } else if (ch->size == bytelen) {
#if 0
      if (ch->cs == JISX0208_1983 || ch->cs == JISC6226_1978) {
        /*
         * XXX
         * we should check gaiji here and replace JISX0208 or JISC6226_1978
         * by it.
         */
      }
#endif

      return 1;
    }

    if (!next_byte(iso2022_parser, ch)) {
      return 0;
    }

    if (cs != ch->cs) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG
                     " prev byte cs(%x) and cur byte cs(%x) are not the same , strange !"
                     " ignoring this char.\n",
                     cs, ch->cs);
#endif

      return sub_next_char(iso2022_parser, ch);
    }
  }
}

static void iso2022_parser_init(ef_parser_t *parser) {
  ef_iso2022_parser_t *iso2022_parser;

  ef_parser_init(parser);

  iso2022_parser = (ef_iso2022_parser_t *)parser;

  iso2022_parser->g0 = UNKNOWN_CS;
  iso2022_parser->g1 = UNKNOWN_CS;
  iso2022_parser->g2 = UNKNOWN_CS;
  iso2022_parser->g3 = UNKNOWN_CS;

  iso2022_parser->gl = NULL;
  iso2022_parser->gr = NULL;

  iso2022_parser->non_iso2022_cs = UNKNOWN_CS;

  iso2022_parser->is_single_shifted = 0;
#ifdef DECSP_HACK
  iso2022_parser->g1_is_decsp = 0;
#endif
}

/* --- global functions --- */

ef_iso2022_parser_t *ef_iso2022_parser_new(void) {
  ef_iso2022_parser_t *iso2022_parser;

  if ((iso2022_parser = malloc(sizeof(ef_iso2022_parser_t))) == NULL) {
    return NULL;
  }

  ef_iso2022_parser_init_func(iso2022_parser);

  iso2022_parser_init((ef_parser_t *)iso2022_parser);

  return iso2022_parser;
}

void ef_iso2022_parser_init_func(ef_iso2022_parser_t *iso2022_parser) {
  iso2022_parser->non_iso2022_is_started = NULL;
  iso2022_parser->next_non_iso2022_byte = NULL;

  iso2022_parser->parser.init = iso2022_parser_init;
  iso2022_parser->parser.set_str = ef_iso2022_parser_set_str;
  iso2022_parser->parser.delete = ef_iso2022_parser_delete;
  iso2022_parser->parser.next_char = ef_iso2022_parser_next_char;
}

void ef_iso2022_parser_set_str(ef_parser_t *parser, u_char *str, size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

void ef_iso2022_parser_delete(ef_parser_t *parser) { free(parser); }

int ef_iso2022_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  if (sub_next_char((ef_iso2022_parser_t *)parser, ch) == 0) {
    return 0;
  }

  if (ch->cs == JISX0208_1983) {
    ch->property = ef_get_jisx0208_1983_property(ch->ch, ch->size);
  } else if (ch->cs == JISX0213_2000_1) {
    ch->property = ef_get_jisx0213_2000_1_property(ch->ch, ch->size);
  } else if (ch->cs == TCVN5712_1_1993) {
    if (0x30 <= ch->ch[0] && ch->ch[0] <= 0x34) {
      ch->property = EF_COMBINING;
    }
  } else if (ch->cs == TIS620_2533) {
    if (ch->ch[0] == 0x51 || (0x54 <= ch->ch[0] && ch->ch[0] <= 0x5a) ||
        (0x67 <= ch->ch[0] && ch->ch[0] <= 0x6e)) {
      ch->property = EF_COMBINING;
    }
  } else {
    ch->property = 0;
  }

  return 1;
}
