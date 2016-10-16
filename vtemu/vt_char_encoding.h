/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_CHAR_ENCODING_H__
#define __VT_CHAR_ENCODING_H__

#include <pobl/bl_types.h> /* u_char */
#include <mef/ef_parser.h>
#include <mef/ef_conv.h>

/*
 * Supported encodings are those which are not conflicted with US_ASCII.
 * So , UCS-2, UCS-4 etc encodings are not supported.
 */
typedef enum vt_char_encoding {
  VT_UNKNOWN_ENCODING = -1,

  VT_ISO8859_1 = 0,
  VT_ISO8859_2,
  VT_ISO8859_3,
  VT_ISO8859_4,
  VT_ISO8859_5,
  VT_ISO8859_6,
  VT_ISO8859_7,
  VT_ISO8859_8,
  VT_ISO8859_9,
  VT_ISO8859_10,
  VT_TIS620,
  VT_ISO8859_13,
  VT_ISO8859_14,
  VT_ISO8859_15,
  VT_ISO8859_16,
  VT_TCVN5712,

  VT_ISCII_ASSAMESE,
  VT_ISCII_BENGALI,
  VT_ISCII_GUJARATI,
  VT_ISCII_HINDI,
  VT_ISCII_KANNADA,
  VT_ISCII_MALAYALAM,
  VT_ISCII_ORIYA,
  VT_ISCII_PUNJABI,
  VT_ISCII_TELUGU,
  VT_VISCII,
  VT_KOI8_R,
  VT_KOI8_U,
  VT_KOI8_T,
  VT_GEORGIAN_PS,
  VT_CP1250,
  VT_CP1251,
  VT_CP1252,
  VT_CP1253,
  VT_CP1254,
  VT_CP1255,
  VT_CP1256,
  VT_CP1257,
  VT_CP1258,
  VT_CP874,

  VT_UTF8,

  VT_EUCJP,
  VT_EUCJISX0213,
  VT_ISO2022JP,
  VT_ISO2022JP2,
  VT_ISO2022JP3,
  VT_SJIS,
  VT_SJISX0213,

  VT_EUCKR,
  VT_UHC,
  VT_JOHAB,
  VT_ISO2022KR,

  VT_BIG5,
  VT_EUCTW,

  VT_BIG5HKSCS,

  VT_EUCCN,
  VT_GBK,
  VT_GB18030,
  VT_HZ,

  VT_ISO2022CN,

  MAX_CHAR_ENCODINGS

} vt_char_encoding_t;

#define IS_ISO8859_VARIANT(encoding) (VT_ISO8859_1 <= (encoding) && (encoding) <= VT_TCVN5712)

#define IS_8BIT_ENCODING(encoding) (VT_ISO8859_1 <= (encoding) && (encoding) <= VT_CP874)

#define IS_ENCODING_BASED_ON_ISO2022(encoding)                                                \
  (IS_ISO8859_VARIANT(encoding) || (VT_EUCJP <= (encoding) && (encoding) <= VT_ISO2022JP3) || \
   VT_EUCKR == (encoding) || VT_ISO2022KR == (encoding) || VT_EUCTW == (encoding) ||          \
   VT_ISO2022CN == (encoding) || VT_EUCCN == (encoding))

/* ISO2022KR is subset and EUC-TW is not subset */
#define IS_UCS_SUBSET_ENCODING(encoding)                                                       \
  ((encoding) != VT_ISO2022JP && (encoding) != VT_ISO2022JP2 && (encoding) != VT_ISO2022JP3 && \
   (encoding) != VT_ISO2022CN && (encoding) != VT_EUCTW)

/* 0x0 - 0x7f is not necessarily US-ASCII */
#define IS_STATEFUL_ENCODING(encoding)                                                         \
  ((encoding) == VT_ISO2022JP || (encoding) == VT_ISO2022JP2 || (encoding) == VT_ISO2022JP3 || \
   (encoding) == VT_ISO2022KR || (encoding) == VT_ISO2022CN || (encoding) == VT_HZ)

#define IS_ISCII_ENCODING(encoding) \
  (VT_ISCII_ASSAMESE <= (encoding) && (encoding) <= VT_ISCII_TELUGU)

char* vt_get_char_encoding_name(vt_char_encoding_t encoding);

vt_char_encoding_t vt_get_char_encoding(const char* name);

ef_parser_t* vt_char_encoding_parser_new(vt_char_encoding_t encoding);

ef_conv_t* vt_char_encoding_conv_new(vt_char_encoding_t encoding);

int vt_is_msb_set(ef_charset_t cs);

size_t vt_char_encoding_convert(u_char* dst, size_t dst_len, vt_char_encoding_t dst_encoding,
                                u_char* src, size_t src_len, vt_char_encoding_t src_encoding);

size_t vt_char_encoding_convert_with_parser(u_char* dst, size_t dst_len,
                                            vt_char_encoding_t dst_encoding, ef_parser_t* parser);

int vt_parse_unicode_area(const char* str, u_int* min, u_int* max);

#endif
