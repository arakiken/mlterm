/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_tcvn5712_1.h"

#include "table/ef_ucs4_to_tcvn5712_1993.table"
#include "table/ef_tcvn5712_1993_to_ucs4.table"

/* --- global functions --- */

/*
 * not compatible with ISO2022.
 * at the present time , not used.
 */
int ef_map_ucs4_to_tcvn5712_1_1993(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_TCVN5712_1993(ucs4_code))) {
    non_ucs->ch[0] = c;
  } else if (0x20 <= ucs4_code && ucs4_code <= 0x7f) {
    non_ucs->ch[0] = ucs4_code;
  } else {
    /*
     * combining chars.
     */

    if (ucs4_code == 0x300) {
      non_ucs->ch[0] = 0xb0;
    } else if (ucs4_code == 0x301) {
      non_ucs->ch[0] = 0xb3;
    } else if (ucs4_code == 0x303) {
      non_ucs->ch[0] = 0xb2;
    } else if (ucs4_code == 0x309) {
      non_ucs->ch[0] = 0xb1;
    } else if (ucs4_code == 0x323) {
      non_ucs->ch[0] = 0xb4;
    } else {
      return 0;
    }
  }

  non_ucs->size = 1;
  non_ucs->cs = TCVN5712_1_1993;
  non_ucs->property = 0;

  return 1;
}

/*
 * not compatible with ISO2022
 * at the present time , not used.
 */
int ef_map_tcvn5712_1_1993_to_ucs4(ef_char_t* ucs4, u_int16_t tcvn_code) {
  u_int32_t c;

  if ((c = CONV_TCVN5712_1993_TO_UCS4(tcvn_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
  } else if (0x20 <= tcvn_code && tcvn_code <= 0x7f) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = tcvn_code;
  } else {
    /*
     * combining chars.
     */

    u_char forth;

    if (tcvn_code == 0xb0) {
      forth = 0x0;
    } else if (tcvn_code == 0xb1) {
      forth = 0x09;
    } else if (tcvn_code == 0xb2) {
      forth = 0x03;
    } else if (tcvn_code == 0xb3) {
      forth = 0x01;
    } else if (tcvn_code == 0xb4) {
      forth = 0x23;
    } else {
      return 0;
    }

    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x03;
    ucs4->ch[3] = forth;
  }

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}
