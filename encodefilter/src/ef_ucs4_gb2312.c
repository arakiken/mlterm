/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_gb2312.h"

#include "ef_ucs4_gbk.h"

/* --- global functions --- */

int ef_map_gb2312_80_to_ucs4(ef_char_t *ucs4, u_int16_t gb) {
  /* converting to GBK */
  gb |= 0x8080;

  if (ef_map_gbk_to_ucs4(ucs4, gb)) {
    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_gb2312_80(ef_char_t *gb, u_int32_t ucs4_code) {
  if (ef_map_ucs4_to_gbk(gb, ucs4_code)) {
    if (gb->ch[0] >= 0xa1 && gb->ch[1] >= 0xa1) {
      /* converting to GB2312 */
      gb->ch[0] &= 0x7f;
      gb->ch[1] &= 0x7f;

      gb->cs = GB2312_80;

      return 1;
    }
  }

  return 0;
}
