/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_RU_MAP_H__
#define __EF_RU_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_ru(ef_char_t* ru, ef_char_t* ucs4);

int ef_map_koi8_r_to_iso8859_5_r(ef_char_t* iso8859, ef_char_t* ru);

int ef_map_koi8_r_to_koi8_u(ef_char_t* koi8_u, ef_char_t* koi8_r);

int ef_map_koi8_u_to_koi8_r(ef_char_t* koi8_r, ef_char_t* koi8_u);

#endif
