/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_iscii.h"

#include <pobl/bl_str.h> /* bl_snprintf */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <indian.h>

#ifndef LIBDIR
#define INDLIB_DIR "/usr/local/lib/mlterm/"
#else
#define INDLIB_DIR LIBDIR "/mlterm/"
#endif

#define A2IMAXBUFF 30

#if 0
#define __DEBUG
#endif

struct vt_isciikey_state {
  /* used for iitkeyb */
  char prev_key[A2IMAXBUFF];

  int8_t is_inscript;
};

#ifdef STATIC_LINK_INDIC_TABLES

/* for Android */

#include <table/bengali.table>
#include <table/hindi.table>
#if 0
#include <table/assamese.table>
#include <table/gujarati.table>
#include <table/kannada.table>
#include <table/malayalam.table>
#include <table/oriya.table>
#include <table/punjabi.table>
#include <table/telugu.table>
#endif

/* --- static variables --- */

static struct {
  struct tabl *tabl;
  size_t size;

} tables[] = {
#if 0
    {
     iscii_assamese_table, sizeof(iscii_assamese_table) / sizeof(struct tabl),
    },
    {
     iscii_bengali_table, sizeof(iscii_bengali_table) / sizeof(struct tabl),
    },
    {
     iscii_gujarati_table, sizeof(iscii_gujarati_table) / sizeof(struct tabl),
    },
    {
     iscii_hindi_table, sizeof(iscii_hindi_table) / sizeof(struct tabl),
    },
    {
     iscii_kannada_table, sizeof(iscii_kannada_table) / sizeof(struct tabl),
    },
    {
     iscii_malayalam_table, sizeof(iscii_malayalam_table) / sizeof(struct tabl),
    },
    {
     iscii_oriya_table, sizeof(iscii_oriya_table) / sizeof(struct tabl),
    },
    {
     iscii_punjabi_table, sizeof(iscii_punjabi_table) / sizeof(struct tabl),
    },
    {
     iscii_tamil_table, sizeof(iscii_tamil_table) / sizeof(struct tabl),
    },
    {
     iscii_telugu_table, sizeof(iscii_telugu_table) / sizeof(struct tabl),
    },
#else
    {
     NULL, 0,
    },
    {
     iscii_bengali_table, sizeof(iscii_bengali_table) / sizeof(struct tabl),
    },
    {
     NULL, 0,
    },
    {
     iscii_hindi_table, sizeof(iscii_hindi_table) / sizeof(struct tabl),
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
    {
     NULL, 0,
    },
#endif
};

/* --- static functions --- */

static struct tabl *get_iscii_table(int idx, size_t *size) {
  *size = tables[idx].size;

  return tables[idx].tabl;
}

static struct a2i_tabl *get_isciikey_table(int is_inscript, size_t *size) { return NULL; }

#else /* STATIC_LINK_INDIC_TABLES */

/* --- static variables --- */

static char *iscii_table_files[] = {
    "ind_assamese",  "ind_bengali", "ind_gujarati", "ind_hindi", "ind_kannada",
    "ind_malayalam", "ind_oriya",   "ind_punjabi",  "ind_tamil", "ind_telugu",
};

static struct tabl *(*get_iscii_tables[11])(u_int *);
static struct a2i_tabl *(*get_inscript_table)(u_int *);
static struct a2i_tabl *(*get_iitkeyb_table)(u_int *);

/* --- static functions --- */

static void *load_symbol(char *file) {
  void *handle;
  void *sym;

  if (!(handle = bl_dl_open(INDLIB_DIR, file)) && !(handle = bl_dl_open("", file))) {
    bl_msg_printf("Failed to open %s\n", file);

    return NULL;
  }

  bl_dl_close_at_exit(handle);

  if (!(sym = bl_dl_func_symbol(handle, "libind_get_table"))) {
    bl_dl_close(handle);
  }

  return sym;
}

static struct tabl *get_iscii_table(int idx, size_t *size) {
  if (!get_iscii_tables[idx] && !(get_iscii_tables[idx] = load_symbol(iscii_table_files[idx]))) {
    return NULL;
  }

  return (*get_iscii_tables[idx])(size);
}

static struct a2i_tabl *get_isciikey_table(int is_inscript, size_t *size) {
  if (is_inscript) {
    if (!get_inscript_table && !(get_inscript_table = load_symbol("ind_inscript"))) {
      return NULL;
    }

    return (*get_inscript_table)(size);
  } else {
    if (!get_iitkeyb_table && !(get_iitkeyb_table = load_symbol("ind_iitkeyb"))) {
      return NULL;
    }

    return (*get_iitkeyb_table)(size);
  }
}

#endif /* STATIC_LINK_INDIC_TABLES */

/* --- global functions --- */

u_int vt_iscii_shape(ef_charset_t cs, u_char *dst, size_t dst_size, u_char *src) {
  struct tabl *table;
  size_t size;

  if (!IS_ISCII(cs)) {
    return 0;
  }

  if ((table = get_iscii_table(cs - ISCII_ASSAMESE, &size)) == NULL) {
    return 0;
  }

  /*
   * XXX
   * iscii2font() expects dst to be terminated by zero.
   * int iscii2font(struct tabl table[MAXLEN], char *input, char *output, int
   *sz) {
   *	...
   *	bzero(output,strlen(output));
   *	...          ^^^^^^^^^^^^^^
   * }
   */
  dst[0] = '\0';

  return iscii2font(table, src, dst, size);
}

vt_iscii_state_t vt_iscii_new(void) { return calloc(1, sizeof(struct vt_iscii_state)); }

int vt_iscii_delete(vt_iscii_state_t state) {
  free(state->num_of_chars_array);
  free(state);

  return 1;
}

int vt_iscii(vt_iscii_state_t state, vt_char_t *src, u_int src_len) {
  int dst_pos;
  int src_pos;
  u_char *iscii_buf;
  u_char *font_buf;
  u_int8_t *num_of_chars_array;
  u_int font_buf_len;
  u_int prev_font_filled;
  u_int iscii_filled;
  ef_charset_t cs;
  ef_charset_t prev_cs;
  int has_ucs;

  if ((iscii_buf = alloca(src_len * MAX_COMB_SIZE + 1)) == NULL) {
    return 0;
  }

  font_buf_len = src_len * MAX_COMB_SIZE + 1;
  if ((font_buf = alloca(font_buf_len)) == NULL) {
    return 0;
  }

  if ((num_of_chars_array = alloca(font_buf_len * sizeof(u_int8_t))) == NULL) {
    return 0;
  }

  state->has_iscii = 0;
  dst_pos = -1;
  prev_cs = cs = UNKNOWN_CS;
  has_ucs = 0;
  for (src_pos = 0; src_pos < src_len; src_pos++) {
    cs = vt_char_cs(src + src_pos);

    if (prev_cs != cs) {
      prev_font_filled = iscii_filled = 0;
      prev_cs = cs;
    }

    if (IS_ISCII(cs)) {
      u_int font_filled;
      u_int count;
      vt_char_t *comb;
      u_int num;

      iscii_buf[iscii_filled++] = vt_char_code(src + src_pos);
      comb = vt_get_combining_chars(src + src_pos, &num);
      for (; num > 0; num--) {
        iscii_buf[iscii_filled++] = vt_char_code(comb++);
      }
      iscii_buf[iscii_filled] = '\0';

      font_filled = vt_iscii_shape(cs, font_buf, font_buf_len, iscii_buf);

      if (font_filled < prev_font_filled) {
        if (font_filled == 0) {
          return 0;
        }

        count = prev_font_filled - font_filled;
        dst_pos -= count;

        for (; count > 0; count--) {
          num_of_chars_array[dst_pos] += num_of_chars_array[dst_pos + count];
        }

        prev_font_filled = font_filled; /* goto to the next if block */
      }

      if (dst_pos >= 0 && font_filled == prev_font_filled) {
        num_of_chars_array[dst_pos]++;
      } else {
        num_of_chars_array[++dst_pos] = 1;

        for (count = font_filled - prev_font_filled; count > 1; count--) {
          num_of_chars_array[++dst_pos] = 0;
        }
      }

      prev_font_filled = font_filled;

      state->has_iscii = 1;
    } else {
      if (cs == ISO10646_UCS4_1) {
        has_ucs = 1;
      }

      num_of_chars_array[++dst_pos] = 1;
    }
  }

  if (state->size != dst_pos + 1) {
    void *p;

    if (!(p = realloc(state->num_of_chars_array,
                      K_MAX(dst_pos + 1, src_len) * sizeof(*num_of_chars_array)))) {
      return 0;
    }

#ifdef __DEBUG
    if (p != state->num_of_chars_array) {
      bl_debug_printf(BL_DEBUG_TAG " REALLOC array %d(%p) -> %d(%p)\n", state->size,
                      state->num_of_chars_array, dst_pos + 1, p);
    }
#endif

    state->num_of_chars_array = p;
    state->size = dst_pos + 1;
  }

  memcpy(state->num_of_chars_array, num_of_chars_array, state->size * sizeof(*num_of_chars_array));

  return (!state->has_iscii && has_ucs) ? -1 : 1;
}

int vt_iscii_copy(vt_iscii_state_t dst, vt_iscii_state_t src, int optimize) {
  u_int8_t *p;

  if (optimize && !src->has_iscii) {
    vt_iscii_delete(dst);

    return -1;
  } else if (src->size == 0) {
    free(dst->num_of_chars_array);
    p = NULL;
  } else if ((p = realloc(dst->num_of_chars_array, sizeof(u_int8_t) * src->size))) {
    memcpy(p, src->num_of_chars_array, sizeof(u_int8_t) * src->size);
  } else {
    return 0;
  }

  dst->num_of_chars_array = p;
  dst->size = src->size;
  dst->has_iscii = src->has_iscii;

  return 1;
}

int vt_iscii_reset(vt_iscii_state_t state) {
  state->size = 0;

  return 1;
}

vt_isciikey_state_t vt_isciikey_state_new(int is_inscript) {
  vt_isciikey_state_t state;

  if ((state = malloc(sizeof(*state))) == NULL) {
    return NULL;
  }

  state->is_inscript = is_inscript;
  state->prev_key[0] = '\0';

  return state;
}

int vt_isciikey_state_delete(vt_isciikey_state_t state) {
  free(state);

  return 1;
}

size_t vt_convert_ascii_to_iscii(vt_isciikey_state_t state, u_char *iscii, size_t iscii_len,
                                 u_char *ascii, size_t ascii_len) {
  struct a2i_tabl *table;
  size_t size;
  u_char *dup;

  /*
   * ins2iscii() and iitk2iscii() return 2nd argument variable whose memory
   * is modified by converted iscii bytes.
   * So, enough memory (* A2IMAXBUFF) should be allocated here.
   */
  if ((dup = alloca(ascii_len * A2IMAXBUFF)) == NULL) {
    goto no_conv;
  }

  if ((table = get_isciikey_table(state->is_inscript, &size)) == NULL) {
    goto no_conv;
  }

  strncpy(dup, ascii, ascii_len);
  dup[ascii_len] = '\0';

  if (state->is_inscript) {
    bl_snprintf(iscii, iscii_len, "%s", ins2iscii(table, dup, size));
  } else {
    bl_snprintf(iscii, iscii_len, "%s", iitk2iscii(table, dup, state->prev_key, size));

    state->prev_key[0] = ascii[0];
    state->prev_key[1] = '\0';
  }

  return strlen(iscii);

no_conv:
  memmove(iscii, ascii, iscii_len);

  return ascii_len;
}
