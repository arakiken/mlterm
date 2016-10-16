/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ko_kr_map.h"

#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_ucs4_ksc5601.h"
#include "ef_ucs4_uhc.h"
#include "ef_ucs4_johab.h"
#include "ef_tblfunc_loader.h"

/*
 * johab <=> ucs4 conversion is so simple without conversion table that
 * ucs4 -> johab conversion is done first of all.
 */
static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_johab, ef_map_ucs4_to_ksc5601_1987, ef_map_ucs4_to_uhc,

};

/* --- global functions --- */

int ef_map_ucs4_to_ko_kr(ef_char_t* kokr, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(kokr, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_uhc_to_ksc5601_1987(ef_char_t* ksc, ef_char_t* uhc) {
  if (0xa1 <= uhc->ch[0] && uhc->ch[0] <= 0xfe && 0xa1 <= uhc->ch[1] && uhc->ch[1] <= 0xfe) {
    ksc->ch[0] = UNMAP_FROM_GR(uhc->ch[0]);
    ksc->ch[1] = UNMAP_FROM_GR(uhc->ch[1]);
    ksc->size = 2;
    ksc->cs = KSC5601_1987;

    return 1;
  } else {
    return 0;
  }
}

int ef_map_ksc5601_1987_to_uhc(ef_char_t* uhc, ef_char_t* ksc) {
  uhc->ch[0] = MAP_TO_GR(ksc->ch[0]);
  uhc->ch[1] = MAP_TO_GR(ksc->ch[1]);
  uhc->size = 2;
  uhc->cs = UHC;

  return 1;
}

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_uhc_johab.c"

#else

ef_map_func2(kokr, ef_map_johab_to_uhc)

    ef_map_func2(kokr, ef_map_uhc_to_johab)

#endif
