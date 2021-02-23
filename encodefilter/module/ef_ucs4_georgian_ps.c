/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_georgian_ps.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_georgian_ps_to_ucs4.table"
#include "table/ef_ucs4_to_georgian_ps.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_georgian_ps_to_ucs4(ef_char_t *ucs4, u_int16_t gp_code) {
  static iconv_t cd;
  u_char src[1];

  ICONV_OPEN(cd, "UTF-32BE", "GEORGIAN-PS");

  src[0] = gp_code;

  ICONV(cd, src, 1, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_georgian_ps(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "GEORGIAN-PS", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, non_ucs->ch, 1);

  non_ucs->size = 1;
  non_ucs->cs = GEORGIAN_PS;
  non_ucs->property = 0;

  return 1;
}

#else

int ef_map_georgian_ps_to_ucs4(ef_char_t *ucs4, u_int16_t gp_code) {
  u_int32_t c;

  if ((c = CONV_GEORGIAN_PS_TO_UCS4(gp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_georgian_ps(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_GEORGIAN_PS(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = GEORGIAN_PS;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif
