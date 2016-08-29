/*
 *	$Id$
 */

#ifndef __EF_UK_MAP_H__
#define __EF_UK_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_uk(ef_char_t* uk, ef_char_t* ucs4);

int ef_map_koi8_u_to_iso8859_5_r(ef_char_t* iso8859, ef_char_t* uk);

#endif
