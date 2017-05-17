/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_line.h"

#include <string.h> /* memset */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* K_MIN */
#include <pobl/bl_mem.h>  /* alloca */

#include "vt_ctl_loader.h"
#include "vt_ot_layout.h"

#if 0
#define __DEBUG
#endif

#ifdef __DEBUG
#define END_CHAR_INDEX(line)                                                              \
  ((line)->num_of_filled_chars == 0 &&                                                    \
           bl_debug_printf("END_CHAR_INDEX()" BL_DEBUG_TAG " num_of_filled_chars is 0.\n") \
       ? 0                                                                                \
       : (line)->num_of_filled_chars - 1)
#else
#define END_CHAR_INDEX(line) \
  ((line)->num_of_filled_chars == 0 ? 0 : (line)->num_of_filled_chars - 1)
#endif

#define IS_EMPTY(line) ((line)->num_of_filled_chars == 0)

#define vt_line_is_using_bidi(line) ((line)->ctl_info_type == VINFO_BIDI)
#define vt_line_is_using_iscii(line) ((line)->ctl_info_type == VINFO_ISCII)
#define vt_line_is_using_ot_layout(line) ((line)->ctl_info_type == VINFO_OT_LAYOUT)

/* You can specify this macro by configure script option. */
#if 0
#define OPTIMIZE_REDRAWING
#endif

/* --- static functions --- */

#ifndef NO_DYNAMIC_LOAD_CTL

static int vt_line_set_use_bidi(vt_line_t *line, int flag) {
  int (*func)(vt_line_t *, int);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_SET_USE_BIDI))) {
    return 0;
  }

  return (*func)(line, flag);
}

static int vt_line_bidi_convert_visual_char_index_to_logical(vt_line_t *line, int char_index) {
  int (*func)(vt_line_t *, int);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_CONVERT_VISUAL_CHAR_INDEX_TO_LOGICAL))) {
    return char_index;
  }

  return (*func)(line, char_index);
}

static int vt_line_bidi_copy_logical_str(vt_line_t *line, vt_char_t *dst,
                                         int beg, /* visual position */
                                         u_int len) {
  int (*func)(vt_line_t *, vt_char_t *, int, u_int);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_COPY_LOGICAL_STR))) {
    return 0;
  }

  return (*func)(line, dst, beg, len);
}

static int vt_line_bidi_is_rtl(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_IS_RTL))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_bidi_copy(vt_bidi_state_t dst, vt_bidi_state_t src, int optimize_ctl_info) {
  int (*func)(vt_bidi_state_t, vt_bidi_state_t, int);

  if (!(func = vt_load_ctl_bidi_func(VT_BIDI_COPY))) {
    return 0;
  }

  return (*func)(dst, src, optimize_ctl_info);
}

static int vt_bidi_reset(vt_bidi_state_t state) {
  int (*func)(vt_bidi_state_t);

  if (!(func = vt_load_ctl_bidi_func(VT_BIDI_RESET))) {
    return 0;
  }

  return (*func)(state);
}

static int vt_line_bidi_render(vt_line_t *line, vt_bidi_mode_t bidi_mode, const char *separators) {
  int (*func)(vt_line_t *, vt_bidi_mode_t, const char *);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_RENDER))) {
    return 0;
  }

  return (*func)(line, bidi_mode, separators);
}

static int vt_line_bidi_visual(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_VISUAL))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_line_bidi_logical(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_bidi_func(VT_LINE_BIDI_LOGICAL))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_line_set_use_iscii(vt_line_t *line, int flag) {
  int (*func)(vt_line_t *, int);

  if (!(func = vt_load_ctl_iscii_func(VT_LINE_SET_USE_ISCII))) {
    return 0;
  }

  return (*func)(line, flag);
}

static int vt_iscii_copy(vt_iscii_state_t dst, vt_iscii_state_t src, int optimize_ctl_info) {
  int (*func)(vt_iscii_state_t, vt_iscii_state_t, int);

  if (!(func = vt_load_ctl_iscii_func(VT_ISCII_COPY))) {
    return 0;
  }

  return (*func)(dst, src, optimize_ctl_info);
}

static int vt_iscii_reset(vt_iscii_state_t state) {
  int (*func)(vt_iscii_state_t);

  if (!(func = vt_load_ctl_iscii_func(VT_ISCII_RESET))) {
    return 0;
  }

  return (*func)(state);
}

static int vt_line_iscii_render(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_iscii_func(VT_LINE_ISCII_RENDER))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_line_iscii_visual(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_iscii_func(VT_LINE_ISCII_VISUAL))) {
    return 0;
  }

  return (*func)(line);
}

static int vt_line_iscii_logical(vt_line_t *line) {
  int (*func)(vt_line_t *);

  if (!(func = vt_load_ctl_iscii_func(VT_LINE_ISCII_LOGICAL))) {
    return 0;
  }

  return (*func)(line);
}

#else /* NO_DYNAMIC_LOAD_CTL */

#ifndef USE_FRIBIDI
#define vt_line_set_use_bidi(line, flag) (0)
#define vt_line_bidi_convert_visual_char_index_to_logical(line, char_index) (char_index)
#define vt_line_bidi_copy_logical_str(line, dst, beg, len) (0)
#define vt_line_bidi_is_rtl(line) (0)
#define vt_bidi_copy(dst, src, optimize) (0)
#define vt_bidi_reset(state) (0)
#define vt_line_bidi_render(line, bidi_mode, separators) (0)
#define vt_line_bidi_visual(line) (0)
#define vt_line_bidi_logical(line) (0)
#else
/* Link functions in libctl/vt_*bidi.c */
int vt_line_set_use_bidi(vt_line_t *line, int flag);
int vt_line_bidi_convert_visual_char_index_to_logical(vt_line_t *line, int char_index);
int vt_line_bidi_copy_logical_str(vt_line_t *line, vt_char_t *dst, int beg, u_int len);
int vt_line_bidi_is_rtl(vt_line_t *line);
int vt_bidi_copy(vt_bidi_state_t dst, vt_bidi_state_t src, int optimize);
int vt_bidi_reset(vt_bidi_state_t state);
int vt_line_bidi_convert_logical_char_index_to_visual(vt_line_t *line, int char_index,
                                                      u_int32_t *meet_pos_info);
int vt_line_bidi_render(vt_line_t *line, vt_bidi_mode_t bidi_mode, const char *separators);
int vt_line_bidi_visual(vt_line_t *line);
int vt_line_bidi_logical(vt_line_t *line);
#endif /* USE_FRIBIDI */

#ifndef USE_IND
#define vt_line_set_use_iscii(line, flag) (0)
#define vt_iscii_copy(dst, src, optimize) (0)
#define vt_iscii_reset(state) (0)
#define vt_line_iscii_render(line) (0)
#define vt_line_iscii_visual(line) (0)
#define vt_line_iscii_logical(line) (0)
#else
/* Link functions in libctl/vt_*iscii.c */
int vt_line_set_use_iscii(vt_line_t *line, int flag);
int vt_iscii_copy(vt_iscii_state_t dst, vt_iscii_state_t src, int optimize);
int vt_iscii_reset(vt_iscii_state_t state);
int vt_line_iscii_convert_logical_char_index_to_visual(vt_line_t *line, int logical_char_index);
int vt_line_iscii_render(vt_line_t *line);
int vt_line_iscii_visual(vt_line_t *line);
int vt_line_iscii_logical(vt_line_t *line);
#endif /* USE_IND */

#endif /* NO_DYNAMIC_LOAD_CTL */

#ifdef USE_OT_LAYOUT

static int vt_line_set_use_ot_layout(vt_line_t *line, int flag) {
  if (flag) {
    if (vt_line_is_using_ot_layout(line)) {
      return 1;
    } else if (line->ctl_info_type != 0) {
      return 0;
    }

    if ((line->ctl_info.ot_layout = vt_ot_layout_new()) == NULL) {
      return 0;
    }

    line->ctl_info_type = VINFO_OT_LAYOUT;
  } else {
    if (vt_line_is_using_ot_layout(line)) {
      vt_ot_layout_delete(line->ctl_info.ot_layout);
      line->ctl_info_type = 0;
    }
  }

  return 1;
}

/* The caller should check vt_line_is_using_ot_layout() in advance. */
static int vt_line_ot_layout_visual(vt_line_t *line) {
  vt_char_t *src;
  u_int src_len;
  vt_char_t *dst;
  u_int dst_len;
  int dst_pos;
  int src_pos;

  if (line->ctl_info.ot_layout->size == 0 || !line->ctl_info.ot_layout->substituted) {
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

  dst_len = line->ctl_info.ot_layout->size;
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
    if (line->ctl_info.ot_layout->num_of_chars_array[dst_pos] == 0) {
      /*
       * (logical)ACD -> (visual)A0CD -> (shaped)abcd
       *          B              B
       * (B is combining char)
       */
      vt_char_copy(dst + dst_pos, vt_get_base_char(src + src_pos - 1));
      vt_char_set_code(dst + dst_pos, 0);
    } else {
      u_int count;

      vt_char_copy(dst + dst_pos, src + (src_pos++));

      for (count = 1; count < line->ctl_info.ot_layout->num_of_chars_array[dst_pos]; count++) {
        vt_char_t *comb;
        u_int num;

#ifdef DEBUG
        if (vt_char_is_comb(vt_get_base_char(src + src_pos))) {
          bl_debug_printf(BL_DEBUG_TAG " illegal ot_layout\n");
        }
#endif

        vt_char_combine_simple(dst + dst_pos, vt_get_base_char(src + src_pos));

        comb = vt_get_combining_chars(src + (src_pos++), &num);
        for (; num > 0; num--) {
#ifdef DEBUG
          if (!vt_char_is_comb(comb)) {
            bl_debug_printf(BL_DEBUG_TAG " illegal ot_layout\n");
          }
#endif
          vt_char_combine_simple(dst + dst_pos, comb++);
        }
      }
    }
  }

#ifdef DEBUG
  if (src_pos != src_len) {
    bl_debug_printf(BL_DEBUG_TAG "vt_line_ot_layout_visual() failed: %d -> %d\n", src_len, src_pos);
  }
#endif

  vt_str_final(src, src_len);

  line->num_of_filled_chars = dst_pos;

  return 1;
}

/* The caller should check vt_line_is_using_ot_layout() in advance. */
static int vt_line_ot_layout_logical(vt_line_t *line) {
  vt_char_t *src;
  u_int src_len;
  vt_char_t *dst;
  int src_pos;

  if (line->ctl_info.ot_layout->size == 0 || !line->ctl_info.ot_layout->substituted) {
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

  for (src_pos = 0; src_pos < line->ctl_info.ot_layout->size; src_pos++) {
    vt_char_t *comb;
    u_int num;

    if (line->ctl_info.ot_layout->num_of_chars_array[src_pos] == 0) {
      continue;
    }

    if (vt_char_cs(src + src_pos) == ISO10646_UCS4_1_V) {
      vt_char_set_cs(src + src_pos, ISO10646_UCS4_1);
    }

    if (line->ctl_info.ot_layout->num_of_chars_array[src_pos] == 1) {
      vt_char_copy(dst, src + src_pos);
    } else {
      vt_char_copy(dst, vt_get_base_char(src + src_pos));

      comb = vt_get_combining_chars(src + src_pos, &num);
      for (; num > 0; num--, comb++) {
        if (vt_char_cs(comb) == ISO10646_UCS4_1_V) {
          vt_char_set_cs(comb, ISO10646_UCS4_1);
          bl_debug_printf("%x\n", vt_char_cs(comb));
        }

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

/* The caller should check vt_line_is_using_ot_layout() in advance. */
static int vt_line_ot_layout_convert_logical_char_index_to_visual(vt_line_t *line,
                                                                  int logical_char_index) {
  int visual_char_index;

  if (vt_line_is_empty(line)) {
    return 0;
  }

  if (line->ctl_info.ot_layout->size == 0 || !line->ctl_info.ot_layout->substituted) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " logical char_index is same as visual one.\n");
#endif
    return logical_char_index;
  }

  for (visual_char_index = 0; visual_char_index < line->ctl_info.ot_layout->size;
       visual_char_index++) {
    if (logical_char_index == 0 ||
        (logical_char_index -= line->ctl_info.ot_layout->num_of_chars_array[visual_char_index]) <
            0) {
      break;
    }
  }

  return visual_char_index;
}

/* The caller should check vt_line_is_using_ot_layout() in advance. */
static int vt_line_ot_layout_render(vt_line_t *line, /* is always modified */
                                    void *term) {
  int ret;
  int visual_mod_beg;

  /*
   * Lower case: ASCII
   * Upper case: GLYPH INDEX
   *    (Logical) AAA == (Visual) BBBBB
   * => (Logical) aaa == (Visual) aaa
   * In this case vt_line_is_cleared_to_end() returns 0, so "BB" remains on
   * the screen unless following vt_line_set_modified().
   */
  visual_mod_beg = vt_line_get_beg_of_modified(line);
  if (line->ctl_info.ot_layout->substituted) {
    visual_mod_beg = vt_line_ot_layout_convert_logical_char_index_to_visual(line, visual_mod_beg);
  }

  if (vt_line_is_real_modified(line)) {
    line->ctl_info.ot_layout->term = term;

    if ((ret = vt_ot_layout(line->ctl_info.ot_layout, line->chars, line->num_of_filled_chars)) <=
        0) {
      return ret;
    }

    if (line->ctl_info.ot_layout->substituted) {
      int beg;

      if ((beg = vt_line_ot_layout_convert_logical_char_index_to_visual(
               line, vt_line_get_beg_of_modified(line))) < visual_mod_beg) {
        visual_mod_beg = beg;
      }
    }

    /*
     * Conforming line->change_{beg|end}_col to visual mode.
     * If this line contains glyph indeces, it should be redrawn to
     * the end of line.
     */
    vt_line_set_modified(line, visual_mod_beg, line->num_of_chars);
  } else {
    vt_line_set_modified(line, visual_mod_beg,
                         vt_line_ot_layout_convert_logical_char_index_to_visual(
                             line, vt_line_get_end_of_modified(line)));
  }

  return 1;
}

#endif /* USE_OT_LAYOUT */

static void copy_line(vt_line_t *dst, vt_line_t *src, int optimize_ctl_info) {
  u_int copy_len;

  copy_len = K_MIN(src->num_of_filled_chars, dst->num_of_chars);

  vt_str_copy(dst->chars, src->chars, copy_len);
  dst->num_of_filled_chars = copy_len;

  dst->change_beg_col = K_MIN(src->change_beg_col, dst->num_of_chars);
  dst->change_end_col = K_MIN(src->change_end_col, dst->num_of_chars);

  dst->is_modified = src->is_modified;
  dst->is_continued_to_next = src->is_continued_to_next;
  dst->size_attr = src->size_attr;

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
  if (vt_line_is_using_bidi(src)) {
    if (vt_line_is_using_bidi(dst) || vt_line_set_use_bidi(dst, 1)) {
      /*
       * Don't use vt_line_bidi_render() here,
       * or it is impossible to call this function in visual context.
       */
      if (dst->num_of_chars < src->num_of_filled_chars) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Not copy vt_bidi.\n");
#endif
      } else if (vt_bidi_copy(dst->ctl_info.bidi, src->ctl_info.bidi, optimize_ctl_info) == -1) {
        dst->ctl_info_type = 0;
        dst->ctl_info.bidi = NULL;
      }
    }
  } else if (vt_line_is_using_bidi(dst)) {
    vt_line_set_use_bidi(dst, 0);
  }
#endif

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
  if (vt_line_is_using_iscii(src)) {
    if (vt_line_is_using_iscii(dst) || vt_line_set_use_iscii(dst, 1)) {
      /*
       * Don't use vt_line_iscii_render() here,
       * or it is impossible to call this function in visual context.
       */
      if (dst->num_of_chars < src->num_of_filled_chars) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Not copy vt_iscii.\n");
#endif
      } else if (vt_iscii_copy(dst->ctl_info.iscii, src->ctl_info.iscii, optimize_ctl_info) == -1) {
        dst->ctl_info_type = 0;
        dst->ctl_info.iscii = NULL;
      }
    }
  } else if (vt_line_is_using_iscii(dst)) {
    vt_line_set_use_iscii(dst, 0);
  }
#endif

#ifdef USE_OT_LAYOUT
  if (vt_line_is_using_ot_layout(src)) {
    if (vt_line_is_using_ot_layout(dst) || vt_line_set_use_ot_layout(dst, 1)) {
      /*
       * Don't use vt_line_ot_layout_render() here,
       * or it is impossible to call this function in visual context.
       */
      if (dst->num_of_chars < src->num_of_filled_chars) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Not copy vt_ot_layout.\n");
#endif
      } else if (vt_ot_layout_copy(dst->ctl_info.ot_layout, src->ctl_info.ot_layout,
                                   optimize_ctl_info) == -1) {
        dst->ctl_info_type = 0;
        dst->ctl_info.ot_layout = NULL;
      }
    }
  } else if (vt_line_is_using_ot_layout(dst)) {
    vt_line_set_use_ot_layout(dst, 0);
  }
#endif
}

static void set_real_modified(vt_line_t *line, int beg_char_index, int end_char_index) {
  vt_line_set_modified(line, beg_char_index, end_char_index);
  line->is_modified = 2;
}

/* --- global functions --- */

/*
 * Functions which doesn't have to care about visual order.
 */

int vt_line_init(vt_line_t *line, u_int num_of_chars) {
  memset(line, 0, sizeof(vt_line_t));

  if ((line->chars = vt_str_new(num_of_chars)) == NULL) {
    return 0;
  }

  line->num_of_chars = num_of_chars;

  return 1;
}

int vt_line_clone(vt_line_t *clone, vt_line_t *orig, u_int num_of_chars) {
  vt_line_init(clone, num_of_chars);
  copy_line(clone, orig, 1 /* clone->ctl_info can be uncopied. */);

  return 1;
}

int vt_line_final(vt_line_t *line) {
  if (vt_line_is_using_bidi(line)) {
    vt_line_set_use_bidi(line, 0);
  } else if (vt_line_is_using_iscii(line)) {
    vt_line_set_use_iscii(line, 0);
  }
#ifdef USE_OT_LAYOUT
  else if (vt_line_is_using_ot_layout(line)) {
    vt_line_set_use_ot_layout(line, 0);
  }
#endif

  if (line->chars) {
    vt_str_delete(line->chars, line->num_of_chars);
  }

  return 1;
}

/*
 * return: actually broken chars.
 */
u_int vt_line_break_boundary(vt_line_t *line, u_int size) {
  int count;

  if (line->num_of_filled_chars + size > line->num_of_chars) {
/* over line length */

#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " breaking from col %d by size %d failed.",
                   line->num_of_filled_chars, size);
#endif

    size = line->num_of_chars - line->num_of_filled_chars;

#ifdef DEBUG
    bl_msg_printf(" ... size modified -> %d\n", size);
#endif
  }

  if (size == 0) {
    /* nothing is done */

    return 0;
  }

  /* padding spaces */
  for (count = line->num_of_filled_chars; count < line->num_of_filled_chars + size; count++) {
    vt_char_copy(line->chars + count, vt_sp_ch());
  }

#if 0
  set_real_modified(line, END_CHAR_INDEX(line) + 1, END_CHAR_INDEX(line) + size);
#else
  if (vt_line_is_using_ctl(line) && !vt_line_is_real_modified(line)) {
    /* ctl_render_line() should be called in ctl_render() in
     * vt_logical_visual.c. */
    set_real_modified(line, END_CHAR_INDEX(line) + size, END_CHAR_INDEX(line) + size);
  }
#endif

  line->num_of_filled_chars += size;

  return size;
}

int vt_line_assure_boundary(vt_line_t *line, int char_index) {
  if (char_index >= line->num_of_filled_chars) {
    u_int brk_size;

    brk_size = char_index - line->num_of_filled_chars + 1;

    if (vt_line_break_boundary(line, brk_size) < brk_size) {
      return 0;
    }
  }

  return 1;
}

int vt_line_reset(vt_line_t *line) {
  if (IS_EMPTY(line)) {
    /* already reset */

    return 1;
  }

#ifdef OPTIMIZE_REDRAWING
  {
    int count;

    count = END_CHAR_INDEX(line);
    while (1) {
      if (!vt_char_equal(line->chars + count, vt_sp_ch())) {
        set_real_modified(line, 0, count);

        break;
      } else if (--count < 0) {
        break;
      }
    }
  }
#else
  set_real_modified(line, 0, END_CHAR_INDEX(line));
#endif

  line->num_of_filled_chars = 0;

  if (vt_line_is_using_bidi(line)) {
    vt_bidi_reset(line->ctl_info.bidi);
  } else if (vt_line_is_using_iscii(line)) {
    vt_iscii_reset(line->ctl_info.iscii);
  }
#ifdef USE_OT_LAYOUT
  else if (vt_line_is_using_ot_layout(line)) {
    vt_ot_layout_reset(line->ctl_info.ot_layout);
  }
#endif

  line->is_continued_to_next = 0;
  line->size_attr = 0;

  return 1;
}

int vt_line_clear(vt_line_t *line, int char_index) {
  if (char_index >= line->num_of_filled_chars) {
    return 1;
  }

#ifdef OPTIMIZE_REDRAWING
  {
    int count;

    count = END_CHAR_INDEX(line);
    while (1) {
      if (!vt_char_equal(line->chars + count, vt_sp_ch())) {
        set_real_modified(line, char_index, count);

        break;
      } else if (--count < char_index) {
        break;
      }
    }
  }
#else
  set_real_modified(line, char_index, END_CHAR_INDEX(line));
#endif

  vt_char_copy(line->chars + char_index, vt_sp_ch());
  line->num_of_filled_chars = char_index + 1;
  line->is_continued_to_next = 0;
  line->size_attr = 0;

  return 1;
}

int vt_line_clear_with(vt_line_t *line, int char_index, vt_char_t *ch) {
  line->is_continued_to_next = 0;

  return vt_line_fill(
      line, ch, char_index,
      (line->num_of_chars - vt_str_cols(line->chars, char_index)) / vt_char_cols(ch));
}

int vt_line_overwrite(vt_line_t *line, int beg_char_index, /* >= line->num_of_filled_chars is OK */
                      vt_char_t *chars, u_int len, u_int cols) {
  int count;
  u_int cols_to_beg;
  u_int cols_rest;
  u_int padding;
  u_int new_len;
  u_int copy_len;
  vt_char_t *copy_src;

  if (len == 0) {
    return 1;
  }

  if (beg_char_index + len > line->num_of_chars) {
    if (beg_char_index >= line->num_of_chars) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " beg[%d] is over num_of_chars[%d].\n", beg_char_index,
                     line->num_of_chars);
#endif

      return 0;
    }

#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " beg_char_index[%d] + len[%d] is over num_of_chars[%d].\n",
                   beg_char_index, len, line->num_of_chars);
#endif

    len = line->num_of_chars - beg_char_index;
  }

  if (beg_char_index > 0) {
    vt_line_assure_boundary(line, beg_char_index - 1);
  }

#ifdef OPTIMIZE_REDRAWING
  if (len <= line->num_of_filled_chars - beg_char_index) {
    if (vt_str_equal(line->chars + beg_char_index, chars, len)) {
      return 1;
    }
  } else {
    if (vt_str_equal(line->chars + beg_char_index, chars,
                     line->num_of_filled_chars - beg_char_index)) {
      chars += (line->num_of_filled_chars - beg_char_index);
      len -= (line->num_of_filled_chars - beg_char_index);
      beg_char_index = line->num_of_filled_chars;

      count = 0;
      while (1) {
        if (!vt_char_equal(chars + count, vt_sp_ch())) {
          break;
        } else if (++count >= len) {
          vt_str_copy(line->chars + beg_char_index, chars, len);
          line->num_of_filled_chars = beg_char_index + len;

          /* Not necessary vt_line_set_modified() */

          return 1;
        }
      }
    }
  }
#endif

  cols_to_beg = vt_str_cols(line->chars, beg_char_index);

  if (cols_to_beg + cols < line->num_of_chars) {
    int char_index;

    char_index = vt_convert_col_to_char_index(line, &cols_rest, cols_to_beg + cols, 0);

    if (0 < cols_rest && cols_rest < vt_char_cols(line->chars + char_index)) {
      padding = vt_char_cols(line->chars + char_index) - cols_rest;
      char_index++;
    } else {
      padding = 0;
    }

    if (line->num_of_filled_chars > char_index) {
      copy_len = line->num_of_filled_chars - char_index;
    } else {
      copy_len = 0;
    }

    copy_src = line->chars + char_index;
  } else {
    padding = 0;
    copy_len = 0;
    copy_src = NULL;
  }

  new_len = beg_char_index + len + padding + copy_len;

  if (new_len > line->num_of_chars) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " new line len %d(beg %d ow %d padding %d copy %d) is overflowed\n",
                   new_len, beg_char_index, len, padding, copy_len);
#endif

    new_len = line->num_of_chars;

    if (new_len > padding + beg_char_index + len) {
      copy_len = new_len - padding - beg_char_index - len;
    } else {
      copy_len = 0;
      padding = new_len - beg_char_index - len;
    }

#ifdef DEBUG
    bl_msg_printf(" ... modified -> new_len %d , copy_len %d\n", new_len, copy_len);
#endif
  }

  if (copy_len > 0) {
    /* making space */
    vt_str_copy(line->chars + beg_char_index + len + padding, copy_src, copy_len);
  }

  for (count = 0; count < padding; count++) {
    vt_char_copy(line->chars + beg_char_index + len + count, vt_sp_ch());
  }

  vt_str_copy(line->chars + beg_char_index, chars, len);

  line->num_of_filled_chars = new_len;

  set_real_modified(line, beg_char_index, beg_char_index + len + padding - 1);

  return 1;
}

/*
 * Not used for now.
 */
#if 0
int vt_line_overwrite_all(vt_line_t *line, vt_char_t *chars, int len) {
  set_real_modified(line, 0, END_CHAR_INDEX(line));

  vt_str_copy(line->chars, chars, len);
  line->num_of_filled_chars = len;

  set_real_modified(line, 0, END_CHAR_INDEX(line));

  return 1;
}
#endif

int vt_line_fill(vt_line_t *line, vt_char_t *ch, int beg, /* >= line->num_of_filled_chars is OK */
                 u_int num) {
  int count;
  int char_index;
  u_int left_cols;
  u_int copy_len;

  if (num == 0) {
    return 1;
  }

  if (beg >= line->num_of_chars) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " beg[%d] is over num_of_chars[%d].\n", beg, line->num_of_chars);
#endif

    return 0;
  }

  if (beg > 0) {
    vt_line_assure_boundary(line, beg - 1);
  }

#ifdef OPTIMIZE_REDRAWING
  count = 0;
  while (1) {
    if (!vt_char_equal(line->chars + beg + count, ch)) {
      beg += count;
      num -= count;

      if (beg + num <= line->num_of_filled_chars) {
        count = 0;
        while (1) {
          if (!vt_char_equal(line->chars + beg + num - 1 - count, ch)) {
            num -= count;

            break;
          } else if (count++ == num) {
            /* Never happens */

            return 1;
          }
        }
      }

      break;
    } else if (++count >= num) {
      return 1;
    } else if (beg + count == line->num_of_filled_chars) {
      beg += count;
      num -= count;

      break;
    }
  }
#endif

  num = K_MIN(num, line->num_of_chars - beg);

  char_index = beg;
  left_cols = num * vt_char_cols(ch);

  while (1) {
    if (char_index >= line->num_of_filled_chars) {
      left_cols = 0;
      copy_len = 0;

      break;
    } else if (left_cols < vt_char_cols(line->chars + char_index)) {
      if (beg + num + left_cols > line->num_of_chars) {
        left_cols = line->num_of_chars - beg - num;
        copy_len = 0;
      } else {
        copy_len = line->num_of_filled_chars - char_index - left_cols;

        if (beg + num + left_cols + copy_len > line->num_of_chars) {
          /*
           * line->num_of_chars is equal to or larger than
           * beg + num + left_cols since
           * 'if( beg + num + left_cols > line->num_of_chars)'
           * is already passed here.
           */
          copy_len = line->num_of_chars - beg - num - left_cols;
        }
      }

      char_index += (left_cols / vt_char_cols(ch));

      break;
    } else {
      left_cols -= vt_char_cols(line->chars + char_index);
      char_index++;
    }
  }

  if (copy_len > 0) {
    /* making space */
    vt_str_copy(line->chars + beg + num + left_cols, line->chars + char_index, copy_len);
  }

  char_index = beg;

  for (count = 0; count < num; count++) {
    vt_char_copy(&line->chars[char_index++], ch);
  }

  /* padding */
  for (count = 0; count < left_cols; count++) {
    vt_char_copy(&line->chars[char_index++], vt_sp_ch());
  }

  line->num_of_filled_chars = char_index + copy_len;

  set_real_modified(line, beg, beg + num + left_cols);

  return 1;
}

vt_char_t *vt_char_at(vt_line_t *line, int at) {
  if (at >= line->num_of_filled_chars) {
    return NULL;
  } else {
    return line->chars + at;
  }
}

int vt_line_set_modified(vt_line_t *line, int beg_char_index, int end_char_index) {
  int count;
  int beg_col;
  int end_col;

  if (beg_char_index > end_char_index) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " beg_char_index %d > end_char_index %d\n", beg_char_index,
                   end_char_index);
#endif

    return 0;
  }

  if (beg_char_index >= line->num_of_filled_chars) {
    beg_char_index = END_CHAR_INDEX(line);
  }

  beg_col = 0;
  for (count = 0; count < beg_char_index; count++) {
    beg_col += vt_char_cols(line->chars + count);
  }

  if (end_char_index >= line->num_of_filled_chars) {
    /*
     * '* 2' assures change_end_col should point over the end of line.
     * If triple width(or wider) characters(!) were to exist, this hack would
     * make
     * no sense...
     */
    end_col = line->num_of_chars * 2;
  } else {
    end_col = beg_col;
    for (; count <= end_char_index; count++) {
      /*
       * This will be executed at least once, because beg_char_index is never
       * greater than end_char_index.
       */
      end_col += vt_char_cols(line->chars + count);
    }

    /*
     * If vt_char_cols() returns 0, beg_col can be equals to end_col here.
     * If beg_col is equals to end_col, don't minus end_col.
     */
    if (beg_col < end_col) {
      end_col--;
    }
  }

  if (line->is_modified) {
    if (beg_col < line->change_beg_col) {
      line->change_beg_col = beg_col;
    }

    if (end_col > line->change_end_col) {
      line->change_end_col = end_col;
    }
  } else {
    line->change_beg_col = beg_col;
    line->change_end_col = end_col;

    line->is_modified = 1;
  }

  return 1;
}

int vt_line_set_modified_all(vt_line_t *line) {
  line->change_beg_col = 0;

  /*
   * '* 2' assures change_end_col should point over the end of line.
   * If triple width(or wider) characters(!) were to exist, this hack would make
   * no sense...
   */
  line->change_end_col = line->num_of_chars * 2;

  /* Don't overwrite if line->is_modified == 2 (real modified) */
  if (!line->is_modified) {
    line->is_modified = 1;
  }

  return 1;
}

int vt_line_is_cleared_to_end(vt_line_t *line) {
  if (vt_line_get_num_of_filled_cols(line) < line->change_end_col + 1) {
    return 1;
  } else {
    return 0;
  }
}

int vt_line_is_modified(vt_line_t *line) { return line->is_modified; }

int vt_line_get_beg_of_modified(vt_line_t *line) {
  if (IS_EMPTY(line)) {
    return 0;
  } else {
    return vt_convert_col_to_char_index(line, NULL, line->change_beg_col, 0);
  }
}

int vt_line_get_end_of_modified(vt_line_t *line) {
  if (IS_EMPTY(line)) {
    return 0;
  } else {
    return vt_convert_col_to_char_index(line, NULL, line->change_end_col, 0);
  }
}

u_int vt_line_get_num_of_redrawn_chars(vt_line_t *line, int to_end) {
  if (IS_EMPTY(line)) {
    return 0;
  } else if (to_end) {
    return line->num_of_filled_chars - vt_line_get_beg_of_modified(line);
  } else {
    return vt_line_get_end_of_modified(line) - vt_line_get_beg_of_modified(line) + 1;
  }
}

void vt_line_set_updated(vt_line_t *line) {
  line->is_modified = 0;

  line->change_beg_col = 0;
  line->change_end_col = 0;
}

int vt_line_is_continued_to_next(vt_line_t *line) { return line->is_continued_to_next; }

void vt_line_set_continued_to_next(vt_line_t *line, int flag) { line->is_continued_to_next = flag; }

int vt_convert_char_index_to_col(vt_line_t *line, int char_index, int flag /* BREAK_BOUNDARY */
                                 ) {
  int count;
  int col;

  if (char_index >= line->num_of_chars) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " char index %d is larger than num_of_chars(%d) ... modified -> %d.\n",
                    char_index, line->num_of_chars, line->num_of_chars - 1);
#endif

    char_index = line->num_of_chars - 1;
  }

  col = 0;

  if ((flag & BREAK_BOUNDARY) && line->num_of_filled_chars <= char_index) {
    for (count = 0; count < line->num_of_filled_chars; count++) {
#ifdef DEBUG
      if (vt_char_cols(line->chars + count) == 0) {
        bl_warn_printf(BL_DEBUG_TAG " vt_char_cols returns 0.\n");

        continue;
      }
#endif

      col += vt_char_cols(line->chars + count);
    }

    col += (char_index - count);
  } else if (line->num_of_filled_chars > 0) {
    /*
     * excluding the width of the last char.
     */
    for (count = 0; count < K_MIN(char_index, END_CHAR_INDEX(line)); count++) {
      col += vt_char_cols(line->chars + count);
    }
  }

  return col;
}

int vt_convert_col_to_char_index(vt_line_t *line, u_int *cols_rest, int col,
                                 int flag /* BREAK_BOUNDARY */
                                 ) {
  int char_index;

#ifdef DEBUG
  if (col >= line->num_of_chars * 2 && cols_rest) {
    bl_warn_printf(BL_DEBUG_TAG
                   " Since col [%d] is over line->num_of_chars * 2 [%d],"
                   " cols_rest will be corrupt...\n",
                   col, line->num_of_chars * 2);
  }
#endif

  for (char_index = 0; char_index + 1 < line->num_of_filled_chars; char_index++) {
    int cols;

    cols = vt_char_cols(line->chars + char_index);
    if (col < cols) {
      goto end;
    }

    col -= cols;
  }

  if (flag & BREAK_BOUNDARY) {
    char_index += col;
    col = 0;
  }

end:
  if (cols_rest != NULL) {
    *cols_rest = col;
  }

  return char_index;
}

int vt_line_reverse_color(vt_line_t *line, int char_index) {
  if (char_index >= line->num_of_filled_chars) {
    return 0;
  }

  if (vt_char_reverse_color(line->chars + char_index)) {
    vt_line_set_modified(line, char_index, char_index);
  }

  return 1;
}

int vt_line_restore_color(vt_line_t *line, int char_index) {
  if (char_index >= line->num_of_filled_chars) {
    return 0;
  }

  if (vt_char_restore_color(line->chars + char_index)) {
    vt_line_set_modified(line, char_index, char_index);
  }

  return 1;
}

/*
 * This copys a line as it is and doesn't care about visual order.
 * But bidi parameters are also copyed as it is.
 */
int vt_line_copy(vt_line_t *dst, /* should be initialized ahead */
                 vt_line_t *src) {
  copy_line(dst, src, 0);

  return 1;
}

int vt_line_swap(vt_line_t *line1, vt_line_t *line2) {
  vt_line_t tmp;

  tmp = *line1;
  *line1 = *line2;
  *line2 = tmp;

  return 1;
}

int vt_line_share(vt_line_t *dst, vt_line_t *src) {
  memcpy(dst, src, sizeof(vt_line_t));

  return 1;
}

int vt_line_is_empty(vt_line_t *line) { return IS_EMPTY(line); }

int vt_line_beg_char_index_regarding_rtl(vt_line_t *line) {
  int char_index;

  if (vt_line_is_rtl(line)) {
    for (char_index = 0; char_index < line->num_of_filled_chars; char_index++) {
      if (!vt_char_equal(line->chars + char_index, vt_sp_ch())) {
        return char_index;
      }
    }
  }

  return 0;
}

int vt_line_end_char_index(vt_line_t *line) {
  if (IS_EMPTY(line)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " num_of_filled_chars is 0.\n");
#endif

    return 0;
  } else {
    return line->num_of_filled_chars - 1;
  }
}

u_int vt_line_get_num_of_filled_cols(vt_line_t *line) {
  return vt_str_cols(line->chars, line->num_of_filled_chars);
}

u_int vt_line_get_num_of_filled_chars_except_spaces_with_func(vt_line_t *line,
                                                              int (*func)(vt_char_t *,
                                                                          vt_char_t *)) {
  if (IS_EMPTY(line)) {
    return 0;
  } else if (vt_line_is_rtl(line) || line->is_continued_to_next) {
    return line->num_of_filled_chars;
  } else {
    int char_index;

    for (char_index = END_CHAR_INDEX(line); char_index >= 0; char_index--) {
#if 1
      /* >= 3.0.6 */
      if (!(*func)(line->chars + char_index, vt_sp_ch()))
#else
      /* <= 3.0.5 */
      if (!vt_char_equal(line->chars + char_index, vt_sp_ch()))
#endif
      {
        return char_index + 1;
      }
    }

    return 0;
  }
}

void vt_line_set_size_attr(vt_line_t *line, int size_attr) {
  if (line->size_attr != size_attr) {
    line->size_attr = size_attr;
    vt_line_set_modified_all(line);
  }
}

int vt_line_convert_visual_char_index_to_logical(vt_line_t *line, int char_index) {
  if (vt_line_is_using_bidi(line)) {
    return vt_line_bidi_convert_visual_char_index_to_logical(line, char_index);
  } else {
    return char_index;
  }
}

int vt_line_is_rtl(vt_line_t *line) {
  if (vt_line_is_using_bidi(line)) {
    return vt_line_bidi_is_rtl(line);
  } else {
    return 0;
  }
}

/*
 * It is assumed that this function is called in *visual* context.
 */
int vt_line_copy_logical_str(vt_line_t *line, vt_char_t *dst, int beg, /* visual position */
                             u_int len) {
  if (vt_line_is_using_bidi(line)) {
    if (vt_line_bidi_copy_logical_str(line, dst, beg, len)) {
      return 1;
    }
  }

  return vt_str_copy(dst, line->chars + beg, len);
}

int vt_line_convert_logical_char_index_to_visual(vt_line_t *line, int char_index,
                                                 u_int32_t *meet_pos_info) {
#ifdef NO_DYNAMIC_LOAD_CTL
  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      char_index = vt_line_ot_layout_convert_logical_char_index_to_visual(line, char_index);
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
#ifdef USE_FRIBIDI
      char_index = vt_line_bidi_convert_logical_char_index_to_visual(line, char_index,
                                                                     meet_pos_info);
#endif
    } else /* if( vt_line_is_using_iscii( line)) */
    {
#ifdef USE_IND
      char_index = vt_line_iscii_convert_logical_char_index_to_visual(line, char_index);
#endif
    }
  }

  return char_index;
#else
  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      char_index = vt_line_ot_layout_convert_logical_char_index_to_visual(line, char_index);
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      int (*bidi_func)(vt_line_t *, int, int *);

      if ((bidi_func = vt_load_ctl_bidi_func(VT_LINE_BIDI_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL))) {
        char_index = (*bidi_func)(line, char_index, meet_pos_info);
      }
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      int (*iscii_func)(vt_line_t *, int);

      if ((iscii_func =
               vt_load_ctl_iscii_func(VT_LINE_ISCII_CONVERT_LOGICAL_CHAR_INDEX_TO_VISUAL))) {
        char_index = (*iscii_func)(line, char_index);
      }
    }
  }

  return char_index;
#endif
}

int vt_line_unuse_ctl(vt_line_t *line) {
  if (line->ctl_info_type) {
    /* *_render() which may be called later works only if vt_line_t is really modified. */
    set_real_modified(line, 0, END_CHAR_INDEX(line));
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      return vt_line_set_use_ot_layout(line, 0);
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      return vt_line_set_use_bidi(line, 0);
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      return vt_line_set_use_iscii(line, 0);
    }
  }

  return 0;
}

int vt_line_ctl_render(vt_line_t *line, vt_bidi_mode_t bidi_mode, const char *separators,
                       void *term) {
  int ret;
  int (*set_use_ctl)(vt_line_t *, int);

  if (!vt_line_is_using_ctl(line)) {
    if (
#ifdef USE_OT_LAYOUT
        (!term || !vt_line_set_use_ot_layout(line, 1)) &&
#endif
        !vt_line_set_use_bidi(line, 1) && !vt_line_set_use_iscii(line, 1)) {
      return 0;
    }
  }

  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      if (!term) {
        ret = -1;
      } else if ((ret = vt_line_ot_layout_render(line, term)) >= 0) {
        return ret;
      }

      set_use_ctl = vt_line_set_use_ot_layout;

      if (ret == -1) {
        goto render_bidi;
      } else /* if( ret == -2) */
      {
        goto render_iscii;
      }
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      if ((ret = vt_line_bidi_render(line, bidi_mode, separators)) >= 0) {
        return ret;
      }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
      set_use_ctl = vt_line_set_use_bidi;
#else
/* Never enter here */
#endif

      if (ret == -1) {
#ifdef USE_OT_LAYOUT
        if (!term) {
          return 1;
        }

        goto render_ot_layout;
#else
        return 1;
#endif
      } else /* if( ret == -2) */
      {
        goto render_iscii;
      }
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      if ((ret = vt_line_iscii_render(line)) >= 0) {
        return ret;
      }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
      set_use_ctl = vt_line_set_use_iscii;
#else
/* Never enter here */
#endif

#ifdef USE_OT_LAYOUT
      if (term) {
        goto render_ot_layout;
      } else
#endif
      {
        goto render_bidi;
      }
    }
  }

  return 0;

#ifdef USE_OT_LAYOUT
render_ot_layout:
  (*set_use_ctl)(line, 0);
  vt_line_set_use_ot_layout(line, 1);

  if ((ret = vt_line_ot_layout_render(line, term) != -1)) {
    return ret;
  }

/* Fall through */
#endif

render_bidi:
  if (
#if !defined(NO_DYNAMIC_LOAD_CTL)
      vt_load_ctl_bidi_func(VT_LINE_SET_USE_BIDI)
#elif defined(USE_FRIBIDI)
      1
#else
      0
#endif
          ) {
    (*set_use_ctl)(line, 0);
    vt_line_set_use_bidi(line, 1);

    return vt_line_bidi_render(line, bidi_mode, separators);
  } else {
    return 0;
  }

render_iscii:
  if (
#if !defined(NO_DYNAMIC_LOAD_CTL)
      vt_load_ctl_iscii_func(VT_LINE_SET_USE_ISCII)
#elif defined(USE_ISCII)
      1
#else
      0
#endif
          ) {
    (*set_use_ctl)(line, 0);
    vt_line_set_use_iscii(line, 1);

    return vt_line_iscii_render(line);
  } else {
    return 0;
  }
}

int vt_line_ctl_visual(vt_line_t *line) {
  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      return vt_line_ot_layout_visual(line);
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      return vt_line_bidi_visual(line);
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      return vt_line_iscii_visual(line);
    }
  }

  return 0;
}

int vt_line_ctl_logical(vt_line_t *line) {
  if (line->ctl_info_type) {
#ifdef USE_OT_LAYOUT
    if (vt_line_is_using_ot_layout(line)) {
      return vt_line_ot_layout_logical(line);
    } else
#endif
        if (vt_line_is_using_bidi(line)) {
      return vt_line_bidi_logical(line);
    } else /* if( vt_line_is_using_iscii( line)) */
    {
      return vt_line_iscii_logical(line);
    }
  }

  return 0;
}

#ifdef DEBUG

void vt_line_dump(vt_line_t *line) {
  int count;

  for (count = 0; count < line->num_of_filled_chars; count++) {
    vt_char_dump(line->chars + count);
  }

  bl_msg_printf("\n");
}

#endif
