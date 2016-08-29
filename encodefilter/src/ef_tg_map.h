/*
 *	$Id$
 */

#ifndef __EF_TG_MAP_H__
#define __EF_TG_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_tg(ef_char_t* tg, ef_char_t* ucs4);

int ef_map_koi8_t_to_iso8859_5_r(ef_char_t* iso8859, ef_char_t* tg);

#endif
