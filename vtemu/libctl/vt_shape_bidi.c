/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../vt_shape.h"

#include <string.h>        /* strncpy */
#include <pobl/bl_mem.h>   /* alloca */
#include <pobl/bl_debug.h> /* bl_msg_printf */
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

/*
 * 4 arabic codes -> 1 ligature glyph (Dynamically combined)
 * It is not enabled for now, because it doesn't match terminal emulators
 * which assume fixed-pitch columns.
 *
 * If ENABLE_COMB4 is enabled, fix adjust_comb_pos_in_order() in vt_bidi.c and
 * vt_line_bidi_{visual|logical}.c in vt_line_bidi.c.
 */
#if 0
#define ENABLE_COMB4
#endif

typedef struct arabic_present {
  u_int16_t base;

  /* presentations. right or left is visual order's one. */
  u_int16_t no_joining_present;
  u_int16_t right_joining_present;
  u_int16_t left_joining_present;
  u_int16_t both_joining_present;

} arabic_present_t;

typedef struct arabic_comb2_present {
  u_int16_t base[2]; /* logical order */
  u_int16_t no_joining_present;
  u_int16_t right_joining_present;

} arabic_comb2_present_t;

typedef struct arabic_comb4_present {
  u_int16_t base[4]; /* logical order */
  u_int16_t no_joining_present;

} arabic_comb4_present_t;

/* --- static variables --- */

/*
 * Not used (Arabic Presentation Forms-A)
 * FBDD(0677), FC00-FDF1
 *
 * FBEA = FE8B(0626) + FE8E(0627)
 * FBEB = FE8C(0626) + FE8E(0627)
 * FBEE = FE8B(0626) + FEEE(0648)
 * FBEF = FE8C(0626) + FEEE(0648)
 * FBF0 = FB8B(0626) + FBD8(06C7)
 * FBF1 = FE8C(0626) + FBD8(06C7)
 * FBF2 = FB8B(0626) + FBDA(06C6)
 * FBF3 = FE8C(0626) + FBDA(06C6)
 * FBF4 = FB8B(0626) + FBDC(06C8)
 * FBF5 = FB8C(0626) + FBDC(06C8)
 * FBF6 = FB8B(0626) + FBE5(06D0)
 * FBF7 = FE8C(0626) + FBE5(06D0)
 * FBF8 = FE8B(0626) + FBE7(06D0)
 * FBF9 = FE8B(0626) + FEF0(0649)
 * FBFA = FE8C(0626) + FEF0(0649)
 * FBFB = FE8B(0626) + FBE9(0649)
 */

/* See CAN_BE_COMPLEX_SHAPE in vt_bidi.h */
static arabic_present_t arabic_present_table[] = {
  { 0x0621, 0xFE80, 0x0000, 0x0000, 0x0000, },
  { 0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000, },
  { 0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000, },
  { 0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000, },
  { 0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000, },
  { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, },
  { 0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000, },
  { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92, },
  { 0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000, },
  { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98, },
  { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, },
  { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, },
  { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, },
  { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, },
  { 0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000, },
  { 0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000, },
  { 0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000, },
  { 0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000, },
  { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, },
  { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, },
  { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, },
  { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, },
  { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, },
  { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, },
  { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC, },
  { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0, },
  { 0x0640, 0x0640, 0x0640, 0x0640, 0x0640, },
  { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4, },
  { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8, },
  { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, },
  { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, },
  { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, },
  { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, },
  { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, },
  { 0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000, },
  { 0x0649, 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9, },
  { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, },
  { 0x0671, 0xFB50, 0xFB51, 0x0000, 0x0000, },
  { 0x0679, 0xFB66, 0xFB67, 0xFB68, 0xFB69, },
  { 0x067A, 0xFB5E, 0xFB5F, 0xFB60, 0xFB61, },
  { 0x067B, 0xFB52, 0xFB53, 0xFB54, 0xFB55, },
  { 0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59, },
  { 0x067F, 0xFB62, 0xFB63, 0xFB64, 0xFB65, },
  { 0x0680, 0xFB5A, 0xFB5B, 0xFB5C, 0xFB5D, },
  { 0x0683, 0xFB76, 0xFB77, 0xFB78, 0xFB79, },
  { 0x0684, 0xFB72, 0xFB73, 0xFB74, 0xFB75, },
  { 0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D, },
  { 0x0687, 0xFB7E, 0xFB7F, 0xFB80, 0xFB81, },
  { 0x0688, 0xFB88, 0xFB89, 0x0000, 0x0000, },
  { 0x068C, 0xFB84, 0xFB85, 0x0000, 0x0000, },
  { 0x068D, 0xFB82, 0xFB83, 0x0000, 0x0000, },
  { 0x068E, 0xFB86, 0xFB87, 0x0000, 0x0000, },
  { 0x0691, 0xFB8C, 0xFB8D, 0x0000, 0x0000, },
  { 0x0698, 0xFB8A, 0xFB8B, 0x0000, 0x0000, },
  { 0x06A4, 0xFB6A, 0xFB6B, 0xFB6C, 0xFB6D, },
  { 0x06A6, 0xFB6E, 0xFB6F, 0xFB70, 0xFB71, },
  { 0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91, },
  { 0x06AD, 0xFBD3, 0xFBD4, 0xFBD5, 0xFBD6, },
  { 0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95, },
  { 0x06B1, 0xFB9A, 0xFB9B, 0xFB9C, 0xFB9D, },
  { 0x06B3, 0xFB96, 0xFB97, 0xFB98, 0xFB99, },
  { 0x06BA, 0xFB9E, 0xFB9F, 0x0000, 0x0000, },
  { 0x06BB, 0xFBA0, 0xFBA1, 0xFBA2, 0xFBA3, },
  { 0x06BE, 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD, },
  { 0x06C0, 0xFBA4, 0xFBA5, 0x0000, 0x0000, },
  { 0x06C1, 0xFBA6, 0xFBA7, 0xFBA8, 0xFBA9, },
  { 0x06C5, 0xFBE0, 0xFBE1, 0x0000, 0x0000, },
  { 0x06C6, 0xFBD9, 0xFBDA, 0x0000, 0x0000, },
  { 0x06C7, 0xFBD7, 0xFBD8, 0x0000, 0x0000, },
  { 0x06C8, 0xFBDB, 0xFBDC, 0x0000, 0x0000, },
  { 0x06C9, 0xFBE2, 0xFBE3, 0x0000, 0x0000, },
  { 0x06CB, 0xFBDE, 0xFBDF, 0x0000, 0x0000, },
  { 0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF, },
  { 0x06D0, 0xFBE4, 0xFBE5, 0xFBE6, 0xFBE7, },
  { 0x06D2, 0xFBAE, 0xFBAF, 0x0000, 0x0000, },
  { 0x06D3, 0xFBB0, 0xFBB1, 0x0000, 0x0000, },
};

/* If you change this table, don't forget to change vt_is_arabic_combining(). */
static arabic_comb2_present_t arabic_comb2_present_table[] = {
  /* No mono space fonts have 0xFBEC and 0xFBED glyphs. */
#if 0
  { { 0x0626, 0x06D5, }, 0xFBEC, 0xFBED, },
#endif
  { { 0x0644, 0x0622, }, 0xFEF5, 0xFEF6, },
  { { 0x0644, 0x0623, }, 0xFEF7, 0xFEF8, },
  { { 0x0644, 0x0625, }, 0xFEF9, 0xFEFA, },
  { { 0x0644, 0x0627, }, 0xFEFB, 0xFEFC, },
};

#ifdef ENABLE_COMB4
/* If you change this table, don't forget to change vt_is_arabic_combining(). */
static arabic_comb4_present_t arabic_comb4_present_table[] = {
  { { 0x0627, 0x0644, 0x0644, 0x0647, }, 0xFDF2, },
  { { 0x0645, 0x062D, 0x0645, 0x062F, }, 0xFDF4, },
};
#endif

/* --- static functions --- */

static arabic_present_t *get_arabic_present(vt_char_t *ch) {
  u_int16_t code;
  int count;

  if (vt_char_cs(ch) == ISO10646_UCS4_1) {
    code = vt_char_code(ch);
  } else {
    return NULL;
  }

  if (arabic_present_table[0].base <= code &&
      code <= arabic_present_table[BL_ARRAY_SIZE(arabic_present_table) - 1].base) {
    count = BL_ARRAY_SIZE(arabic_present_table) / 2;

    if (code < arabic_present_table[count].base) {
      while (code < arabic_present_table[--count].base);
    } else if (code > arabic_present_table[count].base) {
      while (code > arabic_present_table[++count].base);
    } else {
      return &arabic_present_table[count];
    }

    if (code == arabic_present_table[count].base) {
      return &arabic_present_table[count];
    }
  }

  return NULL;
}

static u_int16_t get_arabic_comb2_present_code(vt_char_t *prev, /* can be NULL */
                                               vt_char_t *base, /* must be ISO10646_UCS4_1 */
                                               vt_char_t *comb  /* must be ISO10646_UCS4_1 */) {
  vt_char_t *seq[2];    /* logical order */
  u_int16_t ucs_seq[4]; /* logical order */
  int count;
  arabic_present_t *prev_present;

  if (prev) {
    vt_char_t *prev_comb;
    u_int size;

    if ((prev_comb = vt_get_combining_chars(prev, &size))) {
      /*
       * No comb chars having left_joining_present
       *
       * Comb characters(*) in 0x600-0x6ff don't appear in arabic_present_table,
       * (*) 0x610-0x061a, 0x64b-0x65f, 0x670, 0x6d6-0x6dc, 0x6df-0x6e4, 0x6e7,
       *     0x6e8, 0x6ea-0x6ed.
       *
       * Characters in arabic_comb2_present_table and arabic_comb4_present_table
       * don't have left_joining_present.
       */
      prev_present = NULL;
    } else {
      prev_present = get_arabic_present(prev);
    }
  } else {
    prev_present = NULL;
  }

  seq[0] = base;
  seq[1] = comb;

  for (count = 0; count < 2; count++) {
    if (seq[count] && vt_char_cs(seq[count]) == ISO10646_UCS4_1) {
      ucs_seq[count] = vt_char_code(seq[count]);
    } else {
      ucs_seq[count] = 0;
    }
  }

  /* Shape the current combinational character */
  for (count = 0; count < BL_ARRAY_SIZE(arabic_comb2_present_table); count++) {
    if (ucs_seq[0] == arabic_comb2_present_table[count].base[0] &&
        ucs_seq[1] == arabic_comb2_present_table[count].base[1]) {
      if (prev_present && prev_present->left_joining_present) {
        return arabic_comb2_present_table[count].right_joining_present;
      } else {
        return arabic_comb2_present_table[count].no_joining_present;
      }
    }
  }

  return 0;
}

#ifdef ENABLE_COMB4
static u_int16_t get_arabic_comb4_present_code(vt_char_t *comb, u_int len) {
  if (len == 3) {
    u_int count;

    for (count = 0; count < BL_ARRAY_SIZE(arabic_comb4_present_table); count++) {
      u_int idx;

      for (idx = 0; idx < 3; idx++) {
        /*
         * It is assumed that base char always equals to
         * arabic_comb4_present_table[count].base[0].
         */
        if (vt_char_code(comb + idx) != arabic_comb4_present_table[count].base[idx + 1]) {
          goto end;
        }
      }

      return arabic_comb4_present_table[count].no_joining_present;

    end:
      ;
    }
  }

  return 0;
}
#else
#define get_arabic_comb4_present_code(comb, len) (0)
#endif

/* --- global functions --- */

#ifdef BL_DEBUG
void TEST_vt_shape_bidi(void);
#endif

/*
 * 'src' characters are right to left (visual) order.
 */
u_int vt_shape_arabic(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len) {
  int count;
  arabic_present_t **list;
  u_int16_t code;
  vt_char_t *comb;
  vt_char_t *cur;
  vt_char_t *next; /* the same as 'prev' in logical order */
  u_int size;

#ifdef BL_DEBUG
  static int done;
  if (!done) {
    done = 1;
    TEST_vt_shape_bidi();
  }
#endif

  if ((list = alloca(sizeof(arabic_present_t*) * (src_len + 2))) == NULL) {
    return 0;
  }

  /* head is NULL */
  *(list++) = NULL;

  for (count = 0; count < src_len; count++) {
    list[count] = get_arabic_present(&src[count]);
  }

  /* tail is NULL */
  list[count] = NULL;

  cur = src;

  if (src_len <= 1) {
    next = NULL;
  } else {
    next = cur + 1;
  }

  for (count = 0; count < src_len && count < dst_len; count++) {
    comb = vt_get_combining_chars(cur, &size);

    if (comb &&
        ((code = get_arabic_comb2_present_code(count + 1 >= src_len ? NULL : &src[count + 1],
                                              vt_get_base_char(cur), comb)) ||
         (code = get_arabic_comb4_present_code(comb, size)))) {
      vt_char_copy(&dst[count], vt_get_base_char(cur));
      vt_char_set_code(&dst[count], code);
    } else if (list[count]) {
#if 0
      /*
       * Tanween characters combining their proceeded characters will
       * be ignored by vt_get_base_char(cur).
       */
      vt_char_copy(&dst[count], vt_get_base_char(cur));
#else
      vt_char_copy(&dst[count], cur);
#endif

      if (list[count - 1] && list[count - 1]->right_joining_present) {
        if ((list[count + 1] && list[count + 1]->left_joining_present) &&
            !(next && (comb = vt_get_combining_chars(next, &size)) &&
              get_arabic_comb2_present_code(count + 2 >= src_len ? NULL : &src[count + 2],
                                            vt_get_base_char(next), comb))) {
          if (list[count]->both_joining_present) {
            code = list[count]->both_joining_present;
          } else if (list[count]->left_joining_present) {
            code = list[count]->left_joining_present;
          } else if (list[count]->right_joining_present) {
            code = list[count]->right_joining_present;
          } else {
            code = list[count]->no_joining_present;
          }
        } else if (list[count]->left_joining_present) {
          code = list[count]->left_joining_present;
        } else {
          code = list[count]->no_joining_present;
        }
      } else if ((list[count + 1] && list[count + 1]->left_joining_present) &&
                 !(next && (comb = vt_get_combining_chars(next, &size)) &&
                   get_arabic_comb2_present_code(count + 2 >= src_len ? NULL : &src[count + 2],
                                                 vt_get_base_char(next), comb))) {
        if (list[count]->right_joining_present) {
          code = list[count]->right_joining_present;
        } else {
          code = list[count]->no_joining_present;
        }
      } else {
        code = list[count]->no_joining_present;
      }

      if (code) {
        vt_char_set_code(&dst[count], code);
      }
    } else {
      vt_char_copy(&dst[count], cur);
    }

    cur = next;
    next++;
  }

  return count;
}

u_int vt_is_arabic_combining(u_int32_t *str, u_int len) {
  if (len >= 2) {
#if 0
    if (str[0] == 0x626) {
      u_int count;

      for(count = 0; count < 1; count++) {
        if (str[1] == arabic_comb2_present_table[count].base[1]) {
          return 1;
        }
      }
    } else
#endif
    if (str[0] == 0x644) {
      u_int count;

      for(count = 0; count < BL_ARRAY_SIZE(arabic_comb2_present_table); count++) {
        if (str[1] == arabic_comb2_present_table[count].base[1]) {
          return 1;
        }
      }
    }
#ifdef ENABLE_COMB4
    else if (len >= 4) {
      u_int count;

      for(count = 0; count < BL_ARRAY_SIZE(arabic_comb4_present_table); count++) {
        u_int idx;

        for (idx = 0; idx < 4; idx++) {
          if (str[idx] != arabic_comb4_present_table[count].base[idx]) {
            goto end;
          }
        }

        return 3;

      end:
        ;
      }
    }
#endif
  }

  return 0;
}

#ifdef BL_DEBUG

#include <assert.h>

void TEST_vt_shape_bidi(void) {
  int count;
  vt_char_t ch;
  arabic_present_t *present;

  vt_char_init(&ch);
  vt_char_set_cs(&ch, ISO10646_UCS4_1);

  for (count = 0; count < BL_ARRAY_SIZE(arabic_present_table); count++) {
    vt_char_set_code(&ch, arabic_present_table[count].base);
    present = get_arabic_present(&ch);
    assert(arabic_present_table[count].base == present->base);
  }

  bl_msg_printf("PASS vt_shape_bidi test.\n");
}

#endif
