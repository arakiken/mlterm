/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_zh_cn_map.h"

#include "ef_iso2022_intern.h"
#include "ef_ucs4_map.h"
#include "ef_ucs4_gb2312.h"
#include "ef_ucs4_gbk.h"

/* --- static variables --- */

static ef_map_ucs4_to_func_t map_ucs4_to_funcs[] = {
    ef_map_ucs4_to_gb2312_80, ef_map_ucs4_to_gbk,
};

/* --- global functions --- */

int ef_map_ucs4_to_zh_cn(ef_char_t* zhcn, ef_char_t* ucs4) {
  return ef_map_ucs4_to_with_funcs(zhcn, ucs4, map_ucs4_to_funcs,
                                    sizeof(map_ucs4_to_funcs) / sizeof(map_ucs4_to_funcs[0]));
}

int ef_map_gbk_to_gb2312_80(ef_char_t* gb2312, ef_char_t* gbk) {
  if (0xa1 <= gbk->ch[0] && gbk->ch[0] <= 0xfe && 0xa1 <= gbk->ch[1] && gbk->ch[1] <= 0xfe) {
    gb2312->ch[0] = UNMAP_FROM_GR(gbk->ch[0]);
    gb2312->ch[1] = UNMAP_FROM_GR(gbk->ch[1]);
    gb2312->size = 2;
    gb2312->cs = GB2312_80;

    return 1;
  } else {
    return 0;
  }
}

int ef_map_gb2312_80_to_gbk(ef_char_t* gbk, ef_char_t* gb2312) {
  gbk->ch[0] = MAP_TO_GR(gb2312->ch[0]);
  gbk->ch[1] = MAP_TO_GR(gb2312->ch[1]);
  gbk->size = 2;
  gbk->cs = GBK;

  return 1;
}
