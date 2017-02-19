/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <string.h> /* memcpy */

#include "ef_ja_jp_map.h"

#include "ef_ucs4_map.h"
#include "ef_ucs4_jisx0201.h"
#include "ef_ucs4_jisx0208.h"
#include "ef_ucs4_jisx0212.h"
#include "ef_ucs4_jisx0213.h"

/* --- static variables --- */

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_jisx0201_roman,  ef_map_ucs4_to_jisx0201_kata,
    ef_map_ucs4_to_jisx0208_1983,   ef_map_ucs4_to_jisx0212_1990,
    ef_map_ucs4_to_jisx0213_2000_1, ef_map_ucs4_to_jisx0213_2000_2,

};

/* --- global functions --- */

int ef_map_ucs4_to_ja_jp(ef_char_t *jajp, ef_char_t *ucs4) {
  return ef_map_ucs4_to_with_funcs(jajp, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_jisx0213_2000_1_to_jisx0208_1983(ef_char_t *jis2k, ef_char_t *jis83) {
  memcpy(jis2k->ch, jis83->ch, 2);
  jis2k->size = 2;
  jis2k->cs = JISX0213_2000_1;
  jis2k->property = jis83->property;

  return 1;
}

int ef_map_jisx0208_1983_to_jisx0213_2000_1(ef_char_t *jis83, ef_char_t *jis2k) {
  /*
   * XXX
   * since JISX0213-1 is upper compatible with JISX0208 , some chars of
   * JISX0213-1
   * must not be mapped to JISX0208.
   */
  memcpy(jis83->ch, jis2k->ch, 2);
  jis83->size = 2;
  jis83->cs = JISX0213_2000_1;
  jis83->property = jis2k->property;

  return 1;
}

int ef_map_jisx0213_2000_2_to_jisx0212_1990(ef_char_t *jis90, ef_char_t *jis2k) {
  return ef_map_via_ucs(jis2k, jis90, JISX0213_2000_2);
}

int ef_map_jisx0212_1990_to_jisx0213_2000_2(ef_char_t *jis2k, ef_char_t *jis90) {
  return ef_map_via_ucs(jis90, jis2k, JISX0212_1990);
}

/*
 * Gaiji characters
 */

/* SJIS_IBM_EXT */

int ef_map_sjis_ibm_ext_to_jisx0208_1983(ef_char_t *jis, ef_char_t *ibm) {
  return ef_map_via_ucs(jis, ibm, JISX0208_1983);
}

int ef_map_sjis_ibm_ext_to_jisx0212_1990(ef_char_t *jis, ef_char_t *ibm) {
  return ef_map_via_ucs(jis, ibm, JISX0212_1990);
}

int ef_map_sjis_ibm_ext_to_jisx0213_2000(ef_char_t *jis, ef_char_t *ibm) {
  ef_char_t ucs4;

  if (!ef_map_to_ucs4(&ucs4, ibm)) {
    return 0;
  }

  if (!ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_2) &&
      !ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_1)) {
    return 0;
  }

  return 1;
}

int ef_map_jisx0212_1990_to_sjis_ibm_ext(ef_char_t *ibm, ef_char_t *jis) {
  return ef_map_via_ucs(ibm, jis, SJIS_IBM_EXT);
}

int ef_map_jisx0213_2000_2_to_sjis_ibm_ext(ef_char_t *ibm, ef_char_t *jis) {
  return ef_map_via_ucs(ibm, jis, SJIS_IBM_EXT);
}

/* NEC EXT */

int ef_map_jisx0208_nec_ext_to_jisx0208_1983(ef_char_t *jis, ef_char_t *nec_ext) {
  return ef_map_via_ucs(jis, nec_ext, JISX0208_1983);
}

int ef_map_jisx0208_nec_ext_to_jisx0212_1990(ef_char_t *jis, ef_char_t *nec_ext) {
  return ef_map_via_ucs(jis, nec_ext, JISX0212_1990);
}

int ef_map_jisx0208_nec_ext_to_jisx0213_2000(ef_char_t *jis, ef_char_t *nec_ext) {
  ef_char_t ucs4;

  if (!ef_map_to_ucs4(&ucs4, nec_ext)) {
    return 0;
  }

  if (!ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_2) &&
      !ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_1)) {
    return 0;
  }

  return 1;
}

int ef_map_jisx0212_1990_to_jisx0208_nec_ext(ef_char_t *nec_ext, ef_char_t *jis) {
  return ef_map_via_ucs(nec_ext, jis, JISC6226_1978_NEC_EXT);
}

int ef_map_jisx0213_2000_2_to_jisx0208_nec_ext(ef_char_t *nec_ext, ef_char_t *jis) {
  return ef_map_via_ucs(nec_ext, jis, JISC6226_1978_NEC_EXT);
}

/* NEC IBM EXT */

int ef_map_jisx0208_necibm_ext_to_jisx0208_1983(ef_char_t *jis, ef_char_t *necibm) {
  return ef_map_via_ucs(jis, necibm, JISX0208_1983);
}

int ef_map_jisx0208_necibm_ext_to_jisx0212_1990(ef_char_t *jis, ef_char_t *necibm) {
  return ef_map_via_ucs(jis, necibm, JISX0212_1990);
}

int ef_map_jisx0208_necibm_ext_to_jisx0213_2000(ef_char_t *jis, ef_char_t *necibm) {
  ef_char_t ucs4;

  if (!ef_map_to_ucs4(&ucs4, necibm)) {
    return 0;
  }

  if (!ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_2) &&
      !ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_1)) {
    return 0;
  }

  return 1;
}

int ef_map_jisx0212_1990_to_jisx0208_necibm_ext(ef_char_t *necibm, ef_char_t *jis) {
  return ef_map_via_ucs(necibm, jis, JISC6226_1978_NECIBM_EXT);
}

int ef_map_jisx0213_2000_2_to_jisx0208_necibm_ext(ef_char_t *necibm, ef_char_t *jis) {
  return ef_map_via_ucs(necibm, jis, JISC6226_1978_NECIBM_EXT);
}

/* MAC EXT */

int ef_map_jisx0208_mac_ext_to_jisx0208_1983(ef_char_t *jis, ef_char_t *mac) {
  return ef_map_via_ucs(jis, mac, JISX0208_1983);
}

int ef_map_jisx0208_mac_ext_to_jisx0212_1990(ef_char_t *jis, ef_char_t *mac) {
  return ef_map_via_ucs(jis, mac, JISX0212_1990);
}

int ef_map_jisx0208_mac_ext_to_jisx0213_2000(ef_char_t *jis, ef_char_t *mac) {
  ef_char_t ucs4;

  if (!ef_map_to_ucs4(&ucs4, mac)) {
    return 0;
  }

  if (!ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_2) &&
      !ef_map_ucs4_to_cs(jis, &ucs4, JISX0213_2000_1)) {
    return 0;
  }

  return 1;
}

int ef_map_jisx0212_1990_to_jisx0208_mac_ext(ef_char_t *mac, ef_char_t *jis) {
  return ef_map_via_ucs(mac, jis, JISX0208_1983_MAC_EXT);
}

int ef_map_jisx0213_2000_2_to_jisx0208_mac_ext(ef_char_t *mac, ef_char_t *jis) {
  return ef_map_via_ucs(mac, jis, JISX0208_1983_MAC_EXT);
}
