/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ru_map.h"

#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"
#include "ef_ucs4_iso8859.h"
#include "ef_ucs4_koi8.h"

/*
 * about KOI8-R <-> KOI8-U uncompatibility , see rfc2319.
 */
#define IS_NOT_COMPAT_AREA_OF_KOI8_R_U(ch)                                             \
  (ch == 0xa4 || ch == 0xa6 || ch == 0xa7 || ch == 0xad || ch == 0xb4 || ch == 0xb6 || \
   ch == 0xb7 || ch == 0xbd)

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_koi8_r, ef_map_ucs4_to_iso8859_5_r,
};

/* --- global functions --- */

int ef_map_ucs4_to_ru(ef_char_t* ru, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(ru, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_koi8_r_to_iso8859_5_r(ef_char_t* iso8859, ef_char_t* ru) {
  return ef_map_via_ucs(iso8859, ru, ISO8859_5_R);
}

int ef_map_koi8_r_to_koi8_u(ef_char_t* koi8_u, ef_char_t* koi8_r) {
  if (IS_NOT_COMPAT_AREA_OF_KOI8_R_U(koi8_r->ch[0])) {
    return 0;
  }

  *koi8_u = *koi8_r;
  koi8_u->cs = KOI8_U;

  return 1;
}

int ef_map_koi8_u_to_koi8_r(ef_char_t* koi8_r, ef_char_t* koi8_u) {
  if (IS_NOT_COMPAT_AREA_OF_KOI8_R_U(koi8_u->ch[0])) {
    return 0;
  }

  *koi8_r = *koi8_u;
  koi8_r->cs = KOI8_R;

  return 1;
}
