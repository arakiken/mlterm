/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_locale_ucs4_map.h"

#include <string.h> /* strncmp */
#include <pobl/bl_locale.h>

#include "ef_ucs4_map.h"
#include "ef_ja_jp_map.h"
#include "ef_ko_kr_map.h"
#include "ef_ru_map.h"
#include "ef_tg_map.h"
#include "ef_uk_map.h"
#include "ef_viet_map.h"
#include "ef_zh_cn_map.h"
#include "ef_zh_hk_map.h"
#include "ef_zh_tw_map.h"

typedef int (*map_func_t)(ef_char_t *, ef_char_t *);

typedef struct map_ucs4_to_func_table {
  char *lang;
  char *country;
  map_func_t func;

} map_ucs4_to_func_table_t;

/* --- static variables --- */

/*
 * XXX
 * in the future , ef_map_ucs4_to_[locale]_iso2022cs() should be prepared.
 *
 * XXX
 * other languages(especially ISO8859 variants) are not supported yet.
 */
static map_ucs4_to_func_table_t map_ucs4_to_func_table[] = {
    {"ja", NULL, ef_map_ucs4_to_ja_jp},
    {"ko", NULL, ef_map_ucs4_to_ko_kr},
    {"ru", NULL, ef_map_ucs4_to_ru},
    {"tg", NULL, ef_map_ucs4_to_tg},
    {"uk", NULL, ef_map_ucs4_to_uk},
    {"vi", NULL, ef_map_ucs4_to_viet},
    {"zh", "CN", ef_map_ucs4_to_zh_cn},
    {"zh", "HK", ef_map_ucs4_to_zh_hk},
    {"zh", "TW", ef_map_ucs4_to_zh_tw},
    {"zh", NULL, ef_map_ucs4_to_zh_tw},
};

/* --- static functions --- */

static map_func_t get_map_ucs4_to_func_for_current_locale(void) {
  size_t count;
  char *lang;
  char *country;
  map_ucs4_to_func_table_t *tablep;
  static map_func_t cached_func;
  static int cached;

  /* Once cached, never changed. NULL is also cached. */
  if (cached) {
    return cached_func;
  }

  cached = 1;

  lang = bl_get_lang();
  country = bl_get_country();

  for (count = 0; count < sizeof(map_ucs4_to_func_table) / sizeof(map_ucs4_to_func_table[0]);
       count++) {
    tablep = map_ucs4_to_func_table + count;

    if (!strcmp(tablep->lang, lang) && (!tablep->country || !strcmp(tablep->country, country))) {
      return (cached_func = tablep->func);
    }
  }

  return NULL;
}

/* --- global functions --- */

int ef_map_locale_ucs4_to(ef_char_t *non_ucs4, ef_char_t *ucs4) {
  map_func_t func;

  if ((func = get_map_ucs4_to_func_for_current_locale()) == NULL || !(*func)(non_ucs4, ucs4)) {
    return ef_map_ucs4_to(non_ucs4, ucs4);
  }

  return 1;
}
