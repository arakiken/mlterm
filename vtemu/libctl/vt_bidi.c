/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_bidi.h"

#include <string.h> /* memset */
#include <ctype.h>  /* isalpha */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* alloca/realloc/free */
#include <fribidi.h>

#if 0
#define __DEBUG
#endif

#define DIR_LTR_MARK 0x200e
#define DIR_RTL_MARK 0x200f

/* --- global functions --- */

vt_bidi_state_t vt_bidi_new(void) {
  vt_bidi_state_t state;

  if ((state = malloc(sizeof(*state))) == NULL) {
    return NULL;
  }

  state->visual_order = NULL;
  state->size = 0;
  state->rtl_state = 0;
  state->bidi_mode = BIDI_NORMAL_MODE;

  return state;
}

int vt_bidi_destroy(vt_bidi_state_t state) {
  free(state->visual_order);
  free(state);

  return 1;
}

/*
 * Don't call this functions with type_p == FRIBIDI_TYPE_ON and size == cur_pos.
 */
static void log2vis(FriBidiChar *str, u_int size, FriBidiCharType *type_p, vt_bidi_mode_t bidi_mode,
                    FriBidiStrIndex *order, u_int cur_pos, int append) {
  FriBidiCharType type;
  u_int pos;

  if (size > cur_pos) {
    if (bidi_mode == BIDI_NORMAL_MODE) {
      type = FRIBIDI_TYPE_ON;
    } else if (bidi_mode == BIDI_ALWAYS_RIGHT) {
      type = FRIBIDI_TYPE_RTL;
    } else /* if (bidi_mode == BIDI_ALWAYS_LEFT) */ {
      type = FRIBIDI_TYPE_LTR;
    }

    fribidi_log2vis(str + cur_pos, size - cur_pos, &type, NULL, order + cur_pos, NULL, NULL);

    if (*type_p == FRIBIDI_TYPE_ON) {
      *type_p = type;
    }
  } else {
    /*
     * This functions is never called if type_p == FRIBIDI_TYPE_ON and
     * size == cur_pos.
     */
    type = *type_p;
  }

  if (*type_p == FRIBIDI_TYPE_LTR) {
    if (type == FRIBIDI_TYPE_RTL) {
      /*
       * (Logical) "LLL/RRRNNN " ('/' is a separator)
       *                      ^-> endsp
       * => (Visual) "LLL/ NNNRRR" => "LLL/NNNRRR "
       */

      u_int endsp_num;

      for (pos = size; pos > cur_pos; pos--) {
        if (str[pos - 1] != ' ') {
          break;
        }

        order[pos - 1] = pos - 1;
      }

      endsp_num = size - pos;

      for (pos = cur_pos; pos < size - endsp_num; pos++) {
        order[pos] = order[pos] + cur_pos - endsp_num;
      }
    } else if (cur_pos > 0) {
      for (pos = cur_pos; pos < size; pos++) {
        order[pos] += cur_pos;
      }
    }

    if (append) {
      order[size] = size;
    }
  } else /* if (*type_p == FRIBIDI_TYPE_RTL) */ {
    if (cur_pos > 0) {
      for (pos = 0; pos < cur_pos; pos++) {
        order[pos] += (size - cur_pos);
      }
    }

    if (type == FRIBIDI_TYPE_LTR) {
      /*
       * (Logical) "RRRNNN/LLL " ('/' is a separator)
       *                      ^-> endsp
       * => (Visual) "LLL /NNNRRR" => " LLL/NNNRRR"
       */

      u_int endsp_num;

      for (pos = size; pos > cur_pos; pos--) {
        if (str[pos - 1] != ' ') {
          break;
        }

        order[pos - 1] = size - pos;
      }

      endsp_num = size - pos;
      for (pos = cur_pos; pos < size - endsp_num; pos++) {
        order[pos] += endsp_num;
      }
    }

    if (append) {
      for (pos = 0; pos < size; pos++) {
        order[pos]++;
      }

      order[size] = 0;
    }
  }
}

static void log2log(FriBidiStrIndex *order, u_int cur_pos, u_int size) {
  u_int pos;

  for (pos = cur_pos; pos < size; pos++) {
    order[pos] = pos;
  }
}

int vt_bidi(vt_bidi_state_t state, vt_char_t *src, u_int size, vt_bidi_mode_t bidi_mode,
            const char *separators) {
  FriBidiChar *fri_src;
  FriBidiCharType fri_type;
  FriBidiStrIndex *fri_order;
  int had_rtl;
  u_int cur_pos;
  ef_charset_t cs;
  u_int32_t code;
  u_int count;
  int ret;

  had_rtl = HAS_RTL(state);

  state->rtl_state = 0;

  if (size == 0) {
    state->size = 0;

    return 0;
  }

  if ((fri_src = alloca(sizeof(FriBidiChar) * size)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

    return 0;
  }

  if ((fri_order = alloca(sizeof(FriBidiStrIndex) * size)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

    return 0;
  }

  fri_type = FRIBIDI_TYPE_ON;

  if (bidi_mode == BIDI_ALWAYS_RIGHT) {
    SET_HAS_RTL(state);
  }

  for (count = 0, cur_pos = 0; count < size; count++) {
    cs = vt_char_cs(&src[count]);
    code = vt_char_code(&src[count]);

    if (cs == US_ASCII) {
      if (!isalpha(code)) {
        if (vt_get_picture_char(&src[count])) {
          fri_src[count] = 'a';
        } else if (separators && strchr(separators, code)) {
          if (HAS_RTL(state)) {
            log2vis(fri_src, count, &fri_type, bidi_mode, fri_order, cur_pos, 1);
          } else {
            fri_type = FRIBIDI_TYPE_LTR;
            log2log(fri_order, cur_pos, count + 1);
          }

          cur_pos = count + 1;
        } else {
          fri_src[count] = code;
        }
      } else {
        fri_src[count] = code;
      }
    } else if (cs == ISO10646_UCS4_1) {
      if (0x2500 <= code && code <= 0x259f) {
        goto decsp;
      } else {
        fri_src[count] = code;

        if (!HAS_RTL(state) && (fribidi_get_type(fri_src[count]) & FRIBIDI_MASK_RTL)) {
          SET_HAS_RTL(state);
        }
      }
    } else if (cs == DEC_SPECIAL) {
    decsp:
      bidi_mode = BIDI_ALWAYS_LEFT;

      if (HAS_RTL(state)) {
        log2vis(fri_src, count, &fri_type, bidi_mode, fri_order, cur_pos, 1);
      } else {
        log2log(fri_order, cur_pos, count + 1);
      }

      cur_pos = count + 1;
    } else if (IS_ISCII(cs)) {
      return -2; /* iscii */
    } else {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " %x is not ucs.\n", cs);
#endif

      /*
       * Regarded as NEUTRAL character.
       */
      fri_src[count] = ' ';
    }
  }

  if (HAS_RTL(state)) {
    log2vis(fri_src, size, &fri_type, bidi_mode, fri_order, cur_pos, 0);

    count = 0;

    if (state->size != size) {
      void *p;

      if (!(p = realloc(state->visual_order, sizeof(u_int16_t) * size))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " realloc() failed.\n");
#endif

        state->size = 0;

        return 0;
      }

      state->visual_order = p;
      state->size = size;

      ret = 2; /* order is changed */
    } else {
      ret = 1; /* order is not changed */

      for (; count < size; count++) {
        if (state->visual_order[count] != fri_order[count]) {
          ret = 2; /* order_is_changed */
          break;
        }
      }
    }

    for (; count < size; count++) {
      state->visual_order[count] = fri_order[count];
    }

#ifdef __DEBUG
    bl_msg_printf("utf8 text => \n");
    for (count = 0; count < size; count++) {
      bl_msg_printf("%.4x ", fri_src[count]);
    }
    bl_msg_printf("\n");

    bl_msg_printf("visual order => ");
    for (count = 0; count < size; count++) {
      bl_msg_printf("%.2d ", state->visual_order[count]);
    }
    bl_msg_printf("\n");
#endif

#ifdef DEBUG
    for (count = 0; count < size; count++) {
      if (state->visual_order[count] >= size) {
        bl_warn_printf(BL_DEBUG_TAG " visual order(%d) of %d is illegal.\n",
                       state->visual_order[count], count);

        bl_msg_printf("returned order => ");
        for (count = 0; count < size; count++) {
          bl_msg_printf("%d ", state->visual_order[count]);
        }
        bl_msg_printf("\n");

        abort();
      }
    }
#endif
  } else {
    state->size = 0;

    return -1; /* ot layout */
  }

  if (fri_type == FRIBIDI_TYPE_RTL) {
    SET_BASE_RTL(state);
  }

  state->bidi_mode = bidi_mode;

  return ret;
}

int vt_bidi_copy(vt_bidi_state_t dst, vt_bidi_state_t src, int optimize) {
  u_int16_t *p;

  if (optimize && !HAS_RTL(src)) {
    vt_bidi_destroy(dst);

    return -1;
  } else if (src->size == 0) {
    free(dst->visual_order);
    p = NULL;
  } else if ((p = realloc(dst->visual_order, sizeof(u_int16_t) * src->size))) {
    memcpy(p, src->visual_order, sizeof(u_int16_t) * src->size);
  } else {
    return 0;
  }

  dst->visual_order = p;
  dst->size = src->size;
  dst->rtl_state = src->rtl_state;
  dst->bidi_mode = src->bidi_mode;

  return 1;
}

int vt_bidi_reset(vt_bidi_state_t state) {
  state->size = 0;

  return 1;
}

u_int32_t vt_bidi_get_mirror_char(u_int32_t ch) {
  FriBidiChar mirror;

  if (fribidi_get_mirror_char(ch, &mirror)) {
    return mirror;
  } else {
    return 0;
  }
}

int vt_is_rtl_char(u_int32_t ch) {
  return (fribidi_get_bidi_type(ch) & FRIBIDI_MASK_RTL) == FRIBIDI_MASK_RTL;
}
