/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_font_cache.h"

#include <stdio.h>        /* sprintf */
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */

/* --- static variables --- */

static ui_font_cache_t **font_caches;
static u_int num_of_caches;
static int leftward_double_drawing;

/* --- static functions --- */

#ifdef DEBUG

static void dump_cached_fonts(ui_font_cache_t *font_cache) {
  u_int count;
  u_int size;
  BL_PAIR(ui_font) * f_array;

  bl_debug_printf(BL_DEBUG_TAG " cached fonts info\n");
  bl_map_get_pairs_array(font_cache->xfont_table, f_array, size);
  for (count = 0; count < size; count++) {
    if (f_array[count]->value != NULL) {
      ui_font_dump(f_array[count]->value);
    }
  }
}

#endif

/*
 * Call this function after init all members except font_table
 */
static int init_usascii_font(ui_font_cache_t *font_cache) {
  return (font_cache->usascii_font = ui_font_cache_get_xfont(
              font_cache, NORMAL_FONT_OF(font_cache->usascii_font_cs))) != NULL;
}

static BL_MAP(ui_font) xfont_table_new(void) {
  BL_MAP(ui_font) xfont_table;

  bl_map_new_with_size(vt_font_t, ui_font_t*, xfont_table, bl_map_hash_int, bl_map_compare_int, 16);

  return xfont_table;
}

static int xfont_table_delete(BL_MAP(ui_font) xfont_table) {
  int count;
  u_int size;
  BL_PAIR(ui_font) * f_array;

  bl_map_get_pairs_array(xfont_table, f_array, size);

  for (count = 0; count < size; count++) {
    if (f_array[count]->value != NULL) {
      ui_font_delete(f_array[count]->value);
    }
  }

  bl_map_delete(xfont_table);

  return 1;
}

#ifdef USE_XLIB
static char *get_font_name_list_for_fontset(ui_font_cache_t *font_cache) {
  char *font_name_list;
  char *p;
  size_t list_len;

  if (font_cache->font_config->type_engine != TYPE_XCORE) {
    ui_font_config_t *font_config;

    if ((font_config = ui_acquire_font_config(
             TYPE_XCORE, font_cache->font_config->font_present & ~FONT_AA)) == NULL) {
      font_name_list = NULL;
    } else {
      font_name_list = ui_get_all_config_font_names(font_config, font_cache->font_size);

      ui_release_font_config(font_config);
    }
  } else {
    font_name_list = ui_get_all_config_font_names(font_cache->font_config, font_cache->font_size);
  }

  if (font_name_list) {
    list_len = strlen(font_name_list);
  } else {
    list_len = 0;
  }

  if ((p = malloc(list_len + 28 + DIGIT_STR_LEN(font_cache->font_size) + 1)) == NULL) {
    return font_name_list;
  }

  if (font_name_list) {
    sprintf(p, "%s,-*-*-medium-r-*--%d-*-*-*-*-*", font_name_list, font_cache->font_size);
    free(font_name_list);
  } else {
    sprintf(p, "-*-*-medium-r-*--%d-*-*-*-*-*", font_cache->font_size);
  }

  return p;
}
#endif

/* --- global functions --- */

void ui_set_use_leftward_double_drawing(int use) {
  if (num_of_caches > 0) {
    bl_msg_printf("Unable to change leftward_double_drawing option.\n");
  } else {
    leftward_double_drawing = use;
  }
}

ui_font_cache_t *ui_acquire_font_cache(Display *display, u_int font_size,
                                       ef_charset_t usascii_font_cs, ui_font_config_t *font_config,
                                       int use_multi_col_char, u_int letter_space) {
  int count;
  ui_font_cache_t *font_cache;
  void *p;

  for (count = 0; count < num_of_caches; count++) {
    if (font_caches[count]->display == display && font_caches[count]->font_size == font_size &&
        font_caches[count]->usascii_font_cs == usascii_font_cs &&
        font_caches[count]->font_config == font_config &&
        font_caches[count]->use_multi_col_char == use_multi_col_char &&
        font_caches[count]->letter_space == letter_space) {
      font_caches[count]->ref_count++;

      return font_caches[count];
    }
  }

  if ((p = realloc(font_caches, sizeof(ui_font_cache_t*) * (num_of_caches + 1))) == NULL) {
    return NULL;
  }

  font_caches = p;

  if ((font_cache = malloc(sizeof(ui_font_cache_t))) == NULL) {
    return NULL;
  }

  font_cache->font_config = font_config;

  font_cache->xfont_table = xfont_table_new();

  font_cache->display = display;
  font_cache->font_size = font_size;
  font_cache->usascii_font_cs = usascii_font_cs;
  font_cache->use_multi_col_char = use_multi_col_char;
  font_cache->letter_space = letter_space;
  font_cache->ref_count = 1;

  font_cache->prev_cache.font = 0;
  font_cache->prev_cache.xfont = NULL;

  if (!init_usascii_font(font_cache)) {
    xfont_table_delete(font_cache->xfont_table);
    free(font_cache);

    return NULL;
  }

  return font_caches[num_of_caches++] = font_cache;
}

int ui_release_font_cache(ui_font_cache_t *font_cache) {
  int count;

  if (--font_cache->ref_count > 0) {
    return 1;
  }

  for (count = 0; count < num_of_caches; count++) {
    if (font_caches[count] == font_cache) {
      font_caches[count] = font_caches[--num_of_caches];

      xfont_table_delete(font_cache->xfont_table);
      free(font_cache);

      if (num_of_caches == 0) {
        free(font_caches);
        font_caches = NULL;
      }

      return 1;
    }
  }

  return 0;
}

int ui_font_cache_unload(ui_font_cache_t *font_cache) {
  /*
   * Discarding existing cache.
   */
  xfont_table_delete(font_cache->xfont_table);
  font_cache->usascii_font = NULL;
  font_cache->prev_cache.font = 0;
  font_cache->prev_cache.xfont = NULL;

  /*
   * Creating new cache.
   */
  font_cache->xfont_table = xfont_table_new();

  if (!init_usascii_font(font_cache)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " ui_font_cache_unload failed. Should ui_release_font_cache "
                    "this font cache.\n");
#endif

    return 0;
  }

  return 1;
}

int ui_font_cache_unload_all(void) {
  int count;

  for (count = 0; count < num_of_caches; count++) {
    ui_font_cache_unload(font_caches[count]);
  }

  return 1;
}

ui_font_t *ui_font_cache_get_xfont(ui_font_cache_t *font_cache, vt_font_t font) {
  ui_font_present_t font_present;
  vt_font_t font_for_config;
  int result;
  ui_font_t *xfont;
  BL_PAIR(ui_font) fn_pair;
  char *fontname;
  int use_medium_for_bold;
  u_int col_width;
  int size_attr;

  font_present = font_cache->font_config->font_present;

  if (FONT_CS(font) == US_ASCII) {
    font &= ~US_ASCII;
    font |= font_cache->usascii_font_cs;
    font_for_config = font;
  } else {
    font_for_config = font;

    if (FONT_CS(font) == ISO10646_UCS4_1_V) {
      font_present |= FONT_VAR_WIDTH;
      font_for_config &= ~ISO10646_UCS4_1_V;
      font_for_config |= ISO10646_UCS4_1;
    }
  }

  if (font_cache->prev_cache.xfont && font_cache->prev_cache.font == font) {
    return font_cache->prev_cache.xfont;
  }

  bl_map_get(font_cache->xfont_table, font, fn_pair);
  if (fn_pair) {
    return fn_pair->value;
  }

  if (font == NORMAL_FONT_OF(font_cache->usascii_font_cs)) {
    col_width = 0;
  } else {
    col_width = font_cache->usascii_font->width;
  }

  use_medium_for_bold = 0;

  if ((fontname = ui_get_config_font_name(font_cache->font_config, font_cache->font_size,
                                          font_for_config)) == NULL) {
    vt_font_t next_font;
    int scalable;

    next_font = font_for_config;

#ifndef TYPE_XCORE_SCALABLE
    if (font_cache->font_config->type_engine == TYPE_XCORE) {
      /*
       * If the type engine doesn't support scalable fonts,
       * medium weight font (drawn doubly) is used for bold.
       */
      scalable = 0;
    } else
#endif
    {
      /*
       * If the type engine supports scalable fonts,
       * the face of medium weight / r slant font is used
       * for bold and italic.
       */
      scalable = 1;
    }

    while (next_font & (FONT_BOLD | FONT_ITALIC)) {
      if (next_font & FONT_BOLD) {
        next_font &= ~FONT_BOLD;
      } else /* if( next_font & FONT_ITALIC) */
      {
        if (!scalable) {
          break;
        }

        next_font &= ~FONT_ITALIC;
      }

      if ((fontname = ui_get_config_font_name(font_cache->font_config, font_cache->font_size,
                                              next_font))) {
        if ((font & FONT_BOLD) && !scalable) {
          use_medium_for_bold = 1;
        }

        goto found;
      }
    }
  }

found:
  size_attr = SIZE_ATTR_OF(font);

  if ((xfont = ui_font_new(font_cache->display, FONT_WITHOUT_SIZE_ATTR(font), size_attr,
                           font_cache->font_config->type_engine, font_present, fontname,
                           font_cache->font_size, col_width, use_medium_for_bold,
                           font_cache->letter_space)) ||
      (size_attr &&
       (xfont = ui_font_new(font_cache->display, NORMAL_FONT_OF(font_cache->usascii_font_cs),
                            size_attr, font_cache->font_config->type_engine, font_present, fontname,
                            font_cache->font_size, col_width, use_medium_for_bold,
                            font_cache->letter_space)))) {
    if (!font_cache->use_multi_col_char) {
      ui_change_font_cols(xfont, 1);
    }

    if (xfont->double_draw_gap && leftward_double_drawing) {
      xfont->double_draw_gap = -1;
    }
  }
#ifdef DEBUG
  else {
    bl_warn_printf(BL_DEBUG_TAG " font for id %x doesn't exist.\n", font);
  }
#endif

  free(fontname);

  /*
   * If this font doesn't exist, NULL(which indicates it) is cached.
   */
  bl_map_set(result, font_cache->xfont_table, font, xfont);

  font_cache->prev_cache.font = font;
  font_cache->prev_cache.xfont = xfont;

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Font %x for id %x was cached.%s\n", xfont, font,
                  use_medium_for_bold ? "(medium font is used for bold.)" : "");
#endif

  return xfont;
}

XFontSet ui_font_cache_get_fontset(ui_font_cache_t *font_cache) {
#if defined(USE_XLIB)

  XFontSet fontset;
  char *list_str;
  char **missing;
  int miss_num;
  char *def_str;

  if ((list_str = get_font_name_list_for_fontset(font_cache)) == NULL) {
    return None;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " font set list -> %s\n", list_str);
#endif

  fontset = XCreateFontSet(font_cache->display, list_str, &missing, &miss_num, &def_str);

  free(list_str);

#ifdef DEBUG
  if (miss_num) {
    int count;

    bl_warn_printf(BL_DEBUG_TAG " missing charsets ...\n");
    for (count = 0; count < miss_num; count++) {
      bl_msg_printf(" %s\n", missing[count]);
    }
  }
#endif

  XFreeStringList(missing);

  return fontset;

#elif defined(USE_WIN32GUI)

  static LOGFONT logfont;

  ZeroMemory(&logfont, sizeof(logfont));
  GetObject(font_cache->usascii_font->xfont->fid, sizeof(logfont), &logfont);

  return &logfont;

#else

  return None;

#endif
}
