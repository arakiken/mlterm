/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_BIG5_H__
#define __EF_UCS4_BIG5_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_big5_to_ucs4(ef_char_t *ucs4, u_int16_t big5);

int ef_map_hkscs_to_ucs4(ef_char_t *ucs4, u_int16_t big5);

int ef_map_ucs4_to_big5(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_hkscs(ef_char_t *non_ucs, u_int32_t ucs4_code);

#endif
