/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __CTL_VT_ISCII_H__
#define __CTL_VT_ISCII_H__

#include "../vt_iscii.h"

#include <mef/ef_charset.h>
#include "../vt_char.h"

struct vt_iscii_state {
  u_int8_t *num_chars_array;
  u_int16_t size;

  int8_t has_iscii;
};

u_int vt_iscii_shape(ef_charset_t cs, u_char *dst, size_t dst_size, u_char *src);

vt_iscii_state_t vt_iscii_new(void);

int vt_iscii_delete(vt_iscii_state_t state);

int vt_iscii(vt_iscii_state_t state, vt_char_t *src, u_int src_len);

#endif
