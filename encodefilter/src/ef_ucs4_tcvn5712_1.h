/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_TCVN5712_1_H__
#define __EF_UCS4_TCVN5712_1_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_tcvn5712_1_1993_to_ucs4(ef_char_t *ucs4, u_int16_t tcvn_code);

int ef_map_ucs4_to_tcvn5712_1_1993(ef_char_t *non_ucs, u_int32_t ucs4_code);

#endif
