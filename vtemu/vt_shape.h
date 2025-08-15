/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_SHAPING_H__
#define __VT_SHAPING_H__

#include <pobl/bl_types.h> /* u_int */

#include "vt_char.h"
#include "vt_iscii.h"
#include "vt_line.h" /* ctl_info_t */

#define IS_ARABIC_CHAR(ucode) (((ucode)&0xffffff00) == 0x600)
#define IS_VAR_WIDTH_CHAR(ucode) (0x900 <= (ucode) && (ucode) <= 0xd7f)

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)

u_int vt_shape_arabic(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len);

void vt_set_use_arabic_dynamic_comb(int use);

int vt_get_use_arabic_dynamic_comb(void);

#else

#define vt_shape_arabic (NULL)
#define vt_set_use_arabic_dynamic_comb(use) (0)
#define vt_get_use_arabic_dynamic_comb() (0)

#endif

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)

u_int vt_shape_iscii(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len);

#else

#define vt_shape_iscii (NULL)

#endif

u_int vt_shape_ot_layout(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len,
                         ctl_info_t ctl_info);

#endif /* __VT_SHAPING_H__ */
