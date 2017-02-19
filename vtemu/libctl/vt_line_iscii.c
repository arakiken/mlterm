/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_line_iscii.h"

#include <stdio.h> /* NULL */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* K_MIN */

#include "vt_iscii.h"

/* --- global functions --- */

int vt_line_set_use_iscii(vt_line_t *line, int flag) {
  if (flag) {
    if (vt_line_is_using_iscii(line)) {
      return 1;
    } else if (line->ctl_info_type != 0) {
      return 0;
    }

    if ((line->ctl_info.iscii = vt_iscii_new()) == NULL) {
      return 0;
    }

    line->ctl_info_type = VINFO_ISCII;
  } else {
    if (vt_line_is_using_iscii(line)) {
      vt_iscii_delete(line->ctl_info.iscii);
      line->ctl_info_type = 0;
    }
  }

  return 1;
}

/* The caller should check vt_line_is_using_iscii() in advance. */
int vt_line_iscii_render(vt_line_t *line /* is always visual */
                         ) {
  int ret;
  int visual_mod_beg;

  /*
   * Lower case: ASCII
   * Upper case: ISCII
   *    (Logical) AAA == (Visual) BBBBB
   * => (Logical) aaa == (Visual) aaa
   * In this case vt_line_is_cleared_to_end() returns 0, so "BB" remains on
   * the screen unless following vt_line_set_modified().
   */
  visual_mod_beg = vt_line_get_beg_of_modified(line);
  if (line->ctl_info.iscii->has_iscii) {
    visual_mod_beg = vt_line_iscii_convert_logical_char_index_to_visual(line, visual_mod_beg);
  }

  if (vt_line_is_real_modified(line)) {
    if ((ret = vt_iscii(line->ctl_info.iscii, line->chars, line->num_of_filled_chars)) <= 0) {
      return ret;
    }

    if (line->ctl_info.iscii->has_iscii) {
      int beg;

      if ((beg = vt_line_iscii_convert_logical_char_index_to_visual(
               line, vt_line_get_beg_of_modified(line))) < visual_mod_beg) {
        visual_mod_beg = beg;
      }
    }

    /*
     * Conforming line->change_{beg|end}_col to visual mode.
     * If this line contains ISCII chars, it should be redrawn to the end of
     * line.
     */
    vt_line_set_modified(line, visual_mod_beg, line->num_of_chars);
  } else {
    vt_line_set_modified(line, visual_mod_beg, vt_line_iscii_convert_logical_char_index_to_visual(
                                                   line, vt_line_get_end_of_modified(line)));
  }

  return 1;
}

/* The caller should check vt_line_is_using_iscii() in advance. */
int vt_line_iscii_visual(vt_line_t *line) {
  vt_char_t *src;
  u_int src_len;
  vt_char_t *dst;
  u_int dst_len;
  int dst_pos;
  int src_pos;

  if (line->ctl_info.iscii->size == 0 || !line->ctl_info.iscii->has_iscii) {
#ifdef __DEBUG
    bl_warn_printf(BL_DEBUG_TAG " Not need to visualize.\n");
#endif

    return 1;
  }

  src_len = line->num_of_filled_chars;
  if ((src = vt_str_alloca(src_len)) == NULL) {
    return 0;
  }

  vt_str_copy(src, line->chars, src_len);

  dst_len = line->ctl_info.iscii->size;
  if (line->num_of_chars < dst_len) {
    vt_char_t *chars;

    if ((chars = vt_str_new(dst_len))) {
      /* XXX => shrunk at vt_screen.c and vt_logical_visual_ctl.c */
      vt_str_delete(line->chars, line->num_of_chars);
      line->chars = chars;
      line->num_of_chars = dst_len;
    } else {
      dst_len = line->num_of_chars;
    }
  }

  dst = line->chars;

  src_pos = 0;
  for (dst_pos = 0; dst_pos < dst_len; dst_pos++) {
    if (line->ctl_info.iscii->num_of_chars_array[dst_pos] == 0) {
      vt_char_copy(dst + dst_pos, vt_get_base_char(src + src_pos - 1));
      /* NULL */
      vt_char_set_code(dst + dst_pos, 0);
    } else {
      u_int count;

      vt_char_copy(dst + dst_pos, src + (src_pos++));

      for (count = 1; count < line->ctl_info.iscii->num_of_chars_array[dst_pos]; count++) {
        vt_char_t *comb;
        u_int num;

#ifdef DEBUG
        if (vt_char_is_comb(vt_get_base_char(src + src_pos))) {
          bl_debug_printf(BL_DEBUG_TAG " illegal iscii\n");
        }
#endif
        vt_char_combine_simple(dst + dst_pos, vt_get_base_char(src + src_pos));

        comb = vt_get_combining_chars(src + (src_pos++), &num);
        for (; num > 0; num--) {
#ifdef DEBUG
          if (!vt_char_is_comb(comb)) {
            bl_debug_printf(BL_DEBUG_TAG " illegal iscii\n");
          }
#endif
          vt_char_combine_simple(dst + dst_pos, comb++);
        }
      }
    }
  }

#ifdef DEBUG
  if (src_pos != src_len) {
    bl_debug_printf(BL_DEBUG_TAG "vt_line_iscii_visual() failed: %d -> %d\n", src_len, src_pos);
  }
#endif

  vt_str_final(src, src_len);

  line->num_of_filled_chars = dst_pos;

  return 1;
}

/* The caller should check vt_line_is_using_iscii() in advance. */
int vt_line_iscii_logical(vt_line_t *line) {
  vt_char_t *src;
  u_int src_len;
  vt_char_t *dst;
  int src_pos;

  if (line->ctl_info.iscii->size == 0 || !line->ctl_info.iscii->has_iscii) {
#ifdef __DEBUG
    bl_warn_printf(BL_DEBUG_TAG " Not need to logicalize.\n");
#endif

    return 1;
  }

  src_len = line->num_of_filled_chars;
  if ((src = vt_str_alloca(src_len)) == NULL) {
    return 0;
  }

  vt_str_copy(src, line->chars, src_len);
  dst = line->chars;

  for (src_pos = 0; src_pos < line->ctl_info.iscii->size; src_pos++) {
    vt_char_t *comb;
    u_int num;

    if (line->ctl_info.iscii->num_of_chars_array[src_pos] == 0) {
      continue;
    } else if (line->ctl_info.iscii->num_of_chars_array[src_pos] == 1) {
      vt_char_copy(dst, src + src_pos);
    } else {
      vt_char_copy(dst, vt_get_base_char(src + src_pos));

      comb = vt_get_combining_chars(src + src_pos, &num);
      for (; num > 0; num--, comb++) {
        if (vt_char_is_comb(comb)) {
          vt_char_combine_simple(dst, comb);
        } else {
          vt_char_copy(++dst, comb);
        }
      }
    }

    dst++;
  }

  vt_str_final(src, src_len);

  line->num_of_filled_chars = dst - line->chars;

  return 1;
}

/* The caller should check vt_line_is_using_iscii() in advance. */
int vt_line_iscii_convert_logical_char_index_to_visual(vt_line_t *line, int logical_char_index) {
  int visual_char_index;

  if (vt_line_is_empty(line)) {
    return 0;
  }

  if (line->ctl_info.iscii->size == 0 || !line->ctl_info.iscii->has_iscii) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " logical char_index is same as visual one.\n");
#endif
    return logical_char_index;
  }

  for (visual_char_index = 0; visual_char_index < line->ctl_info.iscii->size; visual_char_index++) {
    if (logical_char_index == 0 ||
        (logical_char_index -= line->ctl_info.iscii->num_of_chars_array[visual_char_index]) < 0) {
      break;
    }
  }

  return visual_char_index;
}

int vt_line_iscii_need_shape(vt_line_t *line) {
  return line->ctl_info.iscii->size > 0 && line->ctl_info.iscii->has_iscii;
}
