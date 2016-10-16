/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_cns11643.h"

#include "table/ef_cns11643_1992_1_to_ucs4.table"
#include "table/ef_cns11643_1992_2_to_ucs4.table"
#include "table/ef_cns11643_1992_3_to_ucs4.table"

#include "table/ef_ucs4_to_cns11643_1992_1.table"
#include "table/ef_ucs4_to_cns11643_1992_2.table"
#include "table/ef_ucs4_to_cns11643_1992_3.table"

/* --- global functions --- */

int ef_map_cns11643_1992_1_to_ucs4(ef_char_t* ucs4, u_int16_t cns) {
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

int ef_map_cns11643_1992_2_to_ucs4(ef_char_t* ucs4, u_int16_t cns) {
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

int ef_map_cns11643_1992_3_to_ucs4(ef_char_t* ucs4, u_int16_t cns) {
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

int ef_map_ucs4_to_cns11643_1992_1(ef_char_t* cns, u_int32_t ucs4_code) {
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

int ef_map_ucs4_to_cns11643_1992_2(ef_char_t* cns, u_int32_t ucs4_code) {
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

int ef_map_ucs4_to_cns11643_1992_3(ef_char_t* cns, u_int32_t ucs4_code) {
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
