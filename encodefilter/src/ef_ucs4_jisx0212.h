/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_JISX0212_H__
#define __EF_UCS4_JISX0212_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_jisx0212_1990_to_ucs4(ef_char_t *ucs4, u_int16_t jis);

int ef_map_ucs4_to_jisx0212_1990(ef_char_t *jis, u_int32_t ucs4_code);

#endif
