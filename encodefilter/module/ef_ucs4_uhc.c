/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_uhc.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_uhc_to_ucs4.table"
#include "table/ef_ucs4_to_uhc.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_uhc_to_ucs4(ef_char_t *ucs4, u_int16_t ks) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "UHC");

  src[1] = ks & 0xff;
  src[0] = (ks >> 8) & 0xff;

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_uhc(ef_char_t *ks, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "UHC", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, ks->ch, 2);

  ks->size = 2;
  ks->cs = UHC;
  ks->property = 0;

  return 1;
}

#else

int ef_map_uhc_to_ucs4(ef_char_t *ucs4, u_int16_t ks) {
  u_int32_t c;

  if ((c = CONV_UHC_TO_UCS4(ks))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_uhc(ef_char_t *ks, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_UHC(ucs4_code))) {
    ef_int_to_bytes(ks->ch, 2, c);
    ks->size = 2;
    ks->cs = UHC;
    ks->property = 0;

    return 1;
  }

  return 0;
}

#endif
