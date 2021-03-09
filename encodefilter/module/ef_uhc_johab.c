/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ko_kr_map.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_johab_to_uhc.table"
#include "table/ef_uhc_to_johab.table"

#endif

#if 0
#define SELF_TEST
#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_johab_to_uhc(ef_char_t *uhc, ef_char_t *johab) {
  static iconv_t cd;

  ICONV_OPEN(cd, "UHC", "JOHAB");

  ICONV(cd, johab->ch, 2, uhc->ch, 2);

  uhc->size = 2;
  uhc->cs = UHC;
  uhc->property = 0;

  return 1;
}

int ef_map_uhc_to_johab(ef_char_t *johab, ef_char_t *uhc) {
  static iconv_t cd;

  ICONV_OPEN(cd, "JOHAB", "UHC");

  ICONV(cd, uhc->ch, 2, johab->ch, 2);

  johab->size = 2;
  johab->cs = UHC;
  johab->property = 0;

  return 1;
}

#else

int ef_map_johab_to_uhc(ef_char_t *uhc, ef_char_t *johab) {
  u_int16_t johab_code;
  u_int16_t c;

  johab_code = ef_char_to_int(johab);

  if ((c = CONV_JOHAB_TO_UHC(johab_code))) {
    ef_int_to_bytes(uhc->ch, 2, c);
    uhc->size = 2;
    uhc->cs = UHC;

    return 1;
  }

  return 0;
}

int ef_map_uhc_to_johab(ef_char_t *johab, ef_char_t *uhc) {
  u_int16_t uhc_code;
  u_int16_t c;

  uhc_code = ef_char_to_int(uhc);

  if ((c = CONV_UHC_TO_JOHAB(uhc_code))) {
    ef_int_to_bytes(johab->ch, 2, c);
    johab->size = 2;
    johab->cs = JOHAB;

    return 1;
  }

  return 0;
}

#endif
