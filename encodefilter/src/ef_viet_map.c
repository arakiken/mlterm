/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_viet_map.h"

#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"
#include "ef_ucs4_viscii.h"
#include "ef_ucs4_iso8859.h"

/* --- static variables --- */

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_tcvn5712_3_1993, ef_map_ucs4_to_viscii,

};

/* --- global functions --- */

int ef_map_ucs4_to_viet(ef_char_t *viet, ef_char_t *ucs4) {
  return ef_map_ucs4_to_with_funcs(viet, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_viscii_to_tcvn5712_3_1993(ef_char_t *tcvn, ef_char_t *viscii) {
  return ef_map_via_ucs(tcvn, viscii, TCVN5712_3_1993);
}
