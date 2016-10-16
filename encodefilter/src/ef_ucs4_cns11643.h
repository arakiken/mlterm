/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_CNS11643_H__
#define __EF_UCS4_CNS11643_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_cns11643_1992_1_to_ucs4(ef_char_t* ucs4, u_int16_t cns);

int ef_map_cns11643_1992_2_to_ucs4(ef_char_t* ucs4, u_int16_t cns);

int ef_map_cns11643_1992_3_to_ucs4(ef_char_t* ucs4, u_int16_t cns);

int ef_map_ucs4_to_cns11643_1992_1(ef_char_t* cns, u_int32_t ucs4_code);

int ef_map_ucs4_to_cns11643_1992_2(ef_char_t* cns, u_int32_t ucs4_code);

int ef_map_ucs4_to_cns11643_1992_3(ef_char_t* cns, u_int32_t ucs4_code);

#endif
