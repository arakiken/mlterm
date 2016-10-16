/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_ISO8859_H__
#define __EF_UCS4_ISO8859_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_ucs4_to_iso8859_1_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_2_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_3_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_4_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_5_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_6_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_7_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_8_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_9_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_10_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_tis620_2533(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_13_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_14_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_15_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_iso8859_16_r(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_tcvn5712_3_1993(ef_char_t* non_ucs, u_int32_t ucs4_code);

int ef_map_iso8859_1_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_2_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_3_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_4_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_5_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_6_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_7_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_8_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_9_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_10_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_tis620_2533_to_ucs4(ef_char_t* ucs4, u_int16_t tis620_code);

int ef_map_iso8859_13_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_14_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_15_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_iso8859_16_r_to_ucs4(ef_char_t* ucs4, u_int16_t iso8859_code);

int ef_map_tcvn5712_3_1993_to_ucs4(ef_char_t* ucs4, u_int16_t tcvn_code);

#endif
