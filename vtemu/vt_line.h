/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_LINE_H__
#define __VT_LINE_H__

#include "vt_str.h"
#include "vt_bidi.h"      /* vt_bidi_state_t */
#include "vt_iscii.h"     /* vt_iscii_state_t */
#include "vt_ot_layout.h" /* vt_ot_layout_state_t */

enum { WRAPAROUND = 0x01, BREAK_BOUNDARY = 0x02, SCROLL = 0x04 };

enum {
  VINFO_BIDI = 0x01,
  VINFO_ISCII = 0x02,
  VINFO_OT_LAYOUT = 0x03,
};

typedef union ctl_info {
  vt_bidi_state_t bidi;
  vt_iscii_state_t iscii;
  vt_ot_layout_state_t ot_layout;

} ctl_info_t;

/*
 * This object size should be kept as small as possible.
 * (160bit ILP32) (224bit ILP64)
 */
typedef struct vt_line {
  /* public(readonly) -- If you access &chars[at], use vt_char_at(). */
  vt_char_t *chars;

  /* public(readonly) */
  u_int16_t num_of_chars;        /* 0 - 65535 */
  u_int16_t num_of_filled_chars; /* 0 - 65535 */

  /* private */
  /*
   * Type of col should be int, but u_int16_t is used here to shrink memory
   * because it is appropriate to assume that change_{beg|end}_col never
   * becomes minus value.
   */
  u_int16_t change_beg_col; /* 0 - 65535 */
  u_int16_t change_end_col; /* 0 - 65535 */

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND) || defined(USE_FRIBIDI) || \
    defined(USE_OT_LAYOUT)
  /* Don't touch from vt_line.c. ctl_info is used by vt_line_bidi.c and
   * vt_line_iscii.c. */
  ctl_info_t ctl_info;
#endif
  u_int8_t ctl_info_type;

  int8_t is_modified; /* 1: need to redraw. 2: was really changed. */
  int8_t is_continued_to_next;

  /* public */
  int8_t size_attr;

} vt_line_t;

int vt_line_init(vt_line_t *line, u_int num_of_chars);

int vt_line_clone(vt_line_t *clone, vt_line_t *orig, u_int num_of_chars);

int vt_line_final(vt_line_t *line);

u_int vt_line_break_boundary(vt_line_t *line, u_int size);

int vt_line_assure_boundary(vt_line_t *line, int char_index);

int vt_line_reset(vt_line_t *line);

int vt_line_clear(vt_line_t *line, int char_index);

int vt_line_clear_with(vt_line_t *line, int char_index, vt_char_t *ch);

int vt_line_overwrite(vt_line_t *line, int beg_char_index, vt_char_t *chars, u_int len, u_int cols);

#if 0
int vt_line_overwrite_all(vt_line_t *line, vt_char_t *chars, int len);
#endif

int vt_line_fill(vt_line_t *line, vt_char_t *ch, int beg, u_int num);

vt_char_t *vt_char_at(vt_line_t *line, int at);

int vt_line_set_modified(vt_line_t *line, int beg_char_index, int end_char_index);

int vt_line_set_modified_all(vt_line_t *line);

int vt_line_is_cleared_to_end(vt_line_t *line);

int vt_line_is_modified(vt_line_t *line);

/* XXX Private api for vt_line.c and vt_line_{iscii|bidi}.c */
#define vt_line_is_real_modified(line) (vt_line_is_modified(line) == 2)

int vt_line_get_beg_of_modified(vt_line_t *line);

int vt_line_get_end_of_modified(vt_line_t *line);

u_int vt_line_get_num_of_redrawn_chars(vt_line_t *line, int to_end);

void vt_line_set_updated(vt_line_t *line);

int vt_line_is_continued_to_next(vt_line_t *line);

void vt_line_set_continued_to_next(vt_line_t *line, int flag);

int vt_convert_char_index_to_col(vt_line_t *line, int char_index, int flag);

int vt_convert_col_to_char_index(vt_line_t *line, u_int *cols_rest, int col, int flag);

int vt_line_reverse_color(vt_line_t *line, int char_index);

int vt_line_restore_color(vt_line_t *line, int char_index);

int vt_line_copy(vt_line_t *dst, vt_line_t *src);

int vt_line_swap(vt_line_t *line1, vt_line_t *line2);

int vt_line_share(vt_line_t *dst, vt_line_t *src);

int vt_line_is_empty(vt_line_t *line);

u_int vt_line_get_num_of_filled_cols(vt_line_t *line);

int vt_line_end_char_index(vt_line_t *line);

int vt_line_beg_char_index_regarding_rtl(vt_line_t *line);

u_int vt_line_get_num_of_filled_chars_except_spaces_with_func(vt_line_t *line,
                                                              int (*func)(vt_char_t *,
                                                                          vt_char_t *));

#define vt_line_get_num_of_filled_chars_except_spaces(line) \
  vt_line_get_num_of_filled_chars_except_spaces_with_func((line), vt_char_code_equal)

void vt_line_set_size_attr(vt_line_t *line, int size_attr);

#define vt_line_is_using_ctl(line) ((line)->ctl_info_type)

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_OT_LAYOUT)
#define vt_line_has_ot_substitute_glyphs(line) \
  ((line)->ctl_info_type == VINFO_OT_LAYOUT && (line)->ctl_info.ot_layout->substituted)
#else
#define vt_line_has_ot_substitute_glyphs(line) (0)
#endif

int vt_line_convert_visual_char_index_to_logical(vt_line_t *line, int char_index);

int vt_line_is_rtl(vt_line_t *line);

int vt_line_copy_logical_str(vt_line_t *line, vt_char_t *dst, int beg, u_int len);

int vt_line_convert_logical_char_index_to_visual(vt_line_t *line, int logical_char_index,
                                                 int *meet_pos);

vt_line_t *vt_line_shape(vt_line_t *line);

int vt_line_unshape(vt_line_t *line, vt_line_t *orig);

int vt_line_unuse_ctl(vt_line_t *line);

int vt_line_ctl_render(vt_line_t *line, vt_bidi_mode_t bidi_mode, const char *separators,
                       void *term);

int vt_line_ctl_visual(vt_line_t *line);

int vt_line_ctl_logical(vt_line_t *line);

#ifdef DEBUG

void vt_line_dump(vt_line_t *line);

#endif

#endif
