/*
 *	$Id$
 */

#include "ef_zh_tw_map.h"

#include <pobl/bl_debug.h>

#include "ef_ucs4_map.h"
#include "ef_ucs4_cns11643.h"
#include "ef_ucs4_big5.h"

/* --- static variables --- */

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_big5, ef_map_ucs4_to_cns11643_1992_1, ef_map_ucs4_to_cns11643_1992_2,
    ef_map_ucs4_to_cns11643_1992_3,
};

/* --- global functions --- */

int ef_map_ucs4_to_zh_tw(ef_char_t* zhtw, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(zhtw, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

/*
 * BIG5 <=> CNS11643_1992_[1-2]
 */

int ef_map_big5_to_cns11643_1992(ef_char_t* cns, ef_char_t* big5) {
  ef_char_t ucs4;

  if (!ef_map_to_ucs4(&ucs4, big5)) {
    return 0;
  }

  if (!ef_map_ucs4_to_cs(cns, &ucs4, CNS11643_1992_1) &&
      !ef_map_ucs4_to_cs(cns, &ucs4, CNS11643_1992_2)) {
    return 0;
  }

  return 1;
}

int ef_map_cns11643_1992_1_to_big5(ef_char_t* big5, ef_char_t* cns) {
  return ef_map_via_ucs(big5, cns, BIG5);
}

int ef_map_cns11643_1992_2_to_big5(ef_char_t* big5, ef_char_t* cns) {
  return ef_map_via_ucs(big5, cns, BIG5);
}
