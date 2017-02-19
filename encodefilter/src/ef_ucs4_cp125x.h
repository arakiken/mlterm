/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UCS4_CP125X_H__
#define __EF_UCS4_CP125X_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_cp1250_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1251_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1252_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1253_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1254_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1255_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1256_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1257_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp1258_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_cp874_to_ucs4(ef_char_t *ucs4, u_int16_t cp_code);

int ef_map_ucs4_to_cp1250(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1251(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1252(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1253(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1254(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1255(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1256(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1257(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp1258(ef_char_t *non_ucs, u_int32_t ucs4_code);

int ef_map_ucs4_to_cp874(ef_char_t *non_ucs, u_int32_t ucs4_code);

#endif
