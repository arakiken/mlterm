/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../vt_ctl_loader.h"

/* Dummy declaration */
void vt_line_set_use_iscii(void);
void vt_line_iscii_convert_logical_char_index_to_visual(void);
void vt_iscii_copy(void);
void vt_iscii_reset(void);
void vt_line_iscii_need_shape(void);
void vt_line_iscii_render(void);
void vt_line_iscii_visual(void);
void vt_line_iscii_logical(void);

/* --- global variables --- */

void *vt_ctl_iscii_func_table[MAX_CTL_ISCII_FUNCS] = {
    (void*)CTL_API_COMPAT_CHECK_MAGIC, vt_isciikey_state_new, vt_isciikey_state_delete,
    vt_convert_ascii_to_iscii, vt_line_set_use_iscii,
    vt_line_iscii_convert_logical_char_index_to_visual, vt_shape_iscii, vt_iscii_copy,
    vt_iscii_reset, vt_line_iscii_need_shape, vt_line_iscii_render, vt_line_iscii_visual,
    vt_line_iscii_logical,

};
