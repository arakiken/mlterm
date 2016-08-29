/*
 *	$Id$
 */

#ifndef __VT_CTL_LOADER_H__
#define __VT_CTL_LOADER_H__

#include "vt_line.h"
#include "vt_logical_visual.h"
#include "vt_shape.h"

typedef enum vt_ctl_bidi_id {
  CTL_BIDI_API_COMPAT_CHECK,
  VT_LINE_SET_USE_BIDI,
  VT_LINE_BIDI_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL,
  VT_LINE_BIDI_CONVERT_VISUAL_CHAR_INDEX_TO_LOGICAL,
  VT_LINE_BIDI_COPY_LOGICAL_STR,
  VT_LINE_BIDI_IS_RTL,
  VT_SHAPE_ARABIC,
  VT_IS_ARABIC_COMBINING,
  VT_IS_RTL_CHAR,
  VT_BIDI_COPY,
  VT_BIDI_RESET,
  VT_LINE_BIDI_NEED_SHAPE,
  VT_LINE_BIDI_RENDER,
  VT_LINE_BIDI_VISUAL,
  VT_LINE_BIDI_LOGICAL,
  MAX_CTL_BIDI_FUNCS,

} vt_ctl_bidi_id_t;

typedef enum vt_ctl_iscii_id {
  CTL_ISCII_API_COMPAT_CHECK,
  VT_ISCIIKEY_STATE_NEW,
  VT_ISCIIKEY_STATE_DELETE,
  VT_CONVERT_ASCII_TO_ISCII,
  VT_LINE_SET_USE_ISCII,
  VT_LINE_ISCII_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL,
  VT_SHAPE_ISCII,
  VT_ISCII_COPY,
  VT_ISCII_RESET,
  VT_LINE_ISCII_NEED_SHAPE,
  VT_LINE_ISCII_RENDER,
  VT_LINE_ISCII_VISUAL,
  VT_LINE_ISCII_LOGICAL,
  MAX_CTL_ISCII_FUNCS,

} vt_ctl_iscii_id_t;

#define CTL_API_VERSION 0x02
#define CTL_API_COMPAT_CHECK_MAGIC \
  (((CTL_API_VERSION & 0x0f) << 28) | ((sizeof(vt_line_t) & 0xff) << 20))

void* vt_load_ctl_bidi_func(vt_ctl_bidi_id_t id);

void* vt_load_ctl_iscii_func(vt_ctl_iscii_id_t id);

#endif
