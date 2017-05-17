/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_LOGICAL_VISUAL_H__
#define __VT_LOGICAL_VISUAL_H__

#include "vt_model.h"
#include "vt_cursor.h"
#include "vt_bidi.h" /* vt_bidi_mode_t */

/*
 * LTR ... e.g. Mongolian
 * RTL ... e.g. CJK
 */
typedef enum vt_vertical_mode {
  VERT_LTR = 0x1,
  VERT_RTL = 0x2,

  VERT_MODE_MAX

} vt_vertical_mode_t;

typedef struct vt_logical_visual {
  /* Private */

  vt_model_t *model;
  vt_cursor_t *cursor;

  int8_t is_visual;

  /* Public */

  /* Whether logical <=> visual is reversible. */
  int8_t is_reversible;

  int (*init)(struct vt_logical_visual *, vt_model_t *, vt_cursor_t *);

  int (*delete)(struct vt_logical_visual *);

  u_int (*logical_cols)(struct vt_logical_visual *);
  u_int (*logical_rows)(struct vt_logical_visual *);

  /*
   * !! Notice !!
   * vt_model_t should not be modified from render/viaul until logical.
   * Any modification is done from logical until render/visual.
   */
  int (*render)(struct vt_logical_visual *);
  int (*visual)(struct vt_logical_visual *);
  int (*logical)(struct vt_logical_visual *);

  int (*visual_line)(struct vt_logical_visual *, vt_line_t *line);

} vt_logical_visual_t;

vt_logical_visual_t *vt_logvis_container_new(void);

int vt_logvis_container_add(vt_logical_visual_t *logvis, vt_logical_visual_t *child);

vt_logical_visual_t *vt_logvis_comb_new(void);

vt_logical_visual_t *vt_logvis_vert_new(vt_vertical_mode_t vertical_mode);

vt_vertical_mode_t vt_get_vertical_mode(char *name);

char *vt_get_vertical_mode_name(vt_vertical_mode_t mode);

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI) || defined(USE_IND) || \
    defined(USE_OT_LAYOUT)
vt_logical_visual_t *vt_logvis_ctl_new(vt_bidi_mode_t mode, const char *separators, void *term);

int vt_logical_visual_cursor_is_rtl(vt_logical_visual_t *logvis);
#else
#define vt_logvis_ctl_new(mode, separators, term) (0)
#define vt_logical_visual_cursor_is_rtl(logvis) (0)
#endif

#endif /* __VT_LOGICAL_VISUAL_H__ */
