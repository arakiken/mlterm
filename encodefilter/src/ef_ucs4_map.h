/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_MAP_H__
#define __EF_UCS4_MAP_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

#define UCSBMP_IS_ALPHABET(ucs) (0 <= (ucs) && (ucs) <= 33ff)
#define UCSBMP_IS_CJK(ucs) (0x3400 <= (ucs) && (ucs) <= 0x9fff)
#define UCSBMP_IS_HANGUL(ucs) (0xac00 <= (ucs) && (ucs) <= 0xd7ff)
#define UCSBMP_IS_SURROGATE(ucs) (0xd800 <= (ucs) && (ucs) <= 0xdfff)
#define UCSBMP_IS_COMPAT(ucs) (0xf900 <= (ucs) && (ucs) <= 0xfffd)

typedef int (*ef_map_ucs4_to_func_t)(ef_char_t*, u_int32_t);

int ef_map_ucs4_to_cs(ef_char_t* non_ucs, ef_char_t* ucs4, ef_charset_t cs);

int ef_map_ucs4_to_with_funcs(ef_char_t* non_ucs, ef_char_t* ucs4,
                               ef_map_ucs4_to_func_t* map_ucs4_to_funcs, size_t list_size);

int ef_map_ucs4_to(ef_char_t* non_ucs, ef_char_t* ucs4);

int ef_map_ucs4_to_iso2022cs(ef_char_t* non_ucs, ef_char_t* ucs4);

int ef_map_to_ucs4(ef_char_t* ucs4, ef_char_t* non_ucs);

int ef_map_via_ucs(ef_char_t* dst, ef_char_t* src, ef_charset_t cs);

#endif
