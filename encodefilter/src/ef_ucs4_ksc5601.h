/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_KSC5601_H__
#define __EF_UCS4_KSC5601_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_ucs4_to_ksc5601_1987(ef_char_t* ks, u_int32_t ucs4_code);

int ef_map_ksc5601_1987_to_ucs4(ef_char_t* ucs4, u_int16_t ks);

#endif
