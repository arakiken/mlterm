/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_JISX0208_H__
#define __EF_UCS4_JISX0208_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_jisx0208_1983_to_ucs4(ef_char_t* ucs4, u_int16_t jis);

int ef_map_jisx0208_nec_ext_to_ucs4(ef_char_t* ucs4, u_int16_t nec_ext);

int ef_map_jisx0208_necibm_ext_to_ucs4(ef_char_t* ucs4, u_int16_t necibm_ext);

int ef_map_sjis_ibm_ext_to_ucs4(ef_char_t* ucs4, u_int16_t ibm_ext);

int ef_map_ucs4_to_jisx0208_1983(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_jisx0208_nec_ext(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_jisx0208_necibm_ext(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_sjis_ibm_ext(ef_char_t* non_ucs, u_int32_t ucs4_code);

#endif
