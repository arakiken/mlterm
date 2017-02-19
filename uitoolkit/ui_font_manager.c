/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_font_manager.h"

#include <string.h>       /* strcat */
#include <stdio.h>        /* sprintf */
#include <pobl/bl_mem.h>  /* malloc/alloca */
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */

#if 0
#define __DEBUG
#endif

typedef struct encoding_to_cs_table {
  vt_char_encoding_t encoding;
  ef_charset_t cs;

} encoding_to_cs_table_t;

/* --- static variables --- */

/*
 * !!! Notice !!!
 * The order should be the same as vt_char_encoding_t.
 * US_ASCII font for encodings after VT_UTF8 is ISO8859_1_R. (see
 * ui_get_usascii_font_cs())
 */
static encoding_to_cs_table_t usascii_font_cs_table[] = {
    {VT_ISO8859_1, ISO8859_1_R},
    {VT_ISO8859_2, ISO8859_2_R},
    {VT_ISO8859_3, ISO8859_3_R},
    {VT_ISO8859_4, ISO8859_4_R},
    {VT_ISO8859_5, ISO8859_5_R},
    {VT_ISO8859_6, ISO8859_6_R},
    {VT_ISO8859_7, ISO8859_7_R},
    {VT_ISO8859_8, ISO8859_8_R},
    {VT_ISO8859_9, ISO8859_9_R},
    {VT_ISO8859_10, ISO8859_10_R},
    {VT_TIS620, TIS620_2533},
    {VT_ISO8859_13, ISO8859_13_R},
    {VT_ISO8859_14, ISO8859_14_R},
    {VT_ISO8859_15, ISO8859_15_R},
    {VT_ISO8859_16, ISO8859_16_R},
    {VT_TCVN5712, TCVN5712_3_1993},

    {VT_ISCII_ASSAMESE, ISO8859_1_R},
    {VT_ISCII_BENGALI, ISO8859_1_R},
    {VT_ISCII_GUJARATI, ISO8859_1_R},
    {VT_ISCII_HINDI, ISO8859_1_R},
    {VT_ISCII_KANNADA, ISO8859_1_R},
    {VT_ISCII_MALAYALAM, ISO8859_1_R},
    {VT_ISCII_ORIYA, ISO8859_1_R},
    {VT_ISCII_PUNJABI, ISO8859_1_R},
    {VT_ISCII_TELUGU, ISO8859_1_R},
    {VT_VISCII, VISCII},
    {VT_KOI8_R, KOI8_R},
    {VT_KOI8_U, KOI8_U},
    {VT_KOI8_T, KOI8_T},
    {VT_GEORGIAN_PS, GEORGIAN_PS},
    {VT_CP1250, CP1250},
    {VT_CP1251, CP1251},
    {VT_CP1252, CP1252},
    {VT_CP1253, CP1253},
    {VT_CP1254, CP1254},
    {VT_CP1255, CP1255},
    {VT_CP1256, CP1256},
    {VT_CP1257, CP1257},
    {VT_CP1258, CP1258},
    {VT_CP874, CP874},

    {VT_UTF8, ISO10646_UCS4_1},

};

#ifdef __ANDROID__
static u_int min_font_size = 10;
static u_int max_font_size = 40;
#else
static u_int min_font_size = 6;
static u_int max_font_size = 30;
#endif

/* --- static functions --- */

static int change_font_cache(ui_font_manager_t *font_man, ui_font_cache_t *font_cache) {
  ui_release_font_cache(font_man->font_cache);
  font_man->font_cache = font_cache;

  return 1;
}

static void adjust_font_size(u_int *font_size) {
  if (*font_size > max_font_size) {
    bl_msg_printf("font size %d is too large. %d is used.\n", *font_size, max_font_size);

    *font_size = max_font_size;
  } else if (*font_size < min_font_size) {
    bl_msg_printf("font size %d is too small. %d is used.\n", *font_size, min_font_size);

    *font_size = min_font_size;
  }
}

/* --- global functions --- */

int ui_set_font_size_range(u_int min_fsize, u_int max_fsize) {
  if (min_fsize == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " min_font_size must not be 0.\n");
#endif

    return 0;
  }

  if (max_fsize < min_fsize) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " max_font_size %d should be larger than min_font_size %d\n",
                   max_fsize, min_fsize);
#endif

    return 0;
  }

  min_font_size = min_fsize;
  max_font_size = max_fsize;

  return 1;
}

ui_font_manager_t *ui_font_manager_new(Display *display, ui_type_engine_t type_engine,
                                       ui_font_present_t font_present, u_int font_size,
                                       ef_charset_t usascii_font_cs, int use_multi_col_char,
                                       u_int step_in_changing_font_size, u_int letter_space,
                                       int use_bold_font, int use_italic_font) {
  ui_font_manager_t *font_man;

  if (!(font_man = malloc(sizeof(ui_font_manager_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return NULL;
  }

  adjust_font_size(&font_size);

  if ((!(font_man->font_config = ui_acquire_font_config(type_engine, font_present)) ||
       !(font_man->font_cache =
             ui_acquire_font_cache(display, font_size, usascii_font_cs, font_man->font_config,
                                   use_multi_col_char, letter_space)))) {
    ui_type_engine_t engine;

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Could not handle %s fonts.\n",
                    ui_get_type_engine_name(type_engine));
#endif

    for (engine = TYPE_XCORE;; engine++) {
      if (engine == type_engine) {
        continue;
      }

      if (font_man->font_config) {
        ui_release_font_config(font_man->font_config);
      }

      if (engine >= TYPE_ENGINE_MAX) {
        free(font_man);

        return NULL;
      }

      if ((font_man->font_config = ui_acquire_font_config(engine, font_present)) &&
          (font_man->font_cache =
               ui_acquire_font_cache(display, font_size, usascii_font_cs, font_man->font_config,
                                     use_multi_col_char, letter_space))) {
        break;
      }
    }

    bl_msg_printf("Fall back to %s.\n", ui_get_type_engine_name(engine));
  }

  if (max_font_size - min_font_size >= step_in_changing_font_size) {
    font_man->step_in_changing_font_size = step_in_changing_font_size;
  } else {
    font_man->step_in_changing_font_size = max_font_size - min_font_size;
  }

  font_man->use_bold_font = use_bold_font;
  font_man->use_italic_font = use_italic_font;
  font_man->size_attr = 0;
#ifdef USE_OT_LAYOUT
  font_man->use_ot_layout = 0;
#endif

  return font_man;
}

int ui_font_manager_delete(ui_font_manager_t *font_man) {
  ui_release_font_cache(font_man->font_cache);

  ui_release_font_config(font_man->font_config);
  font_man->font_config = NULL;

  free(font_man);

  return 1;
}

void ui_font_manager_set_attr(ui_font_manager_t *font_man, int size_attr, int use_ot_layout) {
  font_man->size_attr = size_attr;
#ifdef USE_OT_LAYOUT
  font_man->use_ot_layout = use_ot_layout;
#endif
}

ui_font_t *ui_get_font(ui_font_manager_t *font_man, vt_font_t font) {
  ui_font_t *xfont;

  if (!font_man->use_bold_font) {
    font &= ~FONT_BOLD;
  }

  if (!font_man->use_italic_font) {
    font &= ~FONT_ITALIC;
  }

  if (!(xfont = ui_font_cache_get_xfont(font_man->font_cache,
                                        SIZE_ATTR_FONT(font, font_man->size_attr)))) {
    xfont = font_man->font_cache->usascii_font;
  }

#ifdef USE_OT_LAYOUT
  if (FONT_CS(font) == ISO10646_UCS4_1_V) {
    xfont->use_ot_layout = 1;
  } else {
    xfont->use_ot_layout = font_man->use_ot_layout;
  }
#endif

  return xfont;
}

int ui_font_manager_usascii_font_cs_changed(ui_font_manager_t *font_man,
                                            ef_charset_t usascii_font_cs) {
  ui_font_cache_t *font_cache;

  if (usascii_font_cs == font_man->font_cache->usascii_font_cs) {
    return 0;
  }

  if ((font_cache = ui_acquire_font_cache(
           font_man->font_cache->display, font_man->font_cache->font_size, usascii_font_cs,
           font_man->font_config, font_man->font_cache->use_multi_col_char,
           font_man->font_cache->letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

/*
 * Return 1 if font present is successfully changed.
 * Return 0 if not changed.
 */
int ui_change_font_present(ui_font_manager_t *font_man, ui_type_engine_t type_engine,
                           ui_font_present_t font_present) {
  ui_font_config_t *font_config;
  ui_font_cache_t *font_cache;

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
  /*
   * FONT_AA is effective in xft or cairo, so following hack is necessary in
   * xlib.
   */
  if ((type_engine == TYPE_XCORE) && (font_man->font_config->font_present & FONT_AA)) {
    font_present &= ~FONT_AA;
  } else if ((font_present & FONT_AA) && font_man->font_config->type_engine == TYPE_XCORE &&
             type_engine == TYPE_XCORE) {
#if !defined(USE_TYPE_XFT) && defined(USE_TYPE_CAIRO)
    type_engine = TYPE_CAIRO;
#else
    type_engine = TYPE_XFT;
#endif
  }
#endif

  if (font_present == font_man->font_config->font_present &&
      type_engine == font_man->font_config->type_engine) {
    /* Same as current settings. */

    return 0;
  }

  if ((font_config = ui_acquire_font_config(type_engine, font_present)) == NULL) {
    return 0;
  }

  if ((font_cache = ui_acquire_font_cache(
           font_man->font_cache->display, font_man->font_cache->font_size,
           font_man->font_cache->usascii_font_cs, font_config,
           font_man->font_cache->use_multi_col_char, font_man->font_cache->letter_space)) == NULL) {
    ui_release_font_config(font_config);

    return 0;
  }

  change_font_cache(font_man, font_cache);

  ui_release_font_config(font_man->font_config);
  font_man->font_config = font_config;

  return 1;
}

ui_type_engine_t ui_get_type_engine(ui_font_manager_t *font_man) {
  return font_man->font_config->type_engine;
}

ui_font_present_t ui_get_font_present(ui_font_manager_t *font_man) {
  return font_man->font_config->font_present;
}

int ui_change_font_size(ui_font_manager_t *font_man, u_int font_size) {
  ui_font_cache_t *font_cache;

  adjust_font_size(&font_size);

  if (font_size == font_man->font_cache->font_size) {
    /* not changed (pretending to succeed) */

    return 1;
  }

  if (font_size < min_font_size || max_font_size < font_size) {
    return 0;
  }

  if ((font_cache = ui_acquire_font_cache(
           font_man->font_cache->display, font_size, font_man->font_cache->usascii_font_cs,
           font_man->font_config, font_man->font_cache->use_multi_col_char,
           font_man->font_cache->letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

int ui_larger_font(ui_font_manager_t *font_man) {
  u_int font_size;
  ui_font_cache_t *font_cache;

  if (font_man->font_cache->font_size + font_man->step_in_changing_font_size > max_font_size) {
    font_size = min_font_size;
  } else {
    font_size = font_man->font_cache->font_size + font_man->step_in_changing_font_size;
  }

  if ((font_cache = ui_acquire_font_cache(
           font_man->font_cache->display, font_size, font_man->font_cache->usascii_font_cs,
           font_man->font_config, font_man->font_cache->use_multi_col_char,
           font_man->font_cache->letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

int ui_smaller_font(ui_font_manager_t *font_man) {
  u_int font_size;
  ui_font_cache_t *font_cache;

  if (font_man->font_cache->font_size < min_font_size + font_man->step_in_changing_font_size) {
    font_size = max_font_size;
  } else {
    font_size = font_man->font_cache->font_size - font_man->step_in_changing_font_size;
  }

  if ((font_cache = ui_acquire_font_cache(
           font_man->font_cache->display, font_size, font_man->font_cache->usascii_font_cs,
           font_man->font_config, font_man->font_cache->use_multi_col_char,
           font_man->font_cache->letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

u_int ui_get_font_size(ui_font_manager_t *font_man) { return font_man->font_cache->font_size; }

int ui_set_use_multi_col_char(ui_font_manager_t *font_man, int flag) {
  ui_font_cache_t *font_cache;

  if (font_man->font_cache->use_multi_col_char == flag) {
    return 0;
  }

  if ((font_cache =
           ui_acquire_font_cache(font_man->font_cache->display, font_man->font_cache->font_size,
                                 font_man->font_cache->usascii_font_cs, font_man->font_config, flag,
                                 font_man->font_cache->letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

int ui_set_letter_space(ui_font_manager_t *font_man, u_int letter_space) {
  ui_font_cache_t *font_cache;

  if (font_man->font_cache->letter_space == letter_space) {
    return 0;
  }

  if ((font_cache =
           ui_acquire_font_cache(font_man->font_cache->display, font_man->font_cache->font_size,
                                 font_man->font_cache->usascii_font_cs, font_man->font_config,
                                 font_man->font_cache->use_multi_col_char, letter_space)) == NULL) {
    return 0;
  }

  change_font_cache(font_man, font_cache);

  return 1;
}

int ui_set_use_bold_font(ui_font_manager_t *font_man, int use_bold_font) {
  if (font_man->use_bold_font == use_bold_font) {
    return 0;
  }

  font_man->use_bold_font = use_bold_font;

  return 1;
}

int ui_set_use_italic_font(ui_font_manager_t *font_man, int use_italic_font) {
  if (font_man->use_italic_font == use_italic_font) {
    return 0;
  }

  font_man->use_italic_font = use_italic_font;

  return 1;
}

ef_charset_t ui_get_usascii_font_cs(vt_char_encoding_t encoding) {
  if (encoding < 0 ||
      sizeof(usascii_font_cs_table) / sizeof(usascii_font_cs_table[0]) <= encoding) {
    return ISO8859_1_R;
  }
#ifdef DEBUG
  else if (encoding != usascii_font_cs_table[encoding].encoding) {
    bl_warn_printf(BL_DEBUG_TAG " %x is illegal encoding.\n", encoding);

    return ISO8859_1_R;
  }
#endif
  else {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " us ascii font is %x cs\n", usascii_font_cs_table[encoding].cs);
#endif

    return usascii_font_cs_table[encoding].cs;
  }
}
