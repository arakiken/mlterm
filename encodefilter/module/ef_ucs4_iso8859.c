/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * !!!NOTICE!!!
 *
 * ISO8859_N msb bit is 0 in mef , and including 0x20 and 0x7f.
 */

#include "../src/ef_ucs4_iso8859.h"

#include <string.h>
#include <pobl/bl_debug.h>

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_iso8859_2_r_to_ucs4.table"
#include "table/ef_iso8859_3_r_to_ucs4.table"
#include "table/ef_iso8859_4_r_to_ucs4.table"
#include "table/ef_iso8859_10_r_to_ucs4.table"
#include "table/ef_iso8859_13_r_to_ucs4.table"
#include "table/ef_iso8859_14_r_to_ucs4.table"
#include "table/ef_iso8859_16_r_to_ucs4.table"

#include "table/ef_ucs4_to_iso8859_2_r.table"
#include "table/ef_ucs4_to_iso8859_3_r.table"
#include "table/ef_ucs4_to_iso8859_4_r.table"
#include "table/ef_ucs4_to_iso8859_10_r.table"
#include "table/ef_ucs4_to_iso8859_13_r.table"
#include "table/ef_ucs4_to_iso8859_14_r.table"
#include "table/ef_ucs4_to_iso8859_16_r.table"

#endif

#include "../src/ef_ucs4_tcvn5712_1.h"

/* --- static functions --- */

static int convert_ucs4_to_iso8859_r_common(ef_char_t *non_ucs, u_int32_t ucs4_code,
                                            ef_charset_t cs) {
  if (!(0xa0 <= ucs4_code && ucs4_code <= 0xff)) {
    return 0;
  }

  non_ucs->ch[0] = ucs4_code - 0x80;
  non_ucs->size = 1;
  non_ucs->cs = cs;
  non_ucs->property = 0;

  return 1;
}

static int convert_iso8859_r_common_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (!(0x20 <= iso8859_code && iso8859_code <= 0x7f)) {
    return 0;
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->ch[2] = 0x0;
  ucs4->ch[3] = iso8859_code + 0x80;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#ifdef USE_ICONV

static int map_ucs4_to_iso8859(iconv_t *cd, ef_char_t *non_ucs,
                               u_int32_t ucs4_code, char *iso8859, ef_charset_t cs) {
  ICONV_OPEN(*cd, iso8859, "UTF-32");

  ICONV(*cd, (char*)&ucs4_code, 4, non_ucs->ch, 1);

  non_ucs->ch[0] &= 0x7f;
  non_ucs->size = 1;
  non_ucs->cs = cs;
  non_ucs->property = 0;

  return 1;
}

static int map_iso8859_to_ucs4(iconv_t *cd, ef_char_t *ucs4,
                               u_int16_t iso8859_code, char *iso8859) {
  u_char src[1];

  ICONV_OPEN(*cd, "UTF-32BE", iso8859);

  src[0] = iso8859_code | 0x80;

  ICONV(*cd, src, 1, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#endif

/* --- global functions --- */

int ef_map_ucs4_to_iso8859_1_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_1_R);
}

#ifdef USE_ICONV

int ef_map_ucs4_to_iso8859_2_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-2", ISO8859_2_R);
}

int ef_map_ucs4_to_iso8859_3_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-3", ISO8859_3_R);
}

int ef_map_ucs4_to_iso8859_4_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-4", ISO8859_4_R);
}

#else

int ef_map_ucs4_to_iso8859_2_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_2_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_2_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_iso8859_3_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_3_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_3_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_iso8859_4_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_4_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_4_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_ucs4_to_iso8859_5_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x2116) {
    non_ucs->ch[0] = 0x70;
  } else if ((0x0401 <= ucs4_code && ucs4_code <= 0x040c) ||
             (0x040e <= ucs4_code && ucs4_code <= 0x044f) ||
             (0x0451 <= ucs4_code && ucs4_code <= 0x045c) ||
             (0x045e <= ucs4_code && ucs4_code <= 0x045f)) {
    non_ucs->ch[0] = (ucs4_code & 0x00ff) + 0x20;
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_5_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_5_R;
  non_ucs->property = 0;

  return 1;
}

int ef_map_ucs4_to_iso8859_6_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x060c) {
    non_ucs->ch[0] = 0x2c;
  } else if (0x061b <= ucs4_code && ucs4_code <= 0x0652) {
    non_ucs->ch[0] = (ucs4_code + 0x20) & 0xff;
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_6_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_6_R;
  non_ucs->property = 0;

  return 1;
}

int ef_map_ucs4_to_iso8859_7_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x2015) {
    non_ucs->ch[0] = 0x2f;
  } else if (0x2018 <= ucs4_code && ucs4_code <= 0x2019) {
    non_ucs->ch[0] = 0x21 + (ucs4_code - 0x2018);
  } else if ((0x0384 <= ucs4_code && ucs4_code <= 0x0386) ||
             (0x0388 <= ucs4_code && ucs4_code <= 0x038a) || (ucs4_code == 0x038c) ||
             (0x038e <= ucs4_code && ucs4_code <= 0x03ce)) {
    non_ucs->ch[0] = (ucs4_code - 0x50) & 0x00ff;
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_7_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_7_R;
  non_ucs->property = 0;

  return 1;
}

int ef_map_ucs4_to_iso8859_8_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x2017) {
    non_ucs->ch[0] = 0x5f;
  } else if (0x05d0 <= ucs4_code && ucs4_code <= 0x05ea) {
    non_ucs->ch[0] = (ucs4_code - 0x70) & 0xff;
  } else if (0x200e <= ucs4_code && ucs4_code <= 0x200f) {
    non_ucs->ch[0] = 0x7d + (ucs4_code - 0x200e);
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_8_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_8_R;
  non_ucs->property = 0;

  return 1;
}

int ef_map_ucs4_to_iso8859_9_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x011e) {
    non_ucs->ch[0] = 0x50;
  } else if (ucs4_code == 0x0130) {
    non_ucs->ch[0] = 0x5d;
  } else if (ucs4_code == 0x015e) {
    non_ucs->ch[0] = 0x5e;
  } else if (ucs4_code == 0x011f) {
    non_ucs->ch[0] = 0x70;
  } else if (ucs4_code == 0x0131) {
    non_ucs->ch[0] = 0x7d;
  } else if (ucs4_code == 0x015f) {
    non_ucs->ch[0] = 0x7e;
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_9_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_9_R;
  non_ucs->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_ucs4_to_iso8859_10_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-10", ISO8859_10_R);
}

#else

int ef_map_ucs4_to_iso8859_10_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_10_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_10_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_ucs4_to_tis620_2533(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0xa0) {
    non_ucs->ch[0] = 0x20;
  } else if (0xe01 <= ucs4_code && ucs4_code <= 0xe5f) {
    non_ucs->ch[0] = ucs4_code - 0xde0;
  }
#if 0
  else if (0xe60 <= ucs4_code && ucs4_code <= 0x7f) {
    /* this region is removed between UNICODDE 1.0 and UNICODE 1.1. */
  }
#endif
  else {
    return 0;
  }

  non_ucs->size = 1;
  non_ucs->cs = TIS620_2533;
  non_ucs->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_ucs4_to_iso8859_13_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-13", ISO8859_13_R);
}

int ef_map_ucs4_to_iso8859_14_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-14", ISO8859_14_R);
}

#else

int ef_map_ucs4_to_iso8859_13_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_13_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_13_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_iso8859_14_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_14_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_14_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_ucs4_to_iso8859_15_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (ucs4_code == 0x20ac) {
    non_ucs->ch[0] = 0x24;
  } else if (ucs4_code == 0x0160) {
    non_ucs->ch[0] = 0x26;
  } else if (ucs4_code == 0x0161) {
    non_ucs->ch[0] = 0x28;
  } else if (ucs4_code == 0x017d) {
    non_ucs->ch[0] = 0x34;
  } else if (ucs4_code == 0x017e) {
    non_ucs->ch[0] = 0x38;
  } else if (ucs4_code == 0x0152 || ucs4_code == 0x0153) {
    non_ucs->ch[0] = 0x3c + (ucs4_code - 0x0152);
  } else if (ucs4_code == 0x0178) {
    non_ucs->ch[0] = 0x3e;
  } else {
    return convert_ucs4_to_iso8859_r_common(non_ucs, ucs4_code, ISO8859_15_R);
  }

  non_ucs->size = 1;
  non_ucs->cs = ISO8859_15_R;
  non_ucs->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_ucs4_to_iso8859_16_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_iso8859(&cd, non_ucs, ucs4_code, "ISO_8859-16", ISO8859_16_R);
}

#else

int ef_map_ucs4_to_iso8859_16_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_ISO8859_16_R(ucs4_code))) {
    non_ucs->ch[0] = c - 0x80;
    non_ucs->size = 1;
    non_ucs->cs = ISO8859_16_R;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_ucs4_to_tcvn5712_3_1993(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  if (!ef_map_ucs4_to_tcvn5712_1_1993(non_ucs, ucs4_code)) {
    return 0;
  }

  if (non_ucs->ch[0] < 0xa0) {
    return 0;
  }

  non_ucs->ch[0] -= 0x80;
  non_ucs->size = 1;
  non_ucs->cs = TCVN5712_3_1993;
  non_ucs->property = 0;

  return 1;
}

int ef_map_iso8859_1_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
}

#ifdef USE_ICONV

int ef_map_iso8859_2_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-2");
}

int ef_map_iso8859_3_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-3");
}

int ef_map_iso8859_4_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-4");
}

#else

int ef_map_iso8859_2_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_2_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_iso8859_3_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_3_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_iso8859_4_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_4_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_iso8859_5_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (iso8859_code == 0x70) {
    ucs4->ch[2] = 0x21;
    ucs4->ch[3] = 0x16;
  } else if ((0x21 <= iso8859_code && iso8859_code <= 0x2c) ||
             (0x2e <= iso8859_code && iso8859_code <= 0x6f) ||
             (0x71 <= iso8859_code && iso8859_code <= 0x7c) ||
             (0x7e <= iso8859_code && iso8859_code <= 0x7f)) {
    ucs4->ch[2] = 0x04;
    ucs4->ch[3] = iso8859_code - 0x20;
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_iso8859_6_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (iso8859_code == 0x2c) {
    ucs4->ch[2] = 0x06;
    ucs4->ch[3] = 0x0c;
  } else if (0x3b <= iso8859_code && iso8859_code <= 0x72) {
    ucs4->ch[2] = 0x06;
    ucs4->ch[3] = (iso8859_code - 0x20) & 0xff;
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_iso8859_7_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (0x21 <= iso8859_code && iso8859_code <= 0x22) {
    ucs4->ch[2] = 0x20;
    ucs4->ch[3] = 0x18 + (iso8859_code - 0x21);
  } else if (iso8859_code == 0x2f) {
    ucs4->ch[2] = 0x20;
    ucs4->ch[3] = 0x15;
  } else if ((0x34 <= iso8859_code && iso8859_code <= 0x36) ||
             (0x38 <= iso8859_code && iso8859_code <= 0x3a) || (0x3c == iso8859_code) ||
             (0x3e <= iso8859_code && iso8859_code <= 0x7e)) {
    ucs4->ch[2] = 0x03;
    ucs4->ch[3] = iso8859_code + 0x50;
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_iso8859_8_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (iso8859_code == 0xdf) {
    ucs4->ch[2] = 0x20;
    ucs4->ch[3] = 0x17;
  } else if (0x60 <= iso8859_code && iso8859_code <= 0x7a) {
    ucs4->ch[2] = 0x05;
    ucs4->ch[3] = iso8859_code + 0x70;
  } else if (0x7d <= iso8859_code && iso8859_code <= 0x7e) {
    ucs4->ch[2] = 0x20;
    ucs4->ch[3] = 0x0e + (iso8859_code - 0x7d);
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_iso8859_9_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (iso8859_code == 0x50) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x1e;
  } else if (iso8859_code == 0x5d) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x30;
  } else if (iso8859_code == 0x5e) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x5e;
  } else if (iso8859_code == 0x70) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x1f;
  } else if (iso8859_code == 0x7d) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x31;
  } else if (iso8859_code == 0x7e) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x5f;
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_iso8859_10_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-10");
}

#else

int ef_map_iso8859_10_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_10_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_tis620_2533_to_ucs4(ef_char_t *ucs4, u_int16_t tis620_code) {
  if (0x20 == tis620_code) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = 0xa0;
  } else if (0x21 <= tis620_code && tis620_code <= 0x7f) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0e;
    ucs4->ch[3] = tis620_code - 0x20;
  } else {
    return 0;
  }

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_iso8859_13_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-13");
}

int ef_map_iso8859_14_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-14");
}

#else

int ef_map_iso8859_13_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_13_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_iso8859_14_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_14_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_iso8859_15_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  if (iso8859_code == 0x24) {
    ucs4->ch[2] = 0x20;
    ucs4->ch[3] = 0xac;
  } else if (iso8859_code == 0x26) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x60;
  } else if (iso8859_code == 0x28) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x61;
  } else if (iso8859_code == 0x34) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x7d;
  } else if (iso8859_code == 0x38) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x7e;
  } else if (iso8859_code == 0x3c || iso8859_code == 0x3d) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x52 + (iso8859_code - 0x3c);
  } else if (iso8859_code == 0xbe) {
    ucs4->ch[2] = 0x01;
    ucs4->ch[3] = 0x78;
  } else {
    return convert_iso8859_r_common_to_ucs4(ucs4, iso8859_code);
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#ifdef USE_ICONV

int ef_map_iso8859_16_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  static iconv_t cd;

  return map_iso8859_to_ucs4(&cd, ucs4, iso8859_code, "ISO_8859-16");
}

#else

int ef_map_iso8859_16_r_to_ucs4(ef_char_t *ucs4, u_int16_t iso8859_code) {
  u_int32_t c;

  if ((c = CONV_ISO8859_16_R_TO_UCS4(iso8859_code + 0x80))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_tcvn5712_3_1993_to_ucs4(ef_char_t *ucs4, u_int16_t tcvn_code) {
  if (tcvn_code < 0x20) {
    return 0;
  }

  return ef_map_tcvn5712_1_1993_to_ucs4(ucs4, tcvn_code + 0x80);
}
