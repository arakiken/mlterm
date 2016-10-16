/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_ZH_TW_MAP_H__
#define __EF_ZH_TW_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_zh_tw(ef_char_t* zhtw, ef_char_t* ucs4);

int ef_map_big5_to_cns11643_1992(ef_char_t* cns, ef_char_t* big5);

int ef_map_cns11643_1992_1_to_big5(ef_char_t* big5, ef_char_t* cns);

int ef_map_cns11643_1992_2_to_big5(ef_char_t* big5, ef_char_t* cns);

#endif
