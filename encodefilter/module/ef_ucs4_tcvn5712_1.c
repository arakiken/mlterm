/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_tcvn5712_1.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_ucs4_to_tcvn5712_1993.table"
#include "table/ef_tcvn5712_1993_to_ucs4.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_ucs4_to_tcvn5712_1_1993(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "TCVN5712-1", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, non_ucs->ch, 1);

  non_ucs->size = 1;
  non_ucs->cs = TCVN5712_1_1993;

  /* See also ef_8bit_parser.c */
  if (0xb0 <= non_ucs->ch[0] && non_ucs->ch[0] <= 0xb4) {
    non_ucs->property = EF_COMBINING;
  } else {
    non_ucs->property = 0;
  }

  return 1;
}

int ef_map_tcvn5712_1_1993_to_ucs4(ef_char_t *ucs4, u_int16_t tcvn_code) {
  static iconv_t cd;
  u_char src[1];

  ICONV_OPEN(cd, "UTF-32BE", "TCVN5712-1");

  src[0] = tcvn_code;

  ICONV(cd, src, 1, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#else

int ef_map_ucs4_to_tcvn5712_1_1993(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_TCVN5712_1993(ucs4_code))) {
    non_ucs->ch[0] = c;
  } else {
    return 0;
  }

  non_ucs->size = 1;
  non_ucs->cs = TCVN5712_1_1993;

  if (0xb0 <= c && c <= 0xb4) {
    non_ucs->property = EF_COMBINING;
  } else {
    non_ucs->property = 0;
  }

  return 1;
}

int ef_map_tcvn5712_1_1993_to_ucs4(ef_char_t *ucs4, u_int16_t tcvn_code) {
  u_int32_t c;

  if ((c = CONV_TCVN5712_1993_TO_UCS4(tcvn_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
  } else{
   return 0;
  }

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#endif
