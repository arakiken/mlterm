/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_char_encoding.h"

#include <stdio.h> /* sscanf */

#include <pobl/bl_str.h> /* bl_str_sep */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>    /* alloca */
#include <pobl/bl_locale.h> /* bl_get_codeset */

#include <mef/ef_iso8859_parser.h>
#include <mef/ef_8bit_parser.h>
#include <mef/ef_eucjp_parser.h>
#include <mef/ef_euckr_parser.h>
#include <mef/ef_euccn_parser.h>
#include <mef/ef_euctw_parser.h>
#include <mef/ef_iso2022jp_parser.h>
#include <mef/ef_iso2022kr_parser.h>
#include <mef/ef_iso2022cn_parser.h>
#include <mef/ef_sjis_parser.h>
#include <mef/ef_johab_parser.h>
#include <mef/ef_big5_parser.h>
#include <mef/ef_hz_parser.h>
#include <mef/ef_utf8_parser.h>

#include <mef/ef_iso8859_conv.h>
#include <mef/ef_8bit_conv.h>
#include <mef/ef_eucjp_conv.h>
#include <mef/ef_euckr_conv.h>
#include <mef/ef_euccn_conv.h>
#include <mef/ef_euctw_conv.h>
#include <mef/ef_iso2022jp_conv.h>
#include <mef/ef_iso2022kr_conv.h>
#include <mef/ef_iso2022cn_conv.h>
#include <mef/ef_sjis_conv.h>
#include <mef/ef_johab_conv.h>
#include <mef/ef_big5_conv.h>
#include <mef/ef_hz_conv.h>
#include <mef/ef_utf8_conv.h>

#include <mef/ef_iso2022_conv.h> /* ef_iso2022_illegal_char */

#include "vt_drcs.h"

typedef struct encoding_table {
  vt_char_encoding_t encoding;
  char *name;
  ef_parser_t *(*parser_new)(void);
  ef_conv_t *(*conv_new)(void);

} encoding_table_t;

/* --- static variables --- */

/*
 * !!! Notice !!!
 * The order should be the same as vt_char_encoding_t in vt_char_encoding.h
 * If the order is changed, x_font_manager.c:usascii_font_cs_table should be
 * also changed.
 */
static encoding_table_t encoding_table[] = {
  { VT_ISO8859_1, "ISO88591", ef_iso8859_1_parser_new, ef_iso8859_1_conv_new, },
  { VT_ISO8859_2, "ISO88592", ef_iso8859_2_parser_new, ef_iso8859_2_conv_new, },
  { VT_ISO8859_3, "ISO88593", ef_iso8859_3_parser_new, ef_iso8859_3_conv_new, },
  { VT_ISO8859_4, "ISO88594", ef_iso8859_4_parser_new, ef_iso8859_4_conv_new, },
  { VT_ISO8859_5, "ISO88595", ef_iso8859_5_parser_new, ef_iso8859_5_conv_new, },
  { VT_ISO8859_6, "ISO88596", ef_iso8859_6_parser_new, ef_iso8859_6_conv_new, },
  { VT_ISO8859_7, "ISO88597", ef_iso8859_7_parser_new, ef_iso8859_7_conv_new, },
  { VT_ISO8859_8, "ISO88598", ef_iso8859_8_parser_new, ef_iso8859_8_conv_new, },
  { VT_ISO8859_9, "ISO88599", ef_iso8859_9_parser_new, ef_iso8859_9_conv_new, },
  { VT_ISO8859_10, "ISO885910", ef_iso8859_10_parser_new, ef_iso8859_10_conv_new, },
  { VT_TIS620, "ISO885911", ef_tis620_2533_parser_new, ef_tis620_2533_conv_new, },
  { VT_ISO8859_13, "ISO885913", ef_iso8859_13_parser_new, ef_iso8859_13_conv_new, },
  { VT_ISO8859_14, "ISO885914", ef_iso8859_14_parser_new, ef_iso8859_14_conv_new, },
  { VT_ISO8859_15, "ISO885915", ef_iso8859_15_parser_new, ef_iso8859_15_conv_new, },
  { VT_ISO8859_16, "ISO885916", ef_iso8859_16_parser_new, ef_iso8859_16_conv_new, },
  { VT_TCVN5712, "TCVN5712", ef_tcvn5712_3_1993_parser_new, ef_tcvn5712_3_1993_conv_new, },

  { VT_ISCII_ASSAMESE, "ISCIIASSAMESE", ef_iscii_assamese_parser_new, ef_iscii_assamese_conv_new, },
  { VT_ISCII_BENGALI, "ISCIIBENGALI", ef_iscii_bengali_parser_new, ef_iscii_bengali_conv_new, },
  { VT_ISCII_GUJARATI, "ISCIIGUJARATI", ef_iscii_gujarati_parser_new, ef_iscii_gujarati_conv_new, },
  { VT_ISCII_HINDI, "ISCIIHINDI", ef_iscii_hindi_parser_new, ef_iscii_hindi_conv_new, },
  { VT_ISCII_KANNADA, "ISCIIKANNADA", ef_iscii_kannada_parser_new, ef_iscii_kannada_conv_new, },
  { VT_ISCII_MALAYALAM, "ISCIIMALAYALAM", ef_iscii_malayalam_parser_new,
    ef_iscii_malayalam_conv_new, },
  { VT_ISCII_ORIYA, "ISCIIORIYA", ef_iscii_oriya_parser_new, ef_iscii_oriya_conv_new, },
  { VT_ISCII_PUNJABI, "ISCIIPUNJABI", ef_iscii_punjabi_parser_new, ef_iscii_punjabi_conv_new, },
  { VT_ISCII_TELUGU, "ISCIITELUGU", ef_iscii_telugu_parser_new, ef_iscii_telugu_conv_new, },
  { VT_VISCII, "VISCII", ef_viscii_parser_new, ef_viscii_conv_new, },
  { VT_KOI8_R, "KOI8R", ef_koi8_r_parser_new, ef_koi8_r_conv_new, },
  { VT_KOI8_U, "KOI8U", ef_koi8_u_parser_new, ef_koi8_u_conv_new, },
  { VT_KOI8_T, "KOI8T", ef_koi8_t_parser_new, ef_koi8_t_conv_new, },
  { VT_GEORGIAN_PS, "GEORGIANPS", ef_georgian_ps_parser_new, ef_georgian_ps_conv_new, },
  { VT_CP1250, "CP1250", ef_cp1250_parser_new, ef_cp1250_conv_new, },
  { VT_CP1251, "CP1251", ef_cp1251_parser_new, ef_cp1251_conv_new, },
  { VT_CP1252, "CP1252", ef_cp1252_parser_new, ef_cp1252_conv_new, },
  { VT_CP1253, "CP1253", ef_cp1253_parser_new, ef_cp1253_conv_new, },
  { VT_CP1254, "CP1254", ef_cp1254_parser_new, ef_cp1254_conv_new, },
  { VT_CP1255, "CP1255", ef_cp1255_parser_new, ef_cp1255_conv_new, },
  { VT_CP1256, "CP1256", ef_cp1256_parser_new, ef_cp1256_conv_new, },
  { VT_CP1257, "CP1257", ef_cp1257_parser_new, ef_cp1257_conv_new, },
  { VT_CP1258, "CP1258", ef_cp1258_parser_new, ef_cp1258_conv_new, },
  { VT_CP874, "CP874", ef_cp874_parser_new, ef_cp874_conv_new, },

  { VT_UTF8, "UTF8", ef_utf8_parser_new, ef_utf8_conv_new, },

  { VT_EUCJP, "EUCJP", ef_eucjp_parser_new, ef_eucjp_conv_new, },
  { VT_EUCJISX0213, "EUCJISX0213", ef_eucjisx0213_parser_new, ef_eucjisx0213_conv_new, },
  { VT_ISO2022JP, "ISO2022JP", ef_iso2022jp_7_parser_new, ef_iso2022jp_7_conv_new, },
  { VT_ISO2022JP2, "ISO2022JP2", ef_iso2022jp2_parser_new, ef_iso2022jp2_conv_new, },
  { VT_ISO2022JP3, "ISO2022JP3", ef_iso2022jp3_parser_new, ef_iso2022jp3_conv_new, },
  { VT_SJIS, "SJIS", ef_sjis_parser_new, ef_sjis_conv_new, },
  { VT_SJISX0213, "SJISX0213", ef_sjisx0213_parser_new, ef_sjisx0213_conv_new, },

  { VT_EUCKR, "EUCKR", ef_euckr_parser_new, ef_euckr_conv_new, },
  { VT_UHC, "UHC", ef_uhc_parser_new, ef_uhc_conv_new, },
  { VT_JOHAB, "JOHAB", ef_johab_parser_new, ef_johab_conv_new, },
  { VT_ISO2022KR, "ISO2022KR", ef_iso2022kr_parser_new, ef_iso2022kr_conv_new, },

  { VT_BIG5, "BIG5", ef_big5_parser_new, ef_big5_conv_new, },
  { VT_EUCTW, "EUCTW", ef_euctw_parser_new, ef_euctw_conv_new, },

  { VT_BIG5HKSCS, "BIG5HKSCS", ef_big5hkscs_parser_new, ef_big5hkscs_conv_new, },

  /* not listed in IANA. GB2312 is usually used instead. */
  { VT_EUCCN, "EUCCN", ef_euccn_parser_new, ef_euccn_conv_new, },
  { VT_GBK, "GBK", ef_gbk_parser_new, ef_gbk_conv_new, },
  { VT_GB18030, "GB18030", ef_gb18030_2000_parser_new, ef_gb18030_2000_conv_new, },
  { VT_HZ, "HZ", ef_hz_parser_new, ef_hz_conv_new, },

  { VT_ISO2022CN, "ISO2022CN", ef_iso2022cn_parser_new, ef_iso2022cn_conv_new, },

  /*
   * alternative names.
   * these are not used in vt_{parser|conv}_new , so parser_new/parser_conv
   * members are not necessary.
   */

  { VT_TIS620, "TIS620", },

#if 0
  /* XXX necessary ? */
  { VT_EUCJP, "EXTENDEDUNIXCODEPACKEDFORMATFORJAPANESE", }, /* MIME */
  { VT_EUCJP, "CSEUCPKDFMTJAPANESE", }, /* MIME */
#endif
  { VT_EUCJP, "UJIS" },
  { VT_SJIS, "SHIFTJIS", }, /* MIME */

  { VT_EUCKR, "KSC56011987", }, /* for IIS error page(IIS bug?) */

  { VT_EUCCN, "GB2312", },

  { VT_HZ, "HZGB2312", },
};

/*
 * MSB of these charsets are not set , but must be set manually for X font.
 * These charsets are placed in an ascending order.
 */
static ef_charset_t msb_set_cs_table[] = {
    JISX0201_KATA, ISO8859_1_R,  ISO8859_2_R,  ISO8859_3_R,  ISO8859_4_R,     ISO8859_5_R,
    ISO8859_6_R,   ISO8859_7_R,  ISO8859_8_R,  ISO8859_9_R,  ISO8859_10_R,    TIS620_2533,
    ISO8859_13_R,  ISO8859_14_R, ISO8859_15_R, ISO8859_16_R, TCVN5712_3_1993,

};

static struct {
  u_int16_t ucs;
  u_char decsp;

} ucs_to_decsp_table[] = {
  {0xa0, '_'},
  {0xa3, '}'},
  {0xb0, 'f'},
  {0xb1, 'g'},
  {0xb7, '~'},
  {0x3c0, '{'},
  {0x2260, '|'},
  {0x2264, 'y'},
  {0x2265, 'z'},
  {0x23ba, 'o'},
  {0x23bb, 'p'},
  {0x23bc, 'r'},
  {0x23bd, 's'},
  {0x2409, 'b'},
  {0x240a, 'e'},
  {0x240b, 'i'},
  {0x240c, 'c'},
  {0x240d, 'd'},
  {0x2424, 'h'},
  {0x2500, 'q'},
  {0x2502, 'x'},
  {0x250c, 'l'},
  {0x2510, 'k'},
  {0x2514, 'm'},
  {0x2518, 'j'},
  {0x251c, 't'},
  {0x2524, 'u'},
  {0x252c, 'w'},
  {0x2534, 'v'},
  {0x253c, 'n'},
  {0x2592, 'a'},
  {0x25c6, '`'},
};

static void (*iso2022kr_conv_init)(ef_conv_t *);
static void (*iso2022kr_parser_init)(ef_parser_t *);

/* --- static functions --- */

static void ovrd_iso2022kr_conv_init(ef_conv_t *conv) {
  u_char buf[5];
  ef_parser_t *parser;

  (*iso2022kr_conv_init)(conv);

  if ((parser = ef_iso2022kr_parser_new()) == NULL) {
    return;
  }

  /* designating KSC5601 to G1 */
  (*parser->set_str)(parser, "\x1b$)Ca", 5);

  /* this returns sequence of designating KSC5601 to G1 */
  (*conv->convert)(conv, buf, sizeof(buf), parser);

  (*parser->destroy)(parser);
}

static void ovrd_iso2022kr_parser_init(ef_parser_t *parser) {
  u_char buf[5];
  ef_conv_t *conv;

  (*iso2022kr_parser_init)(parser);

  if ((conv = ef_iso2022kr_conv_new()) == NULL) {
    return;
  }

  /* designating KSC5601 to G1 */
  (*parser->set_str)(parser, "\x1b$)Ca", 5);

  /* this returns sequence of designating KSC5601 to G1 */
  (*conv->convert)(conv, buf, sizeof(buf), parser);

  (*conv->destroy)(conv);
}

static size_t iso2022_illegal_char(ef_conv_t *conv, u_char *dst, size_t dst_size, int *is_full,
                                   ef_char_t *ch) {
  if (ch->cs == ISO10646_UCS4_1) {
    vt_convert_unicode_pua_to_drcs(ch);
  }

  return ef_iso2022_illegal_char(conv, dst, dst_size, is_full, ch);
}

static size_t non_iso2022_illegal_char(ef_conv_t *conv, u_char *dst, size_t dst_size, int *is_full,
                                       ef_char_t *ch) {
  *is_full = 0;

  if (ch->cs == DEC_SPECIAL) {
    if (dst_size < 7) {
      *is_full = 1;

      return 0;
    }

    dst[0] = '\x1b';
    dst[1] = '(';
    dst[2] = '0';
    dst[3] = ch->ch[0];
    dst[4] = '\x1b';
    dst[5] = '(';
    dst[6] = 'B';

    return 7;
  } else {
    return 0;
  }
}

static size_t utf8_illegal_char(ef_conv_t *conv, u_char *dst, size_t dst_size, int *is_full,
                                ef_char_t *ch) {
  *is_full = 0;

  if (ch->cs == DEC_SPECIAL) {
    u_int16_t utf16;

    if (dst_size < 3) {
      *is_full = 1;
    } else if ((utf16 = vt_convert_decsp_to_ucs(ef_char_to_int(ch)))) {
      dst[0] = ((utf16 >> 12) & 0x0f) | 0xe0;
      dst[1] = ((utf16 >> 6) & 0x3f) | 0x80;
      dst[2] = (utf16 & 0x3f) | 0x80;

      return 3;
    }
  }

  return 0;
}

/* --- global functions --- */

char *vt_get_char_encoding_name(vt_char_encoding_t encoding) {
  if (encoding < 0 || MAX_CHAR_ENCODINGS <= encoding) {
    return "ISO88591";
  } else {
    return encoding_table[encoding].name;
  }
}

vt_char_encoding_t vt_get_char_encoding(const char *name /* '_' and '-' are ignored. */
                                        ) {
  int count;
  char *_name;
  char *encoding;
  char *p;

  /*
   * duplicating name so as not to destroy its memory.
   */
  if ((_name = alloca(strlen(name) + 1)) == NULL ||
      (encoding = alloca(strlen(name) + 1)) == NULL) {
    return VT_UNKNOWN_ENCODING;
  }
  strcpy(_name, name);
  encoding[0] = '\0';

  /*
   * removing '-' and '_' from name.
   */
  while ((p = bl_str_sep(&_name, "-_")) != NULL) {
    strcat(encoding, p);
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " encoding -> %s.\n", encoding);
#endif

  if (strcasecmp(encoding, "auto") == 0) {
    /*
     * XXX
     * UTF-8 is used by default in cygwin, msys, win32, android and osx.
     * (On osx, if mlterm.app is started from Finder,
     * vt_get_char_encoding("auto") returns VT_ISO88591.)
     * Note that vt_get_char_encoding("auto") is used to set character encoding
     * of window/icon title string, not only to determine character encoding.
     * (see vt_parser.c)
     */
#if !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(USE_WIN32API) && \
  !defined(__ANDROID__) && !defined(__APPLE__)
    vt_char_encoding_t e;

    if ((e = vt_get_char_encoding(bl_get_codeset())) != VT_UNKNOWN_ENCODING) {
      return e;
    }
#endif

    return VT_UTF8;
  }

  for (count = 0; count < sizeof(encoding_table) / sizeof(encoding_table_t); count++) {
    if (strcasecmp(encoding, encoding_table[count].name) == 0) {
      return encoding_table[count].encoding;
    }
  }

  return VT_UNKNOWN_ENCODING;
}

ef_parser_t *vt_char_encoding_parser_new(vt_char_encoding_t encoding) {
  ef_parser_t *parser;

  if (encoding < 0 || MAX_CHAR_ENCODINGS <= encoding ||
      encoding_table[encoding].encoding != encoding) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %d is illegal encoding.\n", encoding);
#endif

    return NULL;
  }

  if ((parser = (*encoding_table[encoding].parser_new)()) == NULL) {
    return NULL;
  }

  if (encoding == VT_ISO2022KR) {
    /* overriding init method */

    iso2022kr_parser_init = parser->init;
    parser->init = ovrd_iso2022kr_parser_init;

    (*parser->init)(parser);
  }

  return parser;
}

ef_conv_t *vt_char_encoding_conv_new(vt_char_encoding_t encoding) {
  ef_conv_t *conv;

  if (encoding < 0 || MAX_CHAR_ENCODINGS <= encoding ||
      encoding_table[encoding].encoding != encoding) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %d is illegal encoding.\n", encoding);
#endif

    return NULL;
  }

  if ((conv = (*encoding_table[encoding].conv_new)()) == NULL) {
    return NULL;
  }

  if (encoding == VT_UTF8) {
    conv->illegal_char = utf8_illegal_char;
  } else if (IS_ENCODING_BASED_ON_ISO2022(encoding)) {
    if (encoding == VT_ISO2022KR) {
      /* overriding init method */

      iso2022kr_conv_init = conv->init;
      conv->init = ovrd_iso2022kr_conv_init;

      (*conv->init)(conv);
    }
  }

  return conv;
}

int vt_is_msb_set(ef_charset_t cs) {
  if (msb_set_cs_table[0] <= cs &&
      cs <= msb_set_cs_table[sizeof(msb_set_cs_table) / sizeof(msb_set_cs_table[0]) - 1]) {
    int count;

    for (count = 0; count < sizeof(msb_set_cs_table) / sizeof(msb_set_cs_table[0]); count++) {
      if (msb_set_cs_table[count] == cs) {
        return 1;
      }
    }
  }

  return 0;
}

size_t vt_char_encoding_convert(u_char *dst, size_t dst_len, vt_char_encoding_t dst_encoding,
                                u_char *src, size_t src_len, vt_char_encoding_t src_encoding) {
  ef_parser_t *parser;
  size_t filled_len;

  if ((parser = vt_char_encoding_parser_new(src_encoding)) == NULL) {
    return 0;
  }

  (*parser->init)(parser);
  (*parser->set_str)(parser, src, src_len);
  filled_len = vt_char_encoding_convert_with_parser(dst, dst_len, dst_encoding, parser);
  (*parser->destroy)(parser);

  return filled_len;
}

size_t vt_char_encoding_convert_with_parser(u_char *dst, size_t dst_len,
                                            vt_char_encoding_t dst_encoding, ef_parser_t *parser) {
  ef_conv_t *conv;
  size_t filled_len;

  if ((conv = vt_char_encoding_conv_new(dst_encoding)) == NULL) {
    return 0;
  }

  (*conv->init)(conv);
  filled_len = (*conv->convert)(conv, dst, dst_len, parser);
  (*conv->destroy)(conv);

  return filled_len;
}

int vt_parse_unicode_area(const char *str, u_int *min, u_int *max) {
  if (sscanf(str, "U+%x-%x", min, max) != 2) {
    if (sscanf(str, "U+%x", min) != 1) {
      goto error;
    } else {
      *max = *min;
    }
  } else if (*min > *max) {
    goto error;
  }

  return 1;

error:
  bl_msg_printf("Illegal unicode area format: %s\n", str);

  return 0;
}

/* XXX This function should be moved to mef */
u_char vt_convert_ucs_to_decsp(u_int16_t ucs) {
  int l_idx;
  int h_idx;
  int idx;

  l_idx = 0;
  h_idx = sizeof(ucs_to_decsp_table) / sizeof(ucs_to_decsp_table[0]) - 1;

  if (ucs < ucs_to_decsp_table[l_idx].ucs || ucs_to_decsp_table[h_idx].ucs < ucs) {
    return 0;
  }

  while (1) {
    idx = (l_idx + h_idx) / 2;

    if (ucs == ucs_to_decsp_table[idx].ucs) {
      return ucs_to_decsp_table[idx].decsp;
    } else if (ucs < ucs_to_decsp_table[idx].ucs) {
      h_idx = idx;
    } else {
      l_idx = idx + 1;
    }

    if (l_idx >= h_idx) {
      return 0;
    }
  }
}

/* XXX This function should be moved to mef */
u_int16_t vt_convert_decsp_to_ucs(u_char decsp) {
  if ('`' <= decsp && decsp <= 'x') {
    int count;

    for (count = 0; count < sizeof(ucs_to_decsp_table) / sizeof(ucs_to_decsp_table[0]); count++) {
      if (ucs_to_decsp_table[count].decsp == decsp) {
        return ucs_to_decsp_table[count].ucs;
      }
    }
  }

  return 0;
}

void vt_char_encoding_conv_set_use_loose_rule(ef_conv_t *conv, vt_char_encoding_t encoding,
                                              int flag) {
  if (flag) {
    if (IS_ENCODING_BASED_ON_ISO2022(encoding)) {
      conv->illegal_char = iso2022_illegal_char;
    } else {
      conv->illegal_char = non_iso2022_illegal_char;
    }
  } else {
    if (encoding == VT_UTF8) {
      conv->illegal_char = utf8_illegal_char;
    } else {
      conv->illegal_char = NULL;
    }
  }
}
