/*
 *	$Id$
 */

#ifndef __EF_UCS4_JISX0213_H__
#define __EF_UCS4_JISX0213_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_jisx0213_2000_1_to_ucs4(ef_char_t* ucs4, u_int16_t jis);

int ef_map_jisx0213_2000_2_to_ucs4(ef_char_t* ucs4, u_int16_t jis);

int ef_map_ucs4_to_jisx0213_2000_1(ef_char_t* jis, u_int32_t ucs4_code);

int ef_map_ucs4_to_jisx0213_2000_2(ef_char_t* jis, u_int32_t ucs4_code);

#endif
