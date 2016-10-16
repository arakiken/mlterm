/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_iso8859.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_iso8859.c"

#else

ef_map_func(8bits, ef_map_ucs4_to_iso8859_1_r, 32) ef_map_func(
    8bits, ef_map_ucs4_to_iso8859_2_r,
    32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_3_r,
                     32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_4_r,
                                      32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_5_r, 32)
    ef_map_func(8bits, ef_map_ucs4_to_iso8859_6_r,
                 32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_7_r,
                                  32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_8_r, 32)
        ef_map_func(8bits, ef_map_ucs4_to_iso8859_9_r,
                     32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_10_r,
                                      32) ef_map_func(8bits, ef_map_ucs4_to_tis620_2533, 32)
            ef_map_func(8bits, ef_map_ucs4_to_iso8859_13_r,
                         32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_14_r,
                                          32) ef_map_func(8bits, ef_map_ucs4_to_iso8859_15_r, 32)
                ef_map_func(8bits, ef_map_ucs4_to_iso8859_16_r,
                             32) ef_map_func(8bits, ef_map_ucs4_to_tcvn5712_3_1993, 32)

                    ef_map_func(8bits, ef_map_iso8859_1_r_to_ucs4,
                                 16) ef_map_func(8bits, ef_map_iso8859_2_r_to_ucs4, 16)
                        ef_map_func(8bits, ef_map_iso8859_3_r_to_ucs4,
                                     16) ef_map_func(8bits, ef_map_iso8859_4_r_to_ucs4, 16)
                            ef_map_func(8bits, ef_map_iso8859_5_r_to_ucs4,
                                         16) ef_map_func(8bits, ef_map_iso8859_6_r_to_ucs4, 16)
                                ef_map_func(8bits, ef_map_iso8859_7_r_to_ucs4, 16) ef_map_func(
                                    8bits, ef_map_iso8859_8_r_to_ucs4,
                                    16) ef_map_func(8bits, ef_map_iso8859_9_r_to_ucs4, 16)
                                    ef_map_func(8bits, ef_map_iso8859_10_r_to_ucs4,
                                                 16) ef_map_func(8bits,
                                                                  ef_map_tis620_2533_to_ucs4, 16)
                                        ef_map_func(8bits, ef_map_iso8859_13_r_to_ucs4,
                                                     16) ef_map_func(8bits,
                                                                      ef_map_iso8859_14_r_to_ucs4,
                                                                      16)
                                            ef_map_func(8bits, ef_map_iso8859_15_r_to_ucs4, 16)
                                                ef_map_func(8bits, ef_map_iso8859_16_r_to_ucs4,
                                                             16)
                                                    ef_map_func(8bits,
                                                                 ef_map_tcvn5712_3_1993_to_ucs4,
                                                                 16)

#endif
