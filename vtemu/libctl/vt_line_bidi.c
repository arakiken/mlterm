/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_line_bidi.h"

#include <string.h> /* memset */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* alloca */

#define MSB32 0x80000000

/* --- static functions --- */

static void copy_char_with_mirror_check(vt_char_t *dst, vt_char_t *src, u_int16_t *visual_order,
                                        u_int16_t visual_order_size, int pos) {
  vt_char_copy(dst, src);

  if (((pos > 0 && visual_order[pos - 1] == visual_order[pos] + 1) ||
       (pos + 1 < visual_order_size && visual_order[pos] == visual_order[pos + 1] + 1))) {
/*
 * Pos is RTL character.
 * => XXX It is assumed that pos is always US_ASCII or ISO10646_UCS4_1.
 */
#if 0
    ef_charset_t cs;

    if ((cs = vt_char_cs(dst)) == US_ASCII || cs == ISO10646_UCS4_1)
#endif
    {
      u_int mirror;

      if ((mirror = vt_bidi_get_mirror_char(vt_char_code(dst)))) {
        vt_char_set_code(dst, mirror);
      }
    }
  }
}

static void set_visual_modified(vt_line_t *line /* HAS_RTL is true */,
                                int logical_mod_beg /* char index */,
                                int logical_mod_end /* char index */) {
  int log_pos;
  int visual_mod_beg;
  int visual_mod_end;

  /* same as 0 <= char_index < size */
  if (((u_int)logical_mod_beg) >= line->ctl_info.bidi->size ||
      ((u_int)logical_mod_end) >= line->ctl_info.bidi->size) {
    vt_line_set_modified_all(line);

    return;
  }

  visual_mod_beg = vt_line_end_char_index(line);
  visual_mod_end = 0;
  for (log_pos = logical_mod_beg; log_pos <= logical_mod_end; log_pos++) {
    int vis_pos = line->ctl_info.bidi->visual_order[log_pos];

    if (vis_pos < visual_mod_beg) {
      visual_mod_beg = vis_pos;
    }
    if (vis_pos > visual_mod_end) {
      visual_mod_end = vis_pos;
    }
  }

#if 0
  bl_debug_printf("%p %d %d -> %d %d\n", line, logical_mod_beg, logical_mod_end, visual_mod_beg,
                  visual_mod_end);
#endif

  vt_line_set_updated(line);

  /* XXX Conversion from char_index to col is omitted. */
  vt_line_set_modified(line, visual_mod_beg, visual_mod_end);
}

/* --- global functions --- */

int vt_line_set_use_bidi(vt_line_t *line, int flag) {
  if (flag) {
    if (vt_line_is_using_bidi(line)) {
      return 1;
    } else if (line->ctl_info_type != 0) {
      return 0;
    }

    if ((line->ctl_info.bidi = vt_bidi_new()) == NULL) {
      return 0;
    }

    line->ctl_info_type = VINFO_BIDI;
  } else {
    if (vt_line_is_using_bidi(line)) {
      vt_bidi_destroy(line->ctl_info.bidi);
      line->ctl_info_type = 0;
    }
  }

  return 1;
}

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_render(vt_line_t *line, /* is always modified */
                        vt_bidi_mode_t bidi_mode, const char *separators) {
  int ret;
  int change_end_char_index;

  if (vt_line_is_real_modified(line)) {
    int base_was_rtl;

    base_was_rtl = BASE_IS_RTL(line->ctl_info.bidi);

    if ((ret = vt_bidi(line->ctl_info.bidi, line->chars, line->num_filled_chars, bidi_mode,
                       separators)) <= 0) {
      if (base_was_rtl) {
        /* shifting RTL-base to LTR-base (which requires redrawing line all) */
        vt_line_set_modified_all(line);
      }

      return ret;
    }

    /* Conforming line->change_{beg|end}_col to visual mode. */
    if (base_was_rtl != BASE_IS_RTL(line->ctl_info.bidi)) {
      /*
       * shifting RTL-base to LTR-base or LTR-base to RTL-base.
       * (which requires redrawing line all)
       */
      vt_line_set_modified_all(line);

      return 1;
    } else if (HAS_COMPLEX_SHAPE(line->ctl_info.bidi)) {
      int beg = vt_line_get_beg_of_modified(line);
      int end = vt_line_get_end_of_modified(line);
      u_int code;

      if (beg > 0) {
        code = vt_char_code(line->chars + beg);
        if (!CAN_BE_COMPLEX_SHAPE(code)) {
          /*
           * *: shaped arabic glyph
           * +: unshaped arabic glyph
           * -: noarabic glyph
           * ^: modified
           *
           * ** -> +- -> +-
           *        ^    ^^
           * https://github.com/arakiken/mlterm/issues/52#issuecomment-1565464016
           */
          code = vt_char_code(line->chars + beg - 1);
          if (CAN_BE_COMPLEX_SHAPE(code)) {
            beg--;
          }
        } else {
          /*
           * *+ -> *** -> ***
           *         ^    ^^^
           */
          do {
            code = vt_char_code(line->chars + beg - 1);
            if (!CAN_BE_COMPLEX_SHAPE(code)) {
              break;
            }
          } while (--beg > 0);
        }
      }

      if (end + 1 < line->num_filled_chars) {
        code = vt_char_code(line->chars + end);
        if (!CAN_BE_COMPLEX_SHAPE(code)) {
          /*
           * *** -> *-* -> *-*
           *         ^      ^^
           */
          code = vt_char_code(line->chars + end + 1);
          if (CAN_BE_COMPLEX_SHAPE(code)) {
            end++;
          }
        } else {
          /*
           * ** -> *** -> ***
           *       ^      ^^^
           */
          do {
            code = vt_char_code(line->chars + end + 1);
            if (!CAN_BE_COMPLEX_SHAPE(code)) {
              break;
            }
          } while (++end + 1 < line->num_filled_chars);
        }
      }

      vt_line_set_modified(line, beg, end);
    }
  } else {
    ret = 1; /* order is not changed */
  }

  change_end_char_index = vt_convert_col_to_char_index(line, NULL,
                                                       line->change_end_col, BREAK_BOUNDARY);
  if (ret == 2) {
    /* order is changed => force to redraw all */
    if (change_end_char_index > vt_line_end_char_index(line)) {
      vt_line_set_modified_all(line);
    } else {
      vt_line_set_modified(line, 0, vt_line_end_char_index(line));
    }
  } else if (HAS_RTL(line->ctl_info.bidi)) {
    set_visual_modified(line, vt_line_get_beg_of_modified(line), change_end_char_index);
  }

  return 1;
}

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_visual(vt_line_t *line) {
  int count;
  vt_char_t *src;
  int prev = -1;

  if (line->ctl_info.bidi->size == 0 || !HAS_RTL(line->ctl_info.bidi)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Not need to visualize.\n");
#endif

    return 1;
  }

  if ((src = vt_str_alloca(line->ctl_info.bidi->size)) == NULL) {
    return 0;
  }
  vt_str_init(src, line->ctl_info.bidi->size);
  vt_str_copy(src, line->chars, line->ctl_info.bidi->size);

#ifdef DEBUG
  int max_vis_pos = 0;
#endif
  for (count = 0; count < line->ctl_info.bidi->size; count++) {
    int vis_pos = line->ctl_info.bidi->visual_order[count];

#ifdef DEBUG
    if (vis_pos > max_vis_pos) {
      max_vis_pos = vis_pos;
    }
#endif

    if (prev == vis_pos) {
      /* Arabic combining (see adjust_comb_pos_in_order() in vt_bidi.c) */
      u_int num;
      u_int code = vt_char_code(src + count);

      if (0x600 <= code && code <= 0x6ff &&
          vt_get_combining_chars(src + count, &num) == NULL &&
          vt_char_combine_simple(line->chars + vis_pos, src + count)) {
        line->num_filled_chars--;
#ifdef __DEBUG
        bl_debug_printf("Combine arabic character %x to %x\n",
                        vt_char_code(src + count),
                        vt_char_code(line->chars + vis_pos));
#endif
      } else {
        /* XXX Not correctly shown */
#ifdef DEBUG
        vt_char_t *ch = vt_get_combining_chars(src + count, &num);

        bl_debug_printf(BL_DEBUG_TAG " Unexpected arabic combining: ");
        if (num > 0) {
          bl_msg_printf("(prev)%x + (cur)%x + (comb)%x\n", vt_char_code(src + count - 1),
                        code, vt_char_code(ch));
        } else {
          bl_msg_printf("\n");
        }
#endif
      }
    } else {
      copy_char_with_mirror_check(line->chars + vis_pos, src + count,
                                  line->ctl_info.bidi->visual_order,
                                  line->ctl_info.bidi->size, count);
      prev = vis_pos;
    }
  }

#ifdef DEBUG
  if (max_vis_pos >= line->num_filled_chars) {
    bl_debug_printf(BL_DEBUG_TAG " *** Arabic comb error ***: "
                    "ctl_info.size %d max vis_pos %d modified num_filled_chas %d\n",
                    line->ctl_info.bidi->size, max_vis_pos,
                    line->num_filled_chars);
  }
#endif

  vt_str_final(src, line->ctl_info.bidi->size);

  return 1;
}

u_int vt_is_arabic_combining(u_int32_t *str, u_int len, int force);

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_logical(vt_line_t *line) {
  int count;
  vt_char_t *src;
  u_int orig_num_filled_chars;
  u_int num;
  vt_char_t *comb;
  int prev = -1;

  if (line->ctl_info.bidi->size == 0 || !HAS_RTL(line->ctl_info.bidi)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Not need to logicalize.\n");
#endif

    return 0;
  }

  /*
   * line->num_filled_chars can be less than libe->ctl_info.bidi->size.
   * (See vt_line_bidi_visual())
   */
  if ((src = vt_str_alloca(line->num_filled_chars)) == NULL) {
    return 0;
  }
  orig_num_filled_chars = line->num_filled_chars;
  vt_str_init(src, orig_num_filled_chars);
  vt_str_copy(src, line->chars, orig_num_filled_chars);

  for (count = 0; count < line->ctl_info.bidi->size; count++) {
    int vis_pos = line->ctl_info.bidi->visual_order[count];

    if (vis_pos != prev) {
      u_int32_t str[2];

      if ((comb = vt_get_combining_chars(src + vis_pos, &num))) {
        str[0] = vt_char_code(src + vis_pos);
        str[1] = vt_char_code(comb);
        /* force == 1 to check if str contains already dynamically combined arabic chars. */
        if (vt_is_arabic_combining(str, 2, 1)) {
#ifdef __DEBUG
          bl_debug_printf("Uncombine arabic character %x from %x\n",
                          vt_char_code(comb), vt_char_code(src + vis_pos));
#endif

          vt_char_copy(line->chars + count, vt_get_base_char(src + vis_pos));

          if (line->num_filled_chars + num > line->num_chars) {
            bl_error_printf("Failed to show arabic chars correctly.\n");
            if ((num = line->num_chars - line->num_filled_chars) == 0) {
              goto next_count;
            }
          }

          do {
            vt_char_copy(line->chars + (++count), comb++);
            line->num_filled_chars++;
          } while (--num > 0);

          goto next_count;
        }
      }

      copy_char_with_mirror_check(line->chars + count, src + vis_pos,
                                  line->ctl_info.bidi->visual_order,
                                  line->ctl_info.bidi->size, count);

    next_count:
      prev = vis_pos;
    }
  }

  vt_str_final(src, orig_num_filled_chars);

  /*
   * !! Notice !!
   * is_modified is as it is , which should not be touched here.
   */

  return 1;
}

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_convert_logical_char_index_to_visual(vt_line_t *line, int char_index,
                                                      u_int32_t *meet_pos_info) {
  if (((u_int)char_index) < line->ctl_info.bidi->size && /* same as 0 <= char_index < size */
      HAS_RTL(line->ctl_info.bidi)) {
    if (meet_pos_info) {
      int count;

      *meet_pos_info &= ~MSB32;

      if (!BASE_IS_RTL(line->ctl_info.bidi) && char_index >= 1) {
        for (count = char_index - 2; count >= -1; count--) {
          /*
           * visual order -> 1 2 4 3 5
           *                   ^ ^   ^- char index
           *                   | |
           * cursor position --+ +-- meet position
           *
           * visual order -> 1 2*5*4 3 6
           *                 ^ ^ ^     ^- char index
           *                   | |
           * cursor position --+ +-- meet position
           *
           * visual order -> 1 2 3 6 5 4 7
           *                   ^ ^ ^     ^- char index
           *                     | |
           *   cursor position --+ +-- meet position
           */
#if 0
          bl_debug_printf(" Normal pos %d - Current pos %d %d %d - Meet position info %d\n",
                          line->ctl_info.bidi->visual_order[char_index],
                          count >= 0 ? line->ctl_info.bidi->visual_order[count] : 0,
                          line->ctl_info.bidi->visual_order[count + 1],
                          line->ctl_info.bidi->visual_order[count + 2], *meet_pos_info);
#endif
          if ((count < 0 ||
               line->ctl_info.bidi->visual_order[count] <
               line->ctl_info.bidi->visual_order[count + 1]) &&
              line->ctl_info.bidi->visual_order[count + 1] + 1 <
              line->ctl_info.bidi->visual_order[count + 2]) {
            /*
             * If meet position is not changed, text isn't changed
             * but cursor is moved. In this case cursor position should
             * not be fixed to visual_order[count + 1].
             */
            if (((*meet_pos_info) & ~MSB32) !=
                line->ctl_info.bidi->visual_order[count + 1] +
                line->ctl_info.bidi->visual_order[count + 2]) {
              *meet_pos_info = line->ctl_info.bidi->visual_order[count + 1] +
                line->ctl_info.bidi->visual_order[count + 2];

              if (line->ctl_info.bidi->visual_order[char_index] ==
                  line->ctl_info.bidi->visual_order[count + 2] + 1) {
                *meet_pos_info |= MSB32;
                return line->ctl_info.bidi->visual_order[count + 1];
              }
            }

            break;
          }
        }

        if (count == 0) {
          *meet_pos_info = 0;
        }
      } else if (BASE_IS_RTL(line->ctl_info.bidi) && char_index >= 1) {
        for (count = char_index - 2; count >= -1; count--) {
          /*
           * visual order -> 6 5 4 2 3 1
           *                   ^ ^ ^   ^- char index
           *                     |
           *                     +-- meet position & cursor position
           * visual order -> 7 6 5 2 3*4*1
           *                   ^ ^ ^     ^- char index
           *                     |
           *                     +-- meet position & cursor position
           *
           * visual order -> 7 6 4 5 3 2 1
           *                   ^ ^   ^- char index
           *                   | |
           * cursor position --+ +-- meet position
           * visual order -> 7 6 3 4*5*2 1
           *                   ^ ^     ^- char index
           *                   | |
           * cursor position --+ +-- meet position
           */
#if 0
          bl_debug_printf(" Normal pos %d - Current pos %d %d %d - Meet position info %d\n",
                          line->ctl_info.bidi->visual_order[char_index],
                          count >= 0 ? line->ctl_info.bidi->visual_order[count] : 0,
                          line->ctl_info.bidi->visual_order[count + 1],
                          line->ctl_info.bidi->visual_order[count + 2], *meet_pos_info);
#endif

          if ((count < 0 ||
               line->ctl_info.bidi->visual_order[count] >
               line->ctl_info.bidi->visual_order[count + 1]) &&
              line->ctl_info.bidi->visual_order[count + 1] >
              line->ctl_info.bidi->visual_order[count + 2] + 1) {
            /*
             * If meet position is not changed, text isn't changed
             * but cursor is moved. In this case cursor position should
             * not be fixed to visual_order[count + 1].
             */
            if (((*meet_pos_info) & ~MSB32) !=
                line->ctl_info.bidi->visual_order[count + 1] +
                line->ctl_info.bidi->visual_order[count + 2]) {
              *meet_pos_info = line->ctl_info.bidi->visual_order[count + 1] +
                line->ctl_info.bidi->visual_order[count + 2];

              if (line->ctl_info.bidi->visual_order[char_index] + 1 ==
                  line->ctl_info.bidi->visual_order[count + 2]) {
                *meet_pos_info |= MSB32;
                return line->ctl_info.bidi->visual_order[count + 1];
              }
            }

            break;
          }
        }

        if (count == 0) {
          *meet_pos_info = 0;
        }
      } else {
        *meet_pos_info = 0;
      }
    }

    return line->ctl_info.bidi->visual_order[char_index];
  } else {
    if (meet_pos_info) {
      *meet_pos_info = 0;
    }

    return char_index;
  }
}

/*
 * This function is used only by a loader of this module (not used inside this
 * module),
 * so it is assumed that vt_line_is_using_bidi() was already checked (otherwise
 * this
 * module can be loaded unnecessarily).
 */
int vt_line_bidi_convert_visual_char_index_to_logical(vt_line_t *line, int char_index) {
  u_int count;

  for (count = 0; count < line->ctl_info.bidi->size; count++) {
    if (line->ctl_info.bidi->visual_order[count] == char_index) {
      return count;
    }
  }

  return char_index;
}

/*
 * This function is used only by a loader of this module (not used inside this
 * module),
 * so it is assumed that vt_line_is_using_bidi() was already checked (otherwise
 * this
 * module can be loaded unnecessarily).
 */
int vt_line_bidi_is_rtl(vt_line_t *line) { return BASE_IS_RTL(line->ctl_info.bidi); }

int vt_line_bidi_need_shape(vt_line_t *line) { return HAS_RTL(line->ctl_info.bidi); }

/*
 * This function is used only by a loader of this module (not used inside this
 * module),
 * so it is assumed that vt_line_is_using_bidi() was already checked.
 */
int vt_line_bidi_copy_logical_str(vt_line_t *line, vt_char_t *dst, int beg, /* visual position */
                                  u_int len) {
  /*
   * XXX
   * adhoc implementation.
   */

  int *flags;
  int bidi_pos;
  int norm_pos;
  int dst_pos;
  int prev = -1;

#ifdef DEBUG
  if (((int)len) < 0) {
    abort();
  }
#endif

  if (line->ctl_info.bidi->size == 0) {
    return 0;
  }

  if ((flags = alloca(sizeof(int) * line->ctl_info.bidi->size)) == NULL) {
    return 0;
  }

  memset(flags, 0, sizeof(int) * line->ctl_info.bidi->size);

  for (bidi_pos = beg; bidi_pos < beg + len; bidi_pos++) {
    for (norm_pos = 0; norm_pos < line->ctl_info.bidi->size; norm_pos++) {
      if (line->ctl_info.bidi->visual_order[norm_pos] == bidi_pos) {
        flags[norm_pos] = 1;
      }
    }
  }

  for (dst_pos = norm_pos = 0; norm_pos < line->ctl_info.bidi->size; norm_pos++) {
    int vis_pos = line->ctl_info.bidi->visual_order[norm_pos];

    if (vis_pos != prev) {
      if (flags[norm_pos]) {
        copy_char_with_mirror_check(&dst[dst_pos++], line->chars + vis_pos,
                                    line->ctl_info.bidi->visual_order,
                                    line->ctl_info.bidi->size, norm_pos);
      }

      prev = vis_pos;
    }
  }

  return 1;
}
