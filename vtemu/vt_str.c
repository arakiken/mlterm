/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <string.h>

#include "vt_str.h"

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* malloc */

/* --- global functions --- */

/*
 * string functions
 */

int vt_str_init(vt_char_t *str, u_int size) {
  int count;

  for (count = 0; count < size; count++) {
    vt_char_init(str++);
  }

  return 1;
}

vt_char_t* __vt_str_init(vt_char_t *str, /* alloca()-ed memory (see vt_char.h) */
                         u_int size) {
  if (str == NULL) {
    /* alloca() failed. */

    return NULL;
  }

  if (!(vt_str_init(str, size))) {
    return NULL;
  }

  return str;
}

vt_char_t *vt_str_new(u_int size) {
  vt_char_t *str;

  if ((str = malloc(sizeof(vt_char_t) * size)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return NULL;
  }

  if (vt_str_init(str, size) == 0) {
    free(str);

    return NULL;
  }

  return str;
}

int vt_str_final(vt_char_t *str, u_int size) {
  int count;

  for (count = 0; count < size; count++) {
    vt_char_final(&str[count]);
  }

  return 1;
}

int vt_str_delete(vt_char_t *str, u_int size) {
  if (vt_str_final(str, size)) {
    free(str);

    return 1;
  } else {
    free(str);

    return 0;
  }
}

/*
 * dst and src may overlap.
 */
int vt_str_copy(vt_char_t *dst, vt_char_t *src, u_int size) {
  int count;

  if (size == 0 || dst == src) {
    return 0;
  }

  if (dst < src) {
    for (count = 0; count < size; count++) {
      vt_char_copy(dst++, src++);
    }
  } else if (dst > src) {
    dst += size;
    src += size;
    for (count = 0; count < size; count++) {
      vt_char_copy(--dst, --src);
    }
  }

  return 1;
}

u_int vt_str_cols(vt_char_t *chars, u_int len) {
  int count;
  u_int cols;

  cols = 0;

  for (count = 0; count < len; count++) {
    cols += vt_char_cols(&chars[count]);
  }

  return cols;
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same bytes, false is returned since
 * vt_char_t:multi_ch-s never point the same address.)
 */
int vt_str_equal(vt_char_t *str1, vt_char_t *str2, u_int len) {
  return memcmp(str1, str2, sizeof(vt_char_t) * len) == 0;
}

int vt_str_bytes_equal(vt_char_t *str1, vt_char_t *str2, u_int len) {
  int count;

  for (count = 0; count < len; count++) {
    if (!vt_char_code_equal(str1++, str2++)) {
      return 0;
    }
  }

  return 1;
}

#ifdef DEBUG

void vt_str_dump(vt_char_t *chars, u_int len) {
  int count;

  for (count = 0; count < len; count++) {
    vt_char_dump(&chars[count]);
  }

  bl_msg_printf("\n");
}

#endif
