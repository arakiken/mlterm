/*
 *	$Id$
 */

#ifndef __EF_UCS4_KOI8_H__
#define __EF_UCS4_KOI8_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_koi8_r_to_ucs4(ef_char_t* ucs4, u_int16_t koi8_code);

int ef_map_koi8_u_to_ucs4(ef_char_t* ucs4, u_int16_t koi8_code);

int ef_map_koi8_t_to_ucs4(ef_char_t* ucs4, u_int16_t koi8_code);

int ef_map_ucs4_to_koi8_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_koi8_u(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_koi8_t(ef_char_t* non_ucs, u_int32_t ucs4_code);

#endif
