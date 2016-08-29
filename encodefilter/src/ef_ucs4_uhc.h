/*
 *	$Id$
 */

#ifndef __EF_UCS4_UHC_H__
#define __EF_UCS4_UHC_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_uhc_to_ucs4(ef_char_t* ucs4, u_int16_t ks);

int ef_map_ucs4_to_uhc(ef_char_t* ks, u_int32_t ucs4_code);

#endif
