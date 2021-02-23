/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_viscii.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_ucs4_to_viscii.table"
#include "table/ef_viscii_to_ucs4.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_viscii_to_ucs4(ef_char_t *ucs4, u_int16_t viscii_code) {
  static iconv_t cd;
  u_char src[1];

  ICONV_OPEN(cd, "UTF-32BE", "VISCII");

  src[0] = viscii_code;

  ICONV(cd, src, 1, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_ucs4_to_viscii(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "VISCII", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, non_ucs->ch, 1);

  non_ucs->size = 1;
  non_ucs->cs = VISCII;

  /* See also ef_8bit_parser.c */
  if (0xb0 <= non_ucs->ch[0] && non_ucs->ch[0] <= 0xb4) {
    non_ucs->property = EF_COMBINING;
  } else {
    non_ucs->property = 0;
  }

  return 1;
}

#else

int ef_map_viscii_to_ucs4(ef_char_t *ucs4, u_int16_t viscii_code) {
  u_int32_t c;

  if ((c = CONV_VISCII_TO_UCS4(viscii_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= viscii_code && viscii_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = viscii_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_viscii(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_VISCII(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = VISCII;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif
