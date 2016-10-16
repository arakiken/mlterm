/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_SHAPING_H__
#define __VT_SHAPING_H__

#include <pobl/bl_types.h> /* u_int */

#include "vt_char.h"
#include "vt_iscii.h"
#include "vt_line.h" /* ctl_info_t */

#define IS_ARABIC_CHAR(ucode) (((ucode)&0xffffff00) == 0x600)

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)

u_int vt_shape_arabic(vt_char_t* dst, u_int dst_len, vt_char_t* src, u_int src_len);

u_int16_t vt_is_arabic_combining(vt_char_t* prev2, vt_char_t* prev, vt_char_t* ch);

#else

#define vt_shape_arabic (NULL)
#define vt_is_arabic_combining(a, b, c) (0)

#endif

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)

u_int vt_shape_iscii(vt_char_t* dst, u_int dst_len, vt_char_t* src, u_int src_len);

#else

#define vt_shape_iscii (NULL)

#endif

u_int vt_shape_ot_layout(vt_char_t* dst, u_int dst_len, vt_char_t* src, u_int src_len,
                         ctl_info_t ctl_info);

#endif /* __VT_SHAPING_H__ */
