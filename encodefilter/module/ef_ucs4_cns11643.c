/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_cns11643.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_cns11643_1992_1_to_ucs4.table"
#include "table/ef_cns11643_1992_2_to_ucs4.table"
#include "table/ef_cns11643_1992_3_to_ucs4.table"

#include "table/ef_ucs4_to_cns11643_1992_1.table"
#include "table/ef_ucs4_to_cns11643_1992_2.table"
#include "table/ef_ucs4_to_cns11643_1992_3.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_cns11643_1992_1_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "EUC-TW");

  src[1] = ((cns & 0x7f) | 0x80);
  src[0] = (((cns >> 8) & 0x7f) | 0x80);

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_cns11643_1992_2_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  static iconv_t cd;
  u_char src[4];

  ICONV_OPEN(cd, "UTF-32BE", "EUC-TW");

  src[3] = ((cns & 0x7f) | 0x80);
  src[2] = (((cns >> 8) & 0x7f) | 0x80);
  src[1] = 0xa2;
  src[0] = 0x8e;

  ICONV(cd, src, 4, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_cns11643_1992_3_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  static iconv_t cd;
  u_char src[4];

  ICONV_OPEN(cd, "UTF-32BE", "EUC-TW");

  src[3] = ((cns & 0x7f) | 0x80);
  src[2] = (((cns >> 8) & 0x7f) | 0x80);
  src[1] = 0xa3;
  src[0] = 0x8e;

  ICONV(cd, src, 4, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_cns11643_1992_1(ef_char_t *cns, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "EUC-TW", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, cns->ch, 2);

  cns->ch[0] &= 0x7f;
  cns->ch[1] &= 0x7f;

  cns->size = 2;
  cns->cs = CNS11643_1992_1;
  cns->property = 0;

  return 1;
}

int ef_map_ucs4_to_cns11643_1992_2(ef_char_t *cns, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "EUC-TW", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, cns->ch, 4);

  cns->ch[0] = cns->ch[2] & 0x7f;
  cns->ch[1] = cns->ch[3] & 0x7f;

  cns->size = 2;
  cns->cs = CNS11643_1992_2;
  cns->property = 0;

  return 1;
}

int ef_map_ucs4_to_cns11643_1992_3(ef_char_t *cns, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "EUC-TW", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, cns->ch, 4);

  cns->ch[0] = cns->ch[2] & 0x7f;
  cns->ch[1] = cns->ch[3] & 0x7f;

  cns->size = 2;
  cns->cs = CNS11643_1992_3;
  cns->property = 0;

  return 1;
}

#else

int ef_map_cns11643_1992_1_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  u_int32_t c;

  if ((c = CONV_CNS11643_1992_1_TO_UCS4(cns))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cns11643_1992_2_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  u_int32_t c;

  if ((c = CONV_CNS11643_1992_2_TO_UCS4(cns))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cns11643_1992_3_to_ucs4(ef_char_t *ucs4, u_int16_t cns) {
  u_int32_t c;

  if ((c = CONV_CNS11643_1992_3_TO_UCS4(cns))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cns11643_1992_1(ef_char_t *cns, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_CNS11643_1992_1(ucs4_code))) {
    ef_int_to_bytes(cns->ch, 2, c);
    cns->size = 2;
    cns->cs = CNS11643_1992_1;
    cns->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cns11643_1992_2(ef_char_t *cns, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_CNS11643_1992_2(ucs4_code))) {
    ef_int_to_bytes(cns->ch, 2, c);
    cns->size = 2;
    cns->cs = CNS11643_1992_2;
    cns->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cns11643_1992_3(ef_char_t *cns, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_CNS11643_1992_3(ucs4_code))) {
    ef_int_to_bytes(cns->ch, 2, c);
    cns->size = 2;
    cns->cs = CNS11643_1992_3;
    cns->property = 0;

    return 1;
  }

  return 0;
}

#endif
