/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_CHARSET_H__
#define __EF_CHARSET_H__

#include <pobl/bl_types.h> /* u_xxx */

/*
 * ISO2022 Ft should be within 0x40('@') and 0x7e('~') except
 * DEC_SPECIAL(Ft='0').
 */

/* 0x00 - 0x4e (Ft is within 0x30 and 0x7e) */
#define CS94SB_ID(c) ((u_char)(c) - 0x30)
/* 0x50 - 0x9e (Ft is within 0x30 and 0x7e) */
#define CS96SB_ID(c) ((u_char)(c) + 0x20)
/* 0xa0 - 0xbf (XXX Ft is within 0x40 and 0x5f) */
#define CS94MB_ID(c) ((u_char)(c) + 0x60)
/* No 96^n cs exists. */
#define CS96MB_ID(c) UNKNOWN_CS
/* 0xc0 - 0xcf (Ft is within 0x40 and 0x4f) */
#define NON_ISO2022_1_ID(c) ((u_char)(c) + 0x80)
/* 0xd0 - 0xdf (Ft is within 0x40 and 0x4f) */
#define NON_ISO2022_2_ID(c) ((u_char)(c) + 0x90)

/* 0x100 - 0x7bf (= 0xX00 | CS9XXB_ID) */
#define CS_REVISION_N(cs, n) ((cs) + ((n) << 8))

#define CS_ADD_INTERMEDIATE(cs, i) ((cs) + (((i) - 0x1f) << 11))

/*
 * 'and 0xff' should be done because 0x100 - region is used for 'or cs_revision'
 */

#define CS94SB_FT(cs) (((cs)&0xff) + 0x30)
#define CS96SB_FT(cs) (((cs)&0xff) - 0x20)
#define CS94MB_FT(cs) (((cs)&0xff) - 0x60)
#define CS96MB_FT(cs) ' ' /* dummy */

#define IS_CS94SB(cs) ((unsigned int)((cs)&0xff) <= 0x4e) /* same as 0x00 <= .. <= 0x4e */
#define IS_CS96SB(cs) (0x50 <= ((cs)&0xff) && ((cs)&0xff) <= 0x9e)
#define IS_CSSB(cs) ((unsigned int)((cs)&0xff) <= 0x9e)
#define IS_CS94MB(cs) (0xa0 <= ((cs)&0xff) && ((cs)&0xff) <= 0xbf)
#define IS_CS96MB(cs) (0) /* always false */
/* IS_CS94_STRICT() always returns false charsets containing intermediate chars. */
#define IS_CS94_STRICT(cs) (((unsigned int)(cs) & 0xf8ff) <= 0x4e ||    \
                            (0xa0 <= ((cs) & 0xf8ff) && ((cs) & 0xf8ff) <= 0x9e))
#define IS_CS_BASED_ON_ISO2022(cs) (0x0 <= ((cs)&0xff) && ((cs)&0xff) <= 0xbf)
/* without "(cs) != UNKNOWN_CS &&", 0xa0 <= (UNKNOWN_CS & 0xff) returns true. */
#define IS_NON_ISO2022(cs) ((cs) != UNKNOWN_CS && 0xc0 <= ((cs)&0xff))
#define IS_ISCII(cs) (0xf0 <= (cs) && (cs) <= 0xfa)
#define IS_JIS_EXT(cs) (JISC6226_1978_NEC_EXT <= (cs) && (cs) <= SJIS_IBM_EXT)

#define IS_FULLWIDTH_CS(cs) (IS_CS94MB(cs) || IS_CS96MB(cs) || (0x1e0 <= (cs) && (cs) <= 0x1ff))
#define CS_SIZE(cs) \
  ((cs) == ISO10646_UCS4_1 ? 4 : ((IS_FULLWIDTH_CS(cs) || (cs) == ISO10646_UCS2_1) ? 2 : 1))
#define IS_ISO10646_UCS4(cs) (((cs) & ~CS_REVISION_N(0, 1)) == ISO10646_UCS4_1)

#define CS_DEL_INTERMEDIATE(cs) ((cs) & 0x7ff)
#define CS_INTERMEDIATE(cs) (((cs) & 0xf800) ? (((cs) >> 11) & 0x1f) + 0x1f : 0)

/*
 * These enumeration numbers are based on iso2022 Ft(0x30-0x7e).
 * Total range is -1 <-> 0x2ff(int16).
 */
typedef enum ef_charset {
  UNKNOWN_CS = -1,

  /* 94 sb cs */
  DEC_SPECIAL = CS94SB_ID('0'),
  DEC_TECHNICAL = CS94SB_ID('>'),
  ISO646_IRV = CS94SB_ID('@'),
  ISO646_EN = CS94SB_ID('A'),
  US_ASCII = CS94SB_ID('B'),
  NATS_PRIMARY_FOR_FIN_SWEDEN = CS94SB_ID('C'),
  NATS_PRIMARY_FOR_DEN_NOR = CS94SB_ID('E'),
  ISO646_SWEDEN = CS94SB_ID('G'),
  ISO646_SWEDEN_NAME = CS94SB_ID('H'),
  JISX0201_KATA = CS94SB_ID('I'),
  JISX0201_ROMAN = CS94SB_ID('J'),

  /* 96 sb cs */
  ISO8859_1_R = CS96SB_ID('A'),
  ISO8859_2_R = CS96SB_ID('B'),
  ISO8859_3_R = CS96SB_ID('C'),
  ISO8859_4_R = CS96SB_ID('D'),
  ISO8859_7_R = CS96SB_ID('F'),
  ISO8859_6_R = CS96SB_ID('G'),
  ISO8859_8_R = CS96SB_ID('H'),
  ISO8859_5_R = CS96SB_ID('L'),
  ISO8859_9_R = CS96SB_ID('M'),
  ISO8859_10_R = CS96SB_ID('V'),
  TIS620_2533 = CS96SB_ID('T'),
  ISO8859_13_R = CS96SB_ID('Y'), /* Ft = 5/9 */
  ISO8859_14_R = CS96SB_ID('_'), /* Ft = 5/15 */

  ISO8859_15_R = CS96SB_ID('b'), /* Ft = 6/2 */
  ISO8859_16_R = CS96SB_ID('f'), /* Ft = 6/6 */
  TCVN5712_3_1993 = CS96SB_ID('Z'), /* = TVCVN5712_2_1993 (Subset of TCVN5712_1_1993) */

  /* 94 mb cs */
  JISC6226_1978 = CS94MB_ID('@'),
  GB2312_80 = CS94MB_ID('A'),
  JISX0208_1983 = CS94MB_ID('B'),
  KSC5601_1987 = CS94MB_ID('C'),
  JISX0212_1990 = CS94MB_ID('D'),
  CNS11643_1992_1 = CS94MB_ID('G'),
  CNS11643_1992_2 = CS94MB_ID('H'),
  CNS11643_1992_3 = CS94MB_ID('I'),
  CNS11643_1992_4 = CS94MB_ID('J'),
  CNS11643_1992_5 = CS94MB_ID('K'),
  CNS11643_1992_6 = CS94MB_ID('L'),
  CNS11643_1992_7 = CS94MB_ID('M'),
  JISX0213_2000_1 = CS94MB_ID('O'),
  JISX0213_2000_2 = CS94MB_ID('P'),

  /* 96 mb cs */
  /* Nothing */

  /* NOT ISO2022 class 1 (ESC 2/5 Ft) */
  UTF1 = NON_ISO2022_1_ID('B'),
  UTF8 = NON_ISO2022_1_ID('G'),

  /* NOT ISO2022 class 2 (ESC 2/5 2/15 Ft) */
  XCT_NON_ISO2022_CS_1 = NON_ISO2022_2_ID('1'), /* CTEXT */
  XCT_NON_ISO2022_CS_2 = NON_ISO2022_2_ID('2'), /* CTEXT */
  ISO10646_UCS2_1 = NON_ISO2022_2_ID('@'),      /* Including US_ASCII(0x0-0x7f) */
  ISO10646_UCS4_1 = NON_ISO2022_2_ID('A'),      /* Including US_ASCII(0x0-0x7f) */

  /* Followings are mef original classifications */

  /*
   * Those who are not ISO2022 registed characterset or do not confirm to
   * ISO2022.
   * 0xe0 - 0xfa
   */
  VISCII = 0xe0,          /* Excluding most of US_ASCII */
  TCVN5712_1_1993 = 0xe1, /* Excluding most of US_ASCII. Superset of TCVN5712_3_1993 */
  KOI8_R = 0xe2,          /* Excluding US_ASCII(0x0-0x7f) */
  KOI8_U = 0xe3,          /* Excluding US_ASCII(0x0-0x7f) */
  KOI8_T = 0xe4,          /* Excluding US_ASCII(0x0-0x7f) */
  GEORGIAN_PS = 0xe5,     /* Excluding US_ASCII(0x0-0x7f) */
  CP1250 = 0xe6,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1251 = 0xe7,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1252 = 0xe8,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1253 = 0xe9,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1254 = 0xea,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1255 = 0xeb,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1256 = 0xec,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1257 = 0xed,          /* Excluding US_ASCII(0x0-0x7f) */
  CP1258 = 0xee,          /* Excluding US_ASCII(0x0-0x7f) */
  CP874 = 0xef,           /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_ASSAMESE = 0xf0,  /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_BENGALI = 0xf1,   /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_GUJARATI = 0xf2,  /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_HINDI = 0xf3,     /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_KANNADA = 0xf4,   /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_MALAYALAM = 0xf5, /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_ORIYA = 0xf6,     /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_PUNJABI = 0xf7,   /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_TAMIL = 0xf8,     /* Excluding US_ASCII(0x0-0x7f) */
  ISCII_TELUGU = 0xf9,    /* Excluding US_ASCII(0x0-0x7f) */
#if 0
  ISCII_ROMAN = 0xfa, /* Excluding US_ASCII(0x0-0x7f) */
#endif

  /* Followings are ISO2022 based charsets with revisions. */

  /* Revision 1 */
  JISX0208_1990 = CS_REVISION_N(JISX0208_1983, 1),
  ISO10646_UCS4_1_V = CS_REVISION_N(ISO10646_UCS4_1, 1), /* mef original */

  /* Followings are mef original classifications */

  /*
   * Those who are not ISO2022 registed characterset but confirm to ISO2022.
   * (fullwidth)
   * 0x1e0 - 0xf5
   */
  JISC6226_1978_NEC_EXT = 0x1e0,
  JISC6226_1978_NECIBM_EXT = 0x1e1,
  JISX0208_1983_MAC_EXT = 0x1e2,

  /*
   * Those who are not ISO2022 registed characterset or do not confirm to
   * ISO2022.
   * (fullwidth)
   * 0x1e3 - 0x1e9
   */
  SJIS_IBM_EXT = 0x1e3,
  UHC = 0x1e4,
  BIG5 = 0x1e5,
  CNS11643_1992_EUCTW_G2 = 0x1e6,
  GBK = 0x1e7,
  JOHAB = 0x1e8,
  HKSCS = 0x1e9,

  MAX_CHARSET = 0x2ff

} ef_charset_t;

#endif
