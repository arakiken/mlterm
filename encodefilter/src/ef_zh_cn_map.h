/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_ZH_CN_MAP_H__
#define __EF_ZH_CN_MAP_H__

#include "ef_char.h"

int ef_map_ucs4_to_zh_cn(ef_char_t *zhcn, ef_char_t *ucs4);

int ef_map_gbk_to_gb2312_80(ef_char_t *gb2312, ef_char_t *gbk);

int ef_map_gb2312_80_to_gbk(ef_char_t *gbk, ef_char_t *gb2312);

#endif
