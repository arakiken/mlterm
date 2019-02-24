/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_ISCII_H__
#define __VT_ISCII_H__

#include <pobl/bl_types.h> /* u_int/u_char */

typedef struct vt_isciikey_state *vt_isciikey_state_t;

typedef struct vt_iscii_state *vt_iscii_state_t;

vt_isciikey_state_t vt_isciikey_state_new(int is_inscript);

void vt_isciikey_state_destroy(vt_isciikey_state_t state);

size_t vt_convert_ascii_to_iscii(vt_isciikey_state_t state, u_char *iscii, size_t iscii_len,
                                 u_char *ascii, size_t ascii_len);

#endif
