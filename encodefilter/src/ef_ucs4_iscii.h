/*
 *	$Id$
 */

#ifndef __EF_UCS4_ISCII_H__
#define __EF_UCS4_ISCII_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_char.h"

int ef_map_iscii_assamese_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_bengali_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_gujarati_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_hindi_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_kannada_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_malayalam_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_oriya_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_punjabi_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_tamil_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_iscii_telugu_to_ucs4(ef_char_t* ucs4, u_int16_t iscii_code);

int ef_map_ucs4_to_iscii(ef_char_t* non_ucs, u_int32_t ucs4_code);

#endif
