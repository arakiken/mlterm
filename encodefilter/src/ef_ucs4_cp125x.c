/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_cp125x.h"

#include "ef_tblfunc_loader.h"
#include "ef_ucs4_iso8859.h"

/* --- static variables --- */

static struct {
  u_int8_t cp874;
  u_int8_t ucs4; /* 0x20NN */

} cp874_table[] = {
    {0x80, 0xac},
    {0x85, 0x26},
    {0x91, 0x18},
    {0x92, 0x19},
    {0x93, 0x1c},
    {0x94, 0x1d},
    {0x95, 0x22},
    {0x96, 0x13},
    {0x97, 0x14},
};

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_cp125x.c"

#else

ef_map_func(8bits, ef_map_cp1250_to_ucs4, 16) ef_map_func(8bits, ef_map_cp1251_to_ucs4, 16)
    ef_map_func(8bits, ef_map_cp1252_to_ucs4, 16) ef_map_func(8bits, ef_map_cp1253_to_ucs4, 16)
        ef_map_func(8bits, ef_map_cp1254_to_ucs4,
                     16) ef_map_func(8bits, ef_map_cp1255_to_ucs4,
                                      16) ef_map_func(8bits, ef_map_cp1256_to_ucs4, 16)
            ef_map_func(8bits, ef_map_cp1257_to_ucs4, 16) ef_map_func(8bits,
                                                                         ef_map_cp1258_to_ucs4, 16)

                ef_map_func(8bits, ef_map_ucs4_to_cp1250,
                             32) ef_map_func(8bits, ef_map_ucs4_to_cp1251,
                                              32) ef_map_func(8bits, ef_map_ucs4_to_cp1252, 32)
                    ef_map_func(8bits, ef_map_ucs4_to_cp1253,
                                 32) ef_map_func(8bits, ef_map_ucs4_to_cp1254, 32)
                        ef_map_func(8bits, ef_map_ucs4_to_cp1255,
                                     32) ef_map_func(8bits, ef_map_ucs4_to_cp1256, 32)
                            ef_map_func(8bits, ef_map_ucs4_to_cp1257,
                                         32) ef_map_func(8bits, ef_map_ucs4_to_cp1258, 32)

#endif

int ef_map_cp874_to_ucs4(ef_char_t* ucs4, u_int16_t cp874_code) {
  size_t count;

  if (ef_map_tis620_2533_to_ucs4(ucs4, cp874_code & 0x7f)) {
    return 1;
  }

  for (count = 0; count < sizeof(cp874_table) / sizeof(cp874_table[0]); count++) {
    if (cp874_table[count].cp874 == cp874_code) {
      ucs4->ch[0] = 0x0;
      ucs4->ch[1] = 0x0;
      ucs4->ch[2] = 0x20;
      ucs4->ch[3] = cp874_table[count].ucs4;
      ucs4->size = 4;
      ucs4->cs = ISO10646_UCS4_1;
      ucs4->property = 0;

      return 1;
    }
  }

  return 0;
}

int ef_map_ucs4_to_cp874(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  size_t count;

  if (ef_map_ucs4_to_tis620_2533(non_ucs, ucs4_code)) {
    non_ucs->ch[0] |= 0x80;
    non_ucs->cs = CP874;

    return 1;
  }

  for (count = 0; count < sizeof(cp874_table) / sizeof(cp874_table[0]); count++) {
    if (((u_int32_t)cp874_table[count].ucs4) + 0x2000 == ucs4_code) {
      non_ucs->ch[0] = cp874_table[count].cp874;
      non_ucs->size = 1;
      non_ucs->cs = CP874;
      non_ucs->property = 0;

      return 1;
    }
  }

  return 0;
}
