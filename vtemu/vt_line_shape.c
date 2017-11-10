/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_line.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_mem.h>
#include "vt_shape.h"
#include "vt_ctl_loader.h"

#define vt_line_is_using_bidi(line) ((line)->ctl_info_type == VINFO_BIDI)
#define vt_line_is_using_iscii(line) ((line)->ctl_info_type == VINFO_ISCII)
#define vt_line_is_using_ot_layout(line) ((line)->ctl_info_type == VINFO_OT_LAYOUT)

/* --- static functions --- */

#ifndef NO_DYNAMIC_LOAD_CTL

static int vt_line_bidi_need_shape(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_NEED_SHAPE))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_line_iscii_need_shape(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_iscii_func(VT_LINE_ISCII_NEED_SHAPE))) {
    return 0;
  }

  return (*func)(line);
}

#else

#ifndef USE_FRIBIDI
#define vt_line_bidi_need_shape(line) (0)
#else
/* Link functions in libctl/vt_*bidi.c */
int vt_line_bidi_need_shape(vt_line_t *line);
#endif /* USE_FRIBIDI */

#ifndef USE_IND
#define vt_line_iscii_need_shape(line) (0)
#else
/* Link functions in libctl/vt_*iscii.c */
int vt_line_iscii_need_shape(vt_line_t *line);
#endif /* USE_IND */

#endif

#ifdef USE_OT_LAYOUT
static int vt_line_ot_layout_need_shape(vt_line_t *line) {
  return line->ctl_info.ot_layout->size > 0 && line->ctl_info.ot_layout->substituted;
}
#endif

/* --- global functions --- */

vt_line_t *vt_line_shape(vt_line_t *line) {
  vt_line_t *orig;
  vt_char_t *shaped;
  u_int (*func)(vt_char_t *, u_int, vt_char_t *, u_int, ctl_info_t);

  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      if (!vt_line_ot_layout_need_shape(line)) {
        return NULL;
      }

      func = vt_shape_ot_layout;
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      if (!vt_line_bidi_need_shape(line)) {
        return NULL;
      }

      func = vt_shape_arabic;
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      if (!vt_line_iscii_need_shape(line)) {
        return NULL;
      }

      func = vt_shape_iscii;
    }

    if ((orig = malloc(sizeof(vt_line_t))) == NULL) {
      return NULL;
    }

    vt_line_share(orig, line);

    if ((shaped = vt_str_new(line->num_chars)) == NULL) {
      free(orig);

      return NULL;
    }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND) || defined(USE_FRIBIDI) || \
    defined(USE_OT_LAYOUT)
    line->num_filled_chars =
        (*func)(shaped, line->num_chars, line->chars, line->num_filled_chars, line->ctl_info);
#else
/* Never enter here */
#endif

    line->chars = shaped;

    return orig;
  }

  return NULL;
}

int vt_line_unshape(vt_line_t *line, vt_line_t *orig) {
  vt_str_delete(line->chars, line->num_chars);

  line->chars = orig->chars;
  line->num_filled_chars = orig->num_filled_chars;

  free(orig);

  return 1;
}
