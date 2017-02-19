/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_BIDI_H__
#define __VT_BIDI_H__

#include <pobl/bl_types.h> /* u_int */

typedef enum {
  BIDI_NORMAL_MODE = 0,
  BIDI_ALWAYS_LEFT = 1,
  BIDI_ALWAYS_RIGHT = 2,
  BIDI_MODE_MAX,

} vt_bidi_mode_t;

typedef struct vt_bidi_state *vt_bidi_state_t;

vt_bidi_mode_t vt_get_bidi_mode(const char *name);

char *vt_get_bidi_mode_name(vt_bidi_mode_t mode);

#endif
