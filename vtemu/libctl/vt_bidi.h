/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __CTL_VT_BIDI_H__
#define __CTL_VT_BIDI_H__

#include "../vt_bidi.h"
#include "../vt_char.h"

/* Only used by vt_bidi.c, vt_line_bidi.c */
#define HAS_COMPLEX_SHAPE(state) ((((state)->rtl_state) >> 2) & 0x1)
#define SET_HAS_COMPLEX_SHAPE(state) (((state)->rtl_state) |= (0x1 << 2))
#define UNSET_HAS_COMPLEX_SHAPE(state) (((state)->rtl_state) &= ~(0x1 << 2))
#define BASE_IS_RTL(state) ((((state)->rtl_state) >> 1) & 0x1)
#define SET_BASE_RTL(state) (((state)->rtl_state) |= (0x1 << 1))
#define UNSET_BASE_RTL(state) (((state)->rtl_state) &= ~(0x1 << 1))
#define HAS_RTL(state) (((state)->rtl_state) & 0x1)
#define SET_HAS_RTL(state) (((state)->rtl_state) |= 0x1)
#define UNSET_HAS_RTL(state) (((state)->rtl_state) &= ~0x1)

/*
 * See arabic_present_table, arabic_comb2_present_table and
 * arabic_comb4_present_table in vt_shape_bidi.c.
 */
#define CAN_BE_COMPLEX_SHAPE(code) (0x621 <= (code) && (code) <= 0x6d5)

struct vt_bidi_state {
  u_int16_t *visual_order;
  u_int16_t size;

  int8_t bidi_mode; /* Cache how visual_order is rendered. */

  /*
   * 5bit: Not used for now.
   * 1bit: has_arabic
   * 1bit: base_is_rtl
   * 1bit: has_rtl
   */
  int8_t rtl_state;
};

vt_bidi_state_t vt_bidi_new(void);

int vt_bidi_destroy(vt_bidi_state_t state);

int vt_bidi(vt_bidi_state_t state, vt_char_t *src, u_int size, vt_bidi_mode_t mode,
            const char *separators);

u_int32_t vt_bidi_get_mirror_char(u_int32_t src);

#endif
