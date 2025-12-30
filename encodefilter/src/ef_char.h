/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_CHAR_H__
#define __EF_CHAR_H__

#include <pobl/bl_types.h> /* u_xxx */

#include "ef_property.h"
#include "ef_charset.h"

/*
 * use UNMAP_FROM_GR or MAP_TO_GR to operate gr byte.
 * these are for 8bit cs(ISO8859-R...).
 */
#define SET_MSB(ch) ((ch) |= 0x80)
#define UNSET_MSB(ch) ((ch) &= 0x7f)

/*
 * UCS-4 is the max.
 * Do not forget to change the size of ef_char_t::size according to this.
 */
#define MAX_CS_BYTELEN 4

/*
 * this should be kept as small as possible.
 */
typedef struct ef_char {
  u_char ch[MAX_CS_BYTELEN]; /* Big Endian */

  /* Modify the size of vt_parser_t::cs, gl, gr etc if the size of cs is changed. */
  u_int cs : 24;      /* ef_charset_t */
  u_int size : 4;     /* 1 - 4 (MAX_CS_BYTELEN) */
  u_int property : 4; /* ef_property_t */

} ef_char_t;

u_char *ef_int_to_bytes(u_char *bytes, size_t len, u_int32_t int_ch);

u_int32_t ef_bytes_to_int(const u_char *bytes, size_t len);

#define ef_char_to_int(c) ef_bytes_to_int((c)->ch, (c)->size)

#endif
