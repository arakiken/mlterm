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

static void set_visual_modified(vt_line_t *line, int logical_mod_beg, int logical_mod_end) {
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
    }
  } else {
    ret = 1; /* order is not changed */
  }

  if (ret == 2) {
    /* order is changed => force to redraw all */
    if (vt_line_get_end_of_modified(line) > vt_line_end_char_index(line)) {
      vt_line_set_modified_all(line);
    } else {
      vt_line_set_modified(line, 0, vt_line_end_char_index(line));
    }
  } else if (HAS_RTL(line->ctl_info.bidi)) {
    set_visual_modified(line, vt_line_get_beg_of_modified(line), vt_line_get_end_of_modified(line));
  }

  return 1;
}

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_visual(vt_line_t *line) {
  int count;
  vt_char_t *src;

  if (line->ctl_info.bidi->size == 0 || !HAS_RTL(line->ctl_info.bidi)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Not need to visualize.\n");
#endif

    return 1;
  }

  if ((src = vt_str_alloca(line->ctl_info.bidi->size)) == NULL) {
    return 0;
  }

  vt_str_copy(src, line->chars, line->ctl_info.bidi->size);

  for (count = 0; count < line->ctl_info.bidi->size; count++) {
    copy_char_with_mirror_check(line->chars + line->ctl_info.bidi->visual_order[count], src + count,
                                line->ctl_info.bidi->visual_order, line->ctl_info.bidi->size,
                                count);
  }

  vt_str_final(src, line->ctl_info.bidi->size);

  return 1;
}

/* The caller should check vt_line_is_using_bidi() in advance. */
int vt_line_bidi_logical(vt_line_t *line) {
  int count;
  vt_char_t *src;

  if (line->ctl_info.bidi->size == 0 || !HAS_RTL(line->ctl_info.bidi)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Not need to logicalize.\n");
#endif

    return 0;
  }

  if ((src = vt_str_alloca(line->ctl_info.bidi->size)) == NULL) {
    return 0;
  }

  vt_str_copy(src, line->chars, line->ctl_info.bidi->size);

  for (count = 0; count < line->ctl_info.bidi->size; count++) {
    copy_char_with_mirror_check(line->chars + count, src + line->ctl_info.bidi->visual_order[count],
                                line->ctl_info.bidi->visual_order, line->ctl_info.bidi->size,
                                count);
  }

  vt_str_final(src, line->ctl_info.bidi->size);

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
    if (flags[norm_pos]) {
      copy_char_with_mirror_check(
          &dst[dst_pos++], line->chars + line->ctl_info.bidi->visual_order[norm_pos],
          line->ctl_info.bidi->visual_order, line->ctl_info.bidi->size, norm_pos);
    }
  }

  return 1;
}
