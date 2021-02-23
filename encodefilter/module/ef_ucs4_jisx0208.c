/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ucs4_jisx0208.h"

#include "../src/ef_jis_property.h"
#include "table/ef_jisx0208_nec_ext_to_ucs4.table"
#include "table/ef_jisx0208_necibm_ext_to_ucs4.table"
#include "table/ef_sjis_ibm_ext_to_ucs4.table"

#ifdef USE_ICONV

#include "ef_iconv.h"

#else

#include "table/ef_jisx0208_1983_to_ucs4.table"
#include "table/ef_ucs4_to_jisx0208_1983.table"

#endif

/* --- global functions --- */

#ifdef USE_ICONV

int ef_map_jisx0208_1983_to_ucs4(ef_char_t *ucs4, u_int16_t jis) {
  static iconv_t cd;
  u_char src[2];

  ICONV_OPEN(cd, "UTF-32BE", "EUC-JP");

  src[1] = ((jis & 0x7f) | 0x80);
  src[0] = (((jis >> 8) & 0x7f) | 0x80);

  ICONV(cd, src, 2, ucs4->ch, 4);

  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

#else

int ef_map_jisx0208_1983_to_ucs4(ef_char_t *ucs4, u_int16_t jis) {
  u_int32_t c;

  if ((c = CONV_JISX0208_1983_TO_UCS4(jis))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#endif

int ef_map_jisx0208_nec_ext_to_ucs4(ef_char_t *ucs4, u_int16_t nec_ext) {
  u_int32_t c;

  if ((c = CONV_JISX0208_NEC_EXT_TO_UCS4(nec_ext))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_jisx0208_necibm_ext_to_ucs4(ef_char_t *ucs4, u_int16_t necibm_ext) {
  u_int32_t c;

  if ((c = CONV_JISX0208_NECIBM_EXT_TO_UCS4(necibm_ext))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_sjis_ibm_ext_to_ucs4(ef_char_t *ucs4, u_int16_t ibm_ext) {
  u_int32_t c;

  if ((c = CONV_SJIS_IBM_EXT_TO_UCS4(ibm_ext))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

#ifdef USE_ICONV

int ef_map_ucs4_to_jisx0208_1983(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  static iconv_t cd;

  ICONV_OPEN(cd, "EUC-JP", "UTF-32");

  ICONV(cd, (char*)&ucs4_code, 4, non_ucs->ch, 2);

  if (non_ucs->ch[0] == 0x8e) {
    /* Hankaku Kana */
    return 0;
  }

  non_ucs->size = 2;
  non_ucs->cs = JISX0208_1983;
  non_ucs->property = ef_get_jisx0208_1983_property(non_ucs->ch);

  non_ucs->ch[0] &= 0x7f;
  non_ucs->ch[1] &= 0x7f;

  return 1;
}

#else

int ef_map_ucs4_to_jisx0208_1983(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int16_t c;

  if ((c = CONV_UCS4_TO_JISX0208_1983(ucs4_code))) {
    ef_int_to_bytes(non_ucs->ch, 2, c);
    non_ucs->size = 2;
    non_ucs->cs = JISX0208_1983;
    non_ucs->property = ef_get_jisx0208_1983_property(non_ucs->ch);

    return 1;
  }

  return 0;
}

#endif

int ef_map_ucs4_to_jisx0208_nec_ext(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int16_t offset;

  for (offset = 0; offset <= jisx0208_nec_ext_to_ucs4_end - jisx0208_nec_ext_to_ucs4_beg;
       offset++) {
    if (jisx0208_nec_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code) {
      ef_int_to_bytes(non_ucs->ch, 2, offset + jisx0208_nec_ext_to_ucs4_beg);
      non_ucs->cs = JISC6226_1978_NEC_EXT;
      non_ucs->size = 2;
      non_ucs->property = 0;

      return 1;
    }
  }

  return 0;
}

int ef_map_ucs4_to_jisx0208_necibm_ext(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int16_t offset;

  for (offset = 0; offset <= jisx0208_necibm_ext_to_ucs4_end - jisx0208_necibm_ext_to_ucs4_beg;
       offset++) {
    if (jisx0208_necibm_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code) {
      ef_int_to_bytes(non_ucs->ch, 2, offset + jisx0208_necibm_ext_to_ucs4_beg);
      non_ucs->cs = JISC6226_1978_NECIBM_EXT;
      non_ucs->size = 2;
      non_ucs->property = 0;

      return 1;
    }
  }

  return 0;
}

int ef_map_ucs4_to_sjis_ibm_ext(ef_char_t *non_ucs, u_int32_t ucs4_code) {
  u_int16_t offset;

  for (offset = 0; offset <= sjis_ibm_ext_to_ucs4_end - sjis_ibm_ext_to_ucs4_beg; offset++) {
    if (sjis_ibm_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code) {
      ef_int_to_bytes(non_ucs->ch, 2, offset + sjis_ibm_ext_to_ucs4_beg);
      non_ucs->cs = SJIS_IBM_EXT;
      non_ucs->size = 2;
      non_ucs->property = 0;

      return 1;
    }
  }

  return 0;
}
