/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_ot_layout.h"

#ifdef USE_OT_LAYOUT

#include <pobl/bl_str.h> /* bl_snprintf */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* BL_MAX */

#include "vt_ctl_loader.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static u_int (*shape_func)(void *, u_int32_t *, u_int, int8_t *, int8_t *, u_int8_t *, u_int32_t *,
                           u_int32_t *, u_int, const char *, const char *);
static void *(*get_font_func)(void *, vt_font_t);
static char *ot_layout_attrs[] = {"latn", "liga,clig,dlig,hlig,rlig"};
static int8_t ot_layout_attr_changed[2];

/* --- static functions --- */

#ifndef NO_DYNAMIC_LOAD_CTL

static int vt_is_rtl_char(u_int32_t code) {
  int (*func)(u_int32_t);

  if (!(func = vt_load_ctl_bidi_func(VT_IS_RTL_CHAR))) {
    return 0;
  }

  return (*func)(code);
}

#elif defined(USE_FRIBIDI)

/* Defined in libctl/vt_bidi.c */
int vt_is_rtl_char(u_int32_t code);

#else

#define vt_is_rtl_char(code) (0)

#endif

static int realloc_shape_info(vt_ot_layout_state_t state, u_int *cur_len, u_int new_len) {
  void *p;

  if (*cur_len < new_len) {
    if ((p = realloc(state->glyphs, sizeof(*state->glyphs) * new_len)) == NULL) {
      return 0;
    }
#if 0
    bl_debug_printf("Shape info len: %d->%d\n", *cur_len, new_len);
    bl_debug_printf(" %c (G %p=%p)\n", state->glyphs == p ? 'O' : 'X', state->glyphs, p);
#endif
    state->glyphs = p;

    if ((p = realloc(state->xoffsets, sizeof(*state->xoffsets) * new_len)) == NULL) {
      return 0;
    }
#if 0
    bl_debug_printf(" %c (X %p=%p)\n", state->xoffsets == p ? 'O' : 'X', state->xoffsets, p);
#endif
    state->xoffsets = p;

    if ((p = realloc(state->yoffsets, sizeof(*state->yoffsets) * new_len)) == NULL) {
      return 0;
    }
#if 0
    bl_debug_printf(" %c (Y %p=%p)\n", state->yoffsets == p ? 'O' : 'X', state->yoffsets, p);
#endif
    state->yoffsets = p;

    if ((p = realloc(state->advances, sizeof(*state->advances) * new_len)) == NULL) {
      return 0;
    }
#if 0
    bl_debug_printf(" %c (A %p=%p)\n", state->advances == p ? 'O' : 'X', state->advances, p);
#endif
    state->advances = p;

    *cur_len = new_len;
  }

  return 1;
}

static int store_shape_info(vt_ot_layout_state_t state, u_int32_t *glyphs, int8_t *xoffsets,
                            int8_t *yoffsets, u_int8_t *advances, u_int len, u_int *cur_len) {
  if (!realloc_shape_info(state, cur_len, state->num_glyphs + len + 1)) {
    state->num_glyphs = 0;

    return 0;
  }

  memcpy(state->glyphs + state->num_glyphs, glyphs, sizeof(*glyphs) * len);
  state->glyphs[state->num_glyphs + len] = 0; /* NULL Separator */
  memcpy(state->xoffsets + state->num_glyphs, xoffsets, sizeof(*xoffsets) * len);
  memcpy(state->yoffsets + state->num_glyphs, yoffsets, sizeof(*yoffsets) * len);
  memcpy(state->advances + state->num_glyphs, advances, sizeof(*advances) * len);

  state->num_glyphs += (len + 1);

  return 1;
}

static int ot_layout_cluster(vt_ot_layout_state_t state, u_int8_t *num_chars_array,
                             void *xfont, vt_font_t font, int dst_pos /* initial value is -1 */,
                             u_int32_t *utf32, u_int utf32_len, vt_char_t *src,
                             u_int src_len, u_int *share_info_len) {
  u_int32_t *noshape_glyphs;
  u_int32_t *shape_glyphs;
  u_int num_shape_glyphs;
  u_int num_filled_shape_glyphs;
  int8_t *xoffsets;
  int8_t *yoffsets;
  u_int8_t *advances;
  u_int prev_num_filled_shape_glyphs;
  int utf32_word_len;
  int src_pos;
  u_int32_t prev_shape_glyph;
  int dst_pos_tmp;
  int substituted;
  u_int count;

#ifdef __DEBUG
  int i;
  bl_msg_printf("[utf32 src %d] ", utf32_len);
  for (i = 0; i < utf32_len; i++) {
    bl_msg_printf("%x ", utf32[i]);
  }
  bl_msg_printf("\n");
#endif

  if ((noshape_glyphs = alloca(utf32_len * sizeof(*noshape_glyphs))) == NULL ||
      vt_ot_layout_shape(xfont, NULL, 0, NULL, NULL, NULL, noshape_glyphs, utf32, utf32_len) !=
      utf32_len) {
    return -1;
  }

#ifdef __DEBUG
  bl_msg_printf("[gl(raw)   %d] ", utf32_len);
  for (i = 0; i < utf32_len; i++) {
    bl_msg_printf("%x ", noshape_glyphs[i]);
  }
  bl_msg_printf("\n");
#endif

  num_shape_glyphs = utf32_len * 3; /* XXX */
  if ((shape_glyphs = alloca(num_shape_glyphs * sizeof(*shape_glyphs))) == NULL ||
      (xoffsets = alloca(num_shape_glyphs * sizeof(*xoffsets))) == NULL ||
      (yoffsets = alloca(num_shape_glyphs * sizeof(*yoffsets))) == NULL ||
      (advances = alloca(num_shape_glyphs * sizeof(*advances))) == NULL ||
      /*
       * Apply ot_layout to noshape_glyphs (otf) or
       * utf32 (harfbuzz which ignores noshape_glyphs)
       */
      (num_filled_shape_glyphs =
       vt_ot_layout_shape(xfont, shape_glyphs, num_shape_glyphs, xoffsets, yoffsets,
                          advances, noshape_glyphs, utf32, utf32_len)) == 0) {
    return -1;
  }

#ifdef __DEBUG
  bl_msg_printf("[gl(shape) %d] ", num_filled_shape_glyphs);
  for (i = 0; i < num_filled_shape_glyphs; i++) {
    bl_msg_printf("%x ", shape_glyphs[i]);
  }
  bl_msg_printf("\n");
#endif

  substituted = 0;
  dst_pos_tmp = dst_pos + 1;
  num_chars_array[dst_pos_tmp] = 1;
  count = 0;
  while (1) {
    if (shape_glyphs[count] == 0) {
      /* No glyph -> stop ot_layout */
      return -1;
    } else if (shape_glyphs[count] != noshape_glyphs[count]) {
      substituted = 1;
      break;
    }

    if (++count >= num_filled_shape_glyphs) {
      goto skip_loop;
    } else if (!OTL_IS_COMB(count)) {
      num_chars_array[++dst_pos_tmp] = 1;
    }
  }

  while (++count < num_filled_shape_glyphs) {
    if (shape_glyphs[count] == 0) {
      /* No glyph -> stop ot_layout */
      return -1;
    }
  }

skip_loop:
  if (!store_shape_info(state, shape_glyphs, xoffsets, yoffsets, advances,
                        num_filled_shape_glyphs, share_info_len)) {
    return -1;
  }

  if (!substituted &&
      /*
       * 'visual length == logical length' mostly means 'visual combining == logical combining'.
       * In this case following processing (calculating how many logical characters correspond
       * to 1 visual character) can be skipped.
       */
      dst_pos_tmp - dst_pos == src_len) {
    return (dst_pos = dst_pos_tmp);
  }

  state->substituted = 1;

  prev_num_filled_shape_glyphs = 0;
  utf32_word_len = 0;
  prev_shape_glyph = 0;
  for (src_pos = 0; src_pos < src_len; src_pos++) {
    if (vt_char_code(src + src_pos) == ' ') {
      utf32 += (utf32_word_len + 1);
      noshape_glyphs += (utf32_word_len + 1);
      utf32_word_len = 0;
      prev_num_filled_shape_glyphs = 0;
      num_chars_array[++dst_pos] = 1;

      continue;
    }

    vt_get_combining_chars(src + src_pos, &count);
    utf32_word_len += (count + 1);

    /* apply ot_layout to glyph indeces in utf32. */
    num_filled_shape_glyphs = vt_ot_layout_shape(xfont, shape_glyphs, num_shape_glyphs, xoffsets,
                                              yoffsets,  advances, noshape_glyphs,
                                              utf32, utf32_word_len);

#ifdef __DEBUG
    bl_msg_printf("[gl(word)  %d] ", num_filled_shape_glyphs);
    for (i = 0; i < num_filled_shape_glyphs; i++) {
      bl_msg_printf("%x ", shape_glyphs[i]);
    }
    bl_msg_printf("\n");
#endif

    for (count = num_filled_shape_glyphs; count > 1; count--) {
      if (OTL_IS_COMB(count - 1)) {
        num_filled_shape_glyphs--;
      }
    }
    if (xoffsets[0] < 0) {
      num_filled_shape_glyphs--;
    }

    if (num_filled_shape_glyphs < prev_num_filled_shape_glyphs) {
      if (num_filled_shape_glyphs == 0) {
        return -1;
      }

      count = prev_num_filled_shape_glyphs - num_filled_shape_glyphs;
      dst_pos -= count;
      for (; count > 0; count--) {
        num_chars_array[dst_pos] += num_chars_array[dst_pos + count];
      }

      prev_num_filled_shape_glyphs = num_filled_shape_glyphs; /* goto to the next if block */

      state->complex_shape = 1;
    }

    if (dst_pos >= 0 && num_filled_shape_glyphs == prev_num_filled_shape_glyphs) {
      num_chars_array[dst_pos]++;
      state->complex_shape = 1;
    } else {
      num_chars_array[++dst_pos] = 1;

      if ((count = num_filled_shape_glyphs - prev_num_filled_shape_glyphs) > 1) {
        do {
          num_chars_array[++dst_pos] = 0;
        } while (--count > 1);

        state->complex_shape = 1;
      } else if (!state->complex_shape) {
        if (num_filled_shape_glyphs >= 2 &&
            shape_glyphs[num_filled_shape_glyphs - 2] != prev_shape_glyph) {
          /*
           * This line contains glyphs which are changeable according to the context
           * before and after.
           */
          state->complex_shape = 1;
        }

        prev_shape_glyph = shape_glyphs[num_filled_shape_glyphs - 1];
      }
    }

    prev_num_filled_shape_glyphs = num_filled_shape_glyphs;
  }

  return dst_pos;
}

/* --- global functions --- */

void vt_set_ot_layout_attr(char *value, vt_ot_layout_attr_t attr) {
  if (0 <= attr && attr < MAX_OT_ATTRS) {
    if (ot_layout_attr_changed[attr]) {
      free(ot_layout_attrs[attr]);
    } else {
      ot_layout_attr_changed[attr] = 1;
    }

    if (!value || (attr == OT_SCRIPT && strlen(value) != 4) ||
        !(ot_layout_attrs[attr] = strdup(value))) {
      ot_layout_attrs[attr] = (attr == OT_SCRIPT) ? "latn" : "liga,clig,dlig,hlig,rlig";
    }
  }
}

char *vt_get_ot_layout_attr(vt_ot_layout_attr_t attr) {
  if (0 <= attr && attr < MAX_OT_ATTRS) {
    return ot_layout_attrs[attr];
  } else {
    return "";
  }
}

void vt_ot_layout_set_shape_func(u_int (*func1)(void *, u_int32_t *, u_int, int8_t *, int8_t *,
                                                u_int8_t *, u_int32_t *, u_int32_t *, u_int,
                                                const char *, const char *),
                                 void *(*func2)(void *, vt_font_t)) {
  shape_func = func1;
  get_font_func = func2;
}

u_int vt_ot_layout_shape(void *font, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                         int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances, u_int32_t *cmapped,
                         u_int32_t *src, u_int src_len) {
  if (!shape_func) {
    return 0;
  }

  return (*shape_func)(font, shape_glyphs, num_shape_glyphs, xoffsets, yoffsets, advances, cmapped,
                       src, src_len, ot_layout_attrs[OT_SCRIPT], ot_layout_attrs[OT_FEATURES]);
}

void *vt_ot_layout_get_font(void *term, vt_font_t font) {
  if (!get_font_func) {
    return NULL;
  }

  return (*get_font_func)(term, font);
}

vt_ot_layout_state_t vt_ot_layout_new(void) { return calloc(1, sizeof(struct vt_ot_layout_state)); }

void vt_ot_layout_destroy(vt_ot_layout_state_t state) {
  free(state->num_chars_array);
  free(state->glyphs);
  free(state->xoffsets);
  free(state->yoffsets);
  free(state->advances);
  free(state);
}

int vt_ot_layout(vt_ot_layout_state_t state, vt_char_t *src, u_int src_len) {
  u_int share_info_len = state->num_glyphs;
  int dst_pos;
  int src_pos;
  u_int32_t *ucs_buf;
  u_int num_ucs;
  u_int8_t *num_chars_array;
  vt_font_t font;
  vt_font_t prev_font;
  void *xfont;
  u_int32_t code;
  int cluster_beg;

  if ((ucs_buf = alloca((src_len * (1 + MAX_COMB_SIZE)) * sizeof(*ucs_buf))) == NULL ||
      (num_chars_array = alloca((src_len * (MAX_COMB_SIZE + 1)) *
                                sizeof(*num_chars_array))) == NULL) {
    goto error;
  }

  if (share_info_len == 0 && !realloc_shape_info(state, &share_info_len, src_len)) {
    goto error;
  }

  state->substituted = 0;
  state->complex_shape = 0;
  state->has_var_width_char = 0;
  state->num_glyphs = 0;
  dst_pos = -1;
  prev_font = font = UNKNOWN_CS;
  xfont = NULL;
  cluster_beg = 0;
  for (src_pos = 0; src_pos < src_len; src_pos++) {
    font = vt_char_font(src + src_pos);
    code = vt_char_code(src + src_pos);

    if (FONT_CS(font) == US_ASCII) {
      font &= ~US_ASCII;
      font |= ISO10646_UCS4_1;
    }

    if (prev_font != font) {
      if (xfont && (dst_pos = ot_layout_cluster(state, num_chars_array, xfont, prev_font, dst_pos,
                                                ucs_buf, num_ucs, src + cluster_beg,
                                                src_pos - cluster_beg, &share_info_len)) == -1) {
        goto error;
      }

      cluster_beg = src_pos;
      num_ucs = 0;
      prev_font = font;

      if (FONT_CS(font) == ISO10646_UCS4_1) {
        xfont = vt_ot_layout_get_font(state->term, font);
      } else {
        xfont = NULL;
      }
    }

    if (xfont) {
      u_int count;
      vt_char_t *comb;
      u_int num;

      ucs_buf[num_ucs] = code;
      if (vt_is_rtl_char(ucs_buf[num_ucs])) {
        return -1; /* bidi */
      } else if (IS_VAR_WIDTH_CHAR(ucs_buf[num_ucs])) {
        state->complex_shape = state->substituted = state->has_var_width_char = 1;
      }

      /* Don't do it in vt_is_rtl_char() which may be replaced by (0). */
      num_ucs++;

      comb = vt_get_combining_chars(src + src_pos, &num);
      for (count = 0; count < num; count++) {
        ucs_buf[num_ucs] = vt_char_code(comb++);
        if (vt_is_rtl_char(ucs_buf[num_ucs])) {
          return -1; /* bidi */
        }
        /* Don't do it in vt_is_rtl_char() which may be replaced by (0). */
        num_ucs++;
      }
    } else if (IS_ISCII(FONT_CS(font))) {
      return -2; /* iscii */
    } else {
      num_chars_array[++dst_pos] = 1;
    }
  }

  if (xfont && (dst_pos = ot_layout_cluster(state, num_chars_array, xfont, prev_font, dst_pos,
                                            ucs_buf, num_ucs, src + cluster_beg,
                                            src_pos - cluster_beg, &share_info_len)) == -1) {
    goto error;
  }

#ifdef __DEBUG
  {
    int count;
    bl_msg_printf("ORDER: ");
    for (count = 0; count <= dst_pos; count++) {
      bl_msg_printf("%d ", num_chars_array[count]);
    }
    bl_msg_printf("\n");
  }
#endif

  if (state->size != dst_pos + 1) {
    void *p;

    if (!(p = realloc(state->num_chars_array,
                      BL_MAX(dst_pos + 1, src_len) * sizeof(*num_chars_array)))) {
      goto error;
    }

#ifdef __DEBUG
    if (p != state->num_chars_array) {
      bl_debug_printf(BL_DEBUG_TAG " REALLOC array %d(%p) -> %d(%p)\n", state->size,
                      state->num_chars_array, dst_pos + 1, p);
    }
#endif

    state->num_chars_array = p;
    state->size = dst_pos + 1;
  }

  memcpy(state->num_chars_array, num_chars_array, state->size * sizeof(*num_chars_array));

#ifdef __DEBUG
  bl_debug_printf("OpenType state: substituted %d complex_shape %d has_var_width_char %d\n",
                  state->substituted, state->complex_shape, state->has_var_width_char);
#endif

  return 1;

error:
  state->substituted = 0;
  state->size = 0;
  state->complex_shape = 0;
  state->has_var_width_char = 0;
  state->num_glyphs = 0;

  return 0;
}

int vt_ot_layout_copy(vt_ot_layout_state_t dst, vt_ot_layout_state_t src, int optimize) {
  void *p;

  if (optimize && !src->substituted) {
    /* for vt_line_clone() and vt_log_add() */
    vt_ot_layout_destroy(dst); /* dst = NULL ? XXX */

    return -1;
  }

  if (src->size == 0) {
    free(dst->num_chars_array);
    dst->num_chars_array = NULL;
  } else if ((p = realloc(dst->num_chars_array, sizeof(u_int8_t) * src->size))) {
    dst->num_chars_array = memcpy(p, src->num_chars_array,
                                  sizeof(*dst->num_chars_array) * src->size);
  } else {
    goto error;
  }

  dst->size = src->size;

  if (src->num_glyphs == 0) {
    free(dst->glyphs);
    free(dst->xoffsets);
    free(dst->yoffsets);
    free(dst->advances);
    dst->glyphs = NULL;
    dst->xoffsets = NULL;
    dst->yoffsets = NULL;
    dst->advances = NULL;

    src->substituted = 0;
  } else {
    if ((p = realloc(dst->glyphs, sizeof(*dst->glyphs) * src->num_glyphs)) == NULL) {
      goto error;
    }
    dst->glyphs = p;

    if ((p = realloc(dst->xoffsets, sizeof(*dst->xoffsets) * src->num_glyphs)) == NULL) {
      goto error;
    }
    dst->xoffsets = p;

    if ((p = realloc(dst->yoffsets, sizeof(*dst->yoffsets) * src->num_glyphs)) == NULL) {
      goto error;
    }
    dst->yoffsets = p;

    if ((p = realloc(dst->advances, sizeof(*dst->advances) * src->num_glyphs)) == NULL) {
      goto error;
    }
    dst->advances = p;

    memcpy(dst->glyphs, src->glyphs, sizeof(*dst->glyphs) * src->num_glyphs);
    memcpy(dst->xoffsets, src->xoffsets, sizeof(*dst->xoffsets) * src->num_glyphs);
    memcpy(dst->yoffsets, src->yoffsets, sizeof(*dst->yoffsets) * src->num_glyphs);
    memcpy(dst->advances, src->advances, sizeof(*dst->advances) * src->num_glyphs);

    dst->substituted = src->substituted;
  }

  dst->num_glyphs = src->num_glyphs;

  dst->term = src->term;

  return 1;

error:
  dst->substituted = 0;
  dst->size = 0;
  dst->complex_shape = 0;
  dst->has_var_width_char = 0;
  dst->num_glyphs = 0;

  return 0;
}

void vt_ot_layout_reset(vt_ot_layout_state_t state) {
  state->size = 0;
  state->num_glyphs = 0;
}

#endif
