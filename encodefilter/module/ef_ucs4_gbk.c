/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_gbk.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_gbk_to_ucs4.table"
#include "table/ef_ucs4_to_gbk.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_gbk_to_ucs4(ef_char_t *ucs4, u_int16_t gb) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "GBK");

  src[1] = gb & 0xff;
  src[0] = (gb >> 8) & 0xff;

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_gbk(ef_char_t *gb, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "GBK", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, gb->ch, 2);

  gb->size = 2;
  gb->cs = GBK;
  gb->property = 0;

  return 1;
}

#else

int ef_map_gbk_to_ucs4(ef_char_t *ucs4, u_int16_t gb) {
  u_int32_t c;

  if ((c = CONV_GBK_TO_UCS4(gb))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_gbk(ef_char_t *gb, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_GBK(ucs4_code))) {
    ef_int_to_bytes(gb->ch, 2, c);
    gb->size = 2;
    gb->cs = GBK;
    gb->property = 0;

    return 1;
  }

  return 0;
}

#endif
