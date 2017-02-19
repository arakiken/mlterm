/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_tg_map.h"

#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"
#include "ef_ucs4_iso8859.h"
#include "ef_ucs4_koi8.h"

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_koi8_t, ef_map_ucs4_to_iso8859_5_r,
};

/* --- global functions --- */

int ef_map_ucs4_to_tg(ef_char_t *tg, ef_char_t *ucs4) {
  return ef_map_ucs4_to_with_funcs(tg, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_koi8_t_to_iso8859_5_r(ef_char_t *iso8859, ef_char_t *tg) {
  return ef_map_via_ucs(iso8859, tg, ISO8859_5_R);
}
