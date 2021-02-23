/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_koi8.h"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_koi8_r_to_ucs4.table"
#if 0
/* Not implemented yet */
#include "table/ef_ucs4_to_koi8_r.table"
#endif

#include "table/ef_koi8_t_to_ucs4.table"
#include "table/ef_ucs4_to_koi8_t.table"

#endif

/* --- static functions --- */

#ifdef USE_ICONV

static int map_koi8_to_ucs4(iconv_t *cd, ef_char_t *ucs4,
                            u_int16_t koi8_code, char *koi8) {
  u_char src[1];

  ICONV_OPEN(*cd, "UTF-32BE", koi8);

  src[0] = koi8_code;

  ICONV(*cd, src, 1, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

static int map_ucs4_to_koi8(iconv_t *cd, ef_char_t *non_ucs,
                            u_int32_t ucs4_code, char *koi8, ef_charset_t cs) {
  ICONV_OPEN(*cd, koi8, "UTF-32");

  ICONV(*cd, (char*)&ucs4_code, 4, non_ucs->ch, 1);

  non_ucs->size = 1;
  non_ucs->cs = cs;
  non_ucs->property = 0;

  return 1;
}

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_koi8_r_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  static iconv_t cd;

  return map_koi8_to_ucs4(&cd, ucs4, koi8_code, "KOI8-R");
}

int ef_map_koi8_u_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  static iconv_t cd;

  return map_koi8_to_ucs4(&cd, ucs4, koi8_code, "KOI8-U");
}

int ef_map_koi8_t_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  static iconv_t cd;

  return map_koi8_to_ucs4(&cd, ucs4, koi8_code, "KOI8-T");
}

int ef_map_ucs4_to_koi8_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_koi8(&cd, non_ucs, ucs4_code, "KOI8-R", KOI8_R);
}

int ef_map_ucs4_to_koi8_u(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_koi8(&cd, non_ucs, ucs4_code, "KOI8-U", KOI8_U);
}

int ef_map_ucs4_to_koi8_t(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  return map_ucs4_to_koi8(&cd, non_ucs, ucs4_code, "KOI8-T", KOI8_T);
}

#else

int ef_map_koi8_r_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  u_int32_t c;

  if ((c = CONV_KOI8_R_TO_UCS4(koi8_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_koi8_u_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  /*
   * about KOI8-R <-> KOI8-U incompatibility , see rfc2319.
   * note that the appendix A of rfc2319 is broken.
   */
  if (koi8_code == 0xa4 || koi8_code == 0xa6 || koi8_code == 0xa7) {
    ucs4->ch[3] = 0x54 + (koi8_code - 0xa4);
  } else if (koi8_code == 0xb6 || koi8_code == 0xb7) {
    ucs4->ch[3] = 0x06 + (koi8_code - 0xb6);
  } else if (koi8_code == 0xad) {
    ucs4->ch[3] = 0x91;
  } else if (koi8_code == 0xb4) {
    ucs4->ch[3] = 0x04;
  } else if (koi8_code == 0xbd) {
    ucs4->ch[3] = 0x90;
  } else if (ef_map_koi8_r_to_ucs4(ucs4, koi8_code)) {
    return 1;
  } else {
    return 0;
  }

  ucs4->ch[0] = 0x0;
  ucs4->ch[1] = 0x0;
  ucs4->ch[2] = 0x04;

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_koi8_t_to_ucs4(ef_char_t *ucs4, u_int16_t koi8_code) {
  u_int32_t c;

  if ((c = CONV_KOI8_T_TO_UCS4(koi8_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_koi8_r(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t offset;

  for (offset = 0; offset <= koi8_r_to_ucs4_end - koi8_r_to_ucs4_beg; offset++) {
    if (koi8_r_to_ucs4_table[offset] == (u_int16_t)ucs4_code) {
      non_ucs->ch[0] = offset + koi8_r_to_ucs4_beg;
      non_ucs->size = 1;
      non_ucs->cs = KOI8_R;
      non_ucs->property = 0;

      return 1;
    }
  }

  return 0;
}

int ef_map_ucs4_to_koi8_u(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  /*
   * about KOI8-R <-> KOI8-U incompatibility , see rfc2319.
   * note that the appendix A of rfc2319 is broken.
   */
  if (ucs4_code == 0x454 || ucs4_code == 0x456 || ucs4_code == 0x457) {
    non_ucs->ch[0] = 0xa4 + ucs4_code - 0x454;
  } else if (ucs4_code == 0x406 || ucs4_code == 0x407) {
    non_ucs->ch[0] = 0xb6 + ucs4_code - 0x406;
  } else if (ucs4_code == 0x491) {
    non_ucs->ch[0] = 0xad;
  } else if (ucs4_code == 0x404) {
    non_ucs->ch[0] = 0xb4;
  } else if (ucs4_code == 0x490) {
    non_ucs->ch[0] = 0xbd;
  } else if (ef_map_ucs4_to_koi8_r(non_ucs, ucs4_code)) {
    non_ucs->cs = KOI8_U;

    return 1;
  } else {
    return 0;
  }

  non_ucs->size = 1;
  non_ucs->cs = KOI8_U;
  non_ucs->property = 0;

  return 1;
}

int ef_map_ucs4_to_koi8_t(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_KOI8_T(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = KOI8_T;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#endif
