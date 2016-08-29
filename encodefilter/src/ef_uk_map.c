/*
 *	$Id$
 */

#include "ef_uk_map.h"

#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"
#include "ef_ucs4_iso8859.h"
#include "ef_ucs4_koi8.h"

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_koi8_u, ef_map_ucs4_to_iso8859_5_r,
};

/* --- global functions --- */

int ef_map_ucs4_to_uk(ef_char_t* uk, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(uk, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_koi8_u_to_iso8859_5_r(ef_char_t* iso8859, ef_char_t* uk) {
  return ef_map_via_ucs(iso8859, uk, ISO8859_5_R);
}
