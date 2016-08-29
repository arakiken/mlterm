/*
 *	$Id$
 */

#ifndef __EF_VIET_MAP_H__
#define __EF_VIET_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_viet(ef_char_t* viet, ef_char_t* ucs4);

int ef_map_viscii_to_tcvn5712_3_1993(ef_char_t* tcvn, ef_char_t* viscii);

#endif
