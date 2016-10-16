/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_zh_hk_map.h"

#include "ef_ucs4_map.h"
#include "ef_ucs4_big5.h"

/* --- static variables --- */

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_hkscs, ef_map_ucs4_to_big5,
};

/* --- global functions --- */

int ef_map_ucs4_to_zh_hk(ef_char_t* zhhk, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(zhhk, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}
