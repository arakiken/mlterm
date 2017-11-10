/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_OT_LAYOUT_H__
#define __VT_OT_LAYOUT_H__

#include <pobl/bl_types.h> /* u_int/u_char */

#include "vt_font.h"
#include "vt_char.h"

typedef enum vt_ot_layout_attr {
  OT_SCRIPT = 0,
  OT_FEATURES = 1,
  MAX_OT_ATTRS = 2,

} vt_ot_layout_attr_t;

typedef struct vt_ot_layout_state {
  void *term;
  u_int8_t *num_chars_array;
  u_int16_t size;

  int8_t substituted;

} * vt_ot_layout_state_t;

void vt_set_ot_layout_attr(char *value, vt_ot_layout_attr_t attr);

char *vt_get_ot_layout_attr(vt_ot_layout_attr_t attr);

void vt_ot_layout_set_shape_func(u_int (*func1)(void *, u_int32_t *, u_int, int8_t *, u_int8_t *,
                                                u_int32_t *, u_int32_t *, u_int, const char *,
                                                const char *),
                                 void *(*func2)(void *, vt_font_t));

u_int vt_ot_layout_shape(void *font, u_int32_t *shaped, u_int32_t shaped_len, int8_t *offsets,
                         u_int8_t *widths, u_int32_t *cmapped, u_int32_t *src, u_int32_t src_len);

void *vt_ot_layout_get_font(void *term, vt_font_t font);

vt_ot_layout_state_t vt_ot_layout_new(void);

int vt_ot_layout_delete(vt_ot_layout_state_t state);

int vt_ot_layout(vt_ot_layout_state_t state, vt_char_t *src, u_int src_len);

void vt_ot_layout_reset(vt_ot_layout_state_t state);

int vt_ot_layout_copy(vt_ot_layout_state_t dst, vt_ot_layout_state_t src, int optimize);

#endif
