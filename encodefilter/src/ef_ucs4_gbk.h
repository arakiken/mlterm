/*
 *	$Id$
 */

#ifndef __EF_UCS4_GBK_H__
#define __EF_UCS4_GBK_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_gbk_to_ucs4(ef_char_t* ucs4, u_int16_t gb);

int ef_map_ucs4_to_gbk(ef_char_t* non_ucs, u_int32_t ucs4_code);

#endif
