/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_big5.h"

#include "table/ef_big5_to_ucs4.table"
#include "table/ef_hkscs_to_ucs4.table"

#include "table/ef_ucs4_to_big5.table"
#include "table/ef_ucs4_to_hkscs.table"

/* --- global functions --- */

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
