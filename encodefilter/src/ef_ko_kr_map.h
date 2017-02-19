/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_KO_KR_MAP_H__
#define __EF_KO_KR_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_ko_kr(ef_char_t *kokr, ef_char_t *ucs4);

int ef_map_uhc_to_ksc5601_1987(ef_char_t *ksc, ef_char_t *uhc);

int ef_map_ksc5601_1987_to_uhc(ef_char_t *uhc, ef_char_t *ksc);

int ef_map_johab_to_uhc(ef_char_t *uhc, ef_char_t *johab);

int ef_map_uhc_to_johab(ef_char_t *johab, ef_char_t *uhc);

#endif
