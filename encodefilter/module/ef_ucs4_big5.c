/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_big5.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_big5_to_ucs4.table"
#include "table/ef_hkscs_to_ucs4.table"

#include "table/ef_ucs4_to_big5.table"
#include "table/ef_ucs4_to_hkscs.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_big5_to_ucs4(ef_char_t *ucs4, u_int16_t big5) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "BIG-5");

  src[1] = big5 & 0xff;
  src[0] = (big5 >> 8) & 0xff;

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_hkscs_to_ucs4(ef_char_t *ucs4, u_int16_t hkscs) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "BIG5-HKSCS");

  src[1] = hkscs & 0xff;
  src[0] = (hkscs >> 8) & 0xff;

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_big5(ef_char_t *big5, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "BIG5", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, big5->ch, 2);

  big5->size = 2;
  big5->cs = BIG5;
  big5->property = 0;

  return 1;
}

int ef_map_ucs4_to_hkscs(ef_char_t *hkscs, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "BIG5-HKSCS", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, hkscs->ch, 2);

  hkscs->size = 2;
  hkscs->cs = HKSCS;
  hkscs->property = 0;

  return 1;
}

#else

int ef_map_big5_to_ucs4(ef_char_t *ucs4, u_int16_t big5) {
  u_int32_t c;

  if ((c = CONV_BIG5_TO_UCS4(big5))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_hkscs_to_ucs4(ef_char_t *ucs4, u_int16_t hkscs) {
  u_int32_t c;

  if ((c = CONV_HKSCS_TO_UCS4(hkscs))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_big5(ef_char_t *big5, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_BIG5(ucs4_code))) {
    ef_int_to_bytes(big5->ch, 2, c);
    big5->size = 2;
    big5->cs = BIG5;
    big5->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_hkscs(ef_char_t *hkscs, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_HKSCS(ucs4_code))) {
    ef_int_to_bytes(hkscs->ch, 2, c);
    hkscs->size = 2;
    hkscs->cs = HKSCS;
    hkscs->property = 0;

    return 1;
  }

  return 0;
}

#endif
