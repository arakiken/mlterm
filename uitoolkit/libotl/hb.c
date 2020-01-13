/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <hb.h>
#ifdef USE_QUARTZ
#include <hb-coretext.h>
#else
#include <hb-ft.h>
#endif
#include <hb-ot.h> /* hb_ot_layout_has_substitution */
#ifdef USE_WIN32GUI
#include <windows.h>
#endif
#include <ctype.h> /* isalpha */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>

#if 0
#define __DEBUG
#endif

#ifndef HB_VERSION_ATLEAST
#define HB_VERSION_ATLEAST(a,b,c) !HB_VERSION_CHECK(a,b,c)
#endif

/* --- static functions --- */

static hb_feature_t *get_hb_features(const char *features, u_int *num) {
  static const char *prev_features;
  static hb_feature_t *hbfeatures;
  static u_int num_features;

  if (features != prev_features) {
    char *str;
    void *p;

    if ((str = alloca(strlen(features) + 1)) &&
        (p = realloc(hbfeatures,
                     sizeof(hb_feature_t) * (bl_count_char_in_str(features, ',') + 1)))) {
      hbfeatures = p;

      strcpy(str, features);
      num_features = 0;
      while ((p = bl_str_sep(&str, ","))) {
        if (hb_feature_from_string(p, -1, &hbfeatures[num_features])) {
          num_features++;
        }
      }

      if (num_features == 0) {
        free(hbfeatures);
        hbfeatures = NULL;
      }
    }

    prev_features = features;
  }

  *num = num_features;

  return hbfeatures;
}

static u_int convert_text_to_glyphs(void *hbfont, u_int32_t *shape_glyphs /* never NULL */,
                                    u_int num_shape_glyphs, int8_t *xoffsets, int8_t *yoffsets,
                                    u_int8_t *advances,
                                    u_int32_t *src /* never NULL */,
                                    u_int src_len, hb_script_t hbscript,
                                    hb_feature_t *hbfeatures, u_int hbfeatures_num) {
  static hb_buffer_t *buf;
  hb_glyph_info_t *info;
  u_int count;
  u_int num;

  if (!buf) {
    buf = hb_buffer_create();
  } else {
    hb_buffer_reset(buf);
  }

  hb_buffer_add_utf32(buf, src, src_len, 0, src_len);

#if 0
  hb_buffer_guess_segment_properties(buf);
#else
  hb_buffer_set_script(buf, hbscript);
  hb_buffer_set_direction(buf, hb_script_get_horizontal_direction(hbscript));
  hb_buffer_set_language(buf, hb_language_get_default());
#endif

  hb_shape(hbfont, buf, hbfeatures, hbfeatures_num);

  info = hb_buffer_get_glyph_infos(buf, &num);

  if (num > num_shape_glyphs) {
    num = num_shape_glyphs;
  }

  if (xoffsets /* && yoffsets && advances */) {
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &num);

    for (count = 0; count < num; count++) {
      shape_glyphs[count] = info[count].codepoint;
      xoffsets[count] = pos[count].x_offset / 64;
      yoffsets[count] = pos[count].y_offset / 64;
      advances[count] = pos[count].x_advance / 64;

#ifdef __DEBUG
      bl_msg_printf("glyph %x xoffset %d (%d/64) yoffset %d (%d/64) "
                    "xadvance %d (%d/64) yadvance %d (%d/64)\n",
                    shape_glyphs[count], (int)xoffsets[count], pos[count].x_offset,
                    (int)yoffsets[count], pos[count].y_offset,
                    (int)advances[count], pos[count].x_advance,
                    pos[count].y_advance / 64, pos[count].y_advance);
#endif
    }
  } else {
    for (count = 0; count < num; count++) {
      shape_glyphs[count] = info[count].codepoint;
    }
  }

#if 0
  hb_buffer_destroy(buf);
#endif

  return num;
}

#ifdef USE_WIN32GUI

#include <freetype/ftmodapi.h>

static FT_Library ftlib;
static u_int ref_count;

static void done_ft_face(void *p) {
  FT_Face face;

  face = p;
  free(face->generic.data);
  face->generic.data = NULL;
  FT_Done_Face(face);

  if (--ref_count == 0) {
    FT_Done_FreeType(ftlib);
    ftlib = NULL;
  }
}

#endif

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void *otl_open(void *obj) {
#if defined(USE_WIN32GUI)

  HDC hdc = obj;
  u_char buf[4];
  void *font_data;
  u_int size;
  int is_ttc;
  FT_Face face;

#define TTC_TAG ('t' << 0) + ('t' << 8) + ('c' << 16) + ('f' << 24)

  if (GetFontData(hdc, TTC_TAG, 0, &buf, 1) == 1) {
    is_ttc = 1;
    size = GetFontData(hdc, TTC_TAG, 0, NULL, 0);
  } else {
    is_ttc = 0;
    size = GetFontData(hdc, 0, 0, NULL, 0);
  }

  if (!(font_data = malloc(size))) {
    return NULL;
  }

  if (is_ttc) {
    GetFontData(hdc, TTC_TAG, 0, font_data, size);
  } else {
    GetFontData(hdc, 0, 0, font_data, size);
  }

  if (!ftlib) {
    if (FT_Init_FreeType(&ftlib) != 0) {
      free(font_data);

      return NULL;
    }
  }

  if (FT_New_Memory_Face(ftlib, font_data, size, 0, &face) == 0) {
    hb_font_t *hbfont;

    if ((hbfont = hb_ft_font_create(face, done_ft_face))) {
#if 1
      if (hb_ot_layout_has_substitution(hb_font_get_face(hbfont)))
#endif
      {
        face->generic.data = font_data;
        ref_count++;

        return hbfont;
      }

      hb_font_destroy(hbfont);
    }

    FT_Done_Face(face);
  }

  free(font_data);

  if (ref_count == 0) {
    FT_Done_FreeType(ftlib);
    ftlib = NULL;
  }

  return NULL;

#elif defined(USE_QUARTZ)

  hb_face_t *face;

  if ((face = hb_coretext_face_create(obj /* CGFont */))) {
    hb_font_t *font;

    /* XXX hb_ot_layout_has_substitution() of harfbuzz 1.1.3 always returns 0 */
    if (/* hb_ot_layout_has_substitution(face) && */ (font = hb_font_create(face))) {
      hb_ot_font_set_funcs(font);
      hb_face_destroy(face);

      return font;
    }

    hb_face_destroy(face);
  }

  return NULL;

#else

  hb_font_t *font;

  if ((font = hb_ft_font_create(obj, NULL))) {
#if 1
    if (hb_ot_layout_has_substitution(hb_font_get_face(font)))
#endif
    {
      return font;
    }

    hb_font_destroy(font);
  }

  return NULL;

#endif
}

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void otl_close(void *hbfont) {
  hb_font_destroy(hbfont);
}

static hb_script_t get_hb_script(u_int32_t code, int *is_rtl, hb_script_t default_hbscript) {
  hb_script_t hbscript;

  *is_rtl = 0;
  if (code < 0x590) {
    hbscript = default_hbscript;
  } else if (code < 0x900) {
    if (code < 0x600) {
      hbscript = HB_SCRIPT_HEBREW;
      *is_rtl = 1;
    } else if (code < 0x700) {
      hbscript = HB_SCRIPT_ARABIC;
      *is_rtl = 1;
    } else if (code < 0x750) {
      hbscript = HB_SCRIPT_SYRIAC;
      *is_rtl = 1;
    } else if (code < 0x780) {
      hbscript = HB_SCRIPT_ARABIC;
      *is_rtl = 1;
    } else if (code < 0x7c0) {
      hbscript = HB_SCRIPT_THAANA;
      *is_rtl = 1;
    } else if (code < 0x800) {
      hbscript = HB_SCRIPT_NKO;
      *is_rtl = 1;
    } else if (code < 0x840) {
      hbscript = HB_SCRIPT_SAMARITAN;
      *is_rtl = 1;
    } else if (code < 0x860) {
      hbscript = HB_SCRIPT_MANDAIC;
      *is_rtl = 1;
    } else if (code < 0x870) {
      hbscript = HB_SCRIPT_SYRIAC; /* Syriac Supplement? */
      *is_rtl = 1;
    } else if (code < 0x8a0) {
      hbscript = default_hbscript; /* Undefined area */
    } else /* if (code < 0x900) */ {
      hbscript = HB_SCRIPT_ARABIC;
      *is_rtl = 1;
    }
  } else if (code < 0xd80) {
    if (code < 0x980) {
      hbscript = HB_SCRIPT_DEVANAGARI;
    } else if (code < 0xa00) {
      hbscript = HB_SCRIPT_BENGALI;
    } else if (code < 0xa80) {
      /* PUNJABI */
      hbscript = HB_SCRIPT_GURMUKHI;
    } else if (code < 0xb00) {
      hbscript = HB_SCRIPT_GUJARATI;
    } else if (code < 0xb80) {
      hbscript = HB_SCRIPT_ORIYA;
    } else if (code < 0xc00) {
      hbscript = HB_SCRIPT_TAMIL;
    } else if (code < 0xc80) {
      hbscript = HB_SCRIPT_TELUGU;
    } else if (code < 0xd00) {
      hbscript = HB_SCRIPT_KANNADA;
    } else /* if (code < 0xd80) */ {
      hbscript = HB_SCRIPT_MALAYALAM;
    }
  } else if (0x10300 <= code && code < 0x10e80) {
    if (code < 0x10330) {
      hbscript = HB_SCRIPT_OLD_ITALIC;
      *is_rtl = 1;
    } else if (code < 0x10800) {
      hbscript = default_hbscript;
    } else if (code < 0x10840) {
      hbscript = HB_SCRIPT_CYPRIOT;
      *is_rtl = 1;
    } else if (code < 0x10860) {
      hbscript = HB_SCRIPT_IMPERIAL_ARAMAIC;
      *is_rtl = 1;
    } else if (code < 0x10880) {
      hbscript = default_hbscript;
    } else if (code < 0x108b0) {
#if HB_VERSION_ATLEAST(0,9,30)
      hbscript = HB_SCRIPT_NABATAEAN;
      *is_rtl = 1;
#else
      hbscript = default_hbscript;
#endif
    } else if (code < 0x10900) {
      hbscript = default_hbscript;
    } else if (code < 0x10920) {
      hbscript = HB_SCRIPT_PHOENICIAN;
      *is_rtl = 1;
    } else if (code < 0x10940) {
      hbscript = HB_SCRIPT_LYDIAN;
      *is_rtl = 1;
    } else if (code < 0x10a00) {
      hbscript = default_hbscript;
    } else if (code < 0x10a60) {
      hbscript = HB_SCRIPT_KHAROSHTHI;
      *is_rtl = 1;
    } else if (code < 0x10b00) {
      hbscript = default_hbscript;
    } else if (code < 0x10b40) {
      hbscript = HB_SCRIPT_AVESTAN;
      *is_rtl = 1;
    } else if (code < 0x10b60) {
      hbscript = HB_SCRIPT_INSCRIPTIONAL_PARTHIAN;
      *is_rtl = 1;
    } else if (code < 0x10b80) {
      hbscript = HB_SCRIPT_INSCRIPTIONAL_PAHLAVI;
      *is_rtl = 1;
    } else if (code < 0x10bb0) {
#if HB_VERSION_ATLEAST(0,9,30)
      hbscript = HB_SCRIPT_PSALTER_PAHLAVI;
      *is_rtl = 1;
#else
      hbscript = default_hbscript;
#endif
    } else if (code < 0x10c00) {
      hbscript = default_hbscript;
    } else if (code < 0x10c50) {
      hbscript = HB_SCRIPT_OLD_TURKIC;
      *is_rtl = 1;
    } else if (code < 0x10e60) {
      hbscript = default_hbscript;
    } else /* if (code < 0x10e80) */ {
      hbscript = HB_SCRIPT_ARABIC;
      *is_rtl = 1;
    }
  } else if (0x1e800 <= code && code < 0x1ef00) {
    if (code < 0x1e8f0) {
#if HB_VERSION_ATLEAST(0,9,30)
      hbscript = HB_SCRIPT_MENDE_KIKAKUI;
      *is_rtl = 1;
#else
      hbscript = default_hbscript;
#endif
    } else if (code < 0x1e900) {
      hbscript = default_hbscript;
    } else if (code < 0x1e960) {
#if HB_VERSION_ATLEAST(1,3,0)
      hbscript = HB_SCRIPT_ADLAM;
      *is_rtl = 1;
#else
      hbscript = default_hbscript;
#endif
    } else if (code < 0x1ee00) {
      hbscript = default_hbscript;
    } else /* if (code < 0x1ef00) */ {
      hbscript = HB_SCRIPT_ARABIC;
      *is_rtl = 1;
    }
  } else {
    hbscript = default_hbscript;
  }

  return hbscript;
}

/*
 * shape_glyphs != NULL && noshape_glyphs != NULL && src != NULL:
 *   noshape_glyphs -> shape_glyphs (called from vt_ot_layout.c)
 * shape_glyphs == NULL: src -> noshape_glyphs (called from vt_ot_layout.c)
 * cmaped == NULL: src -> shape_glyphs (Not used for now)
 *
 * num_shape_glyphs should be greater than src_len.
 */
#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
u_int otl_convert_text_to_glyphs(void *hbfont, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                 int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                 u_int32_t *noshape_glyphs, u_int32_t *src, u_int src_len,
                                 const char *script, const char *features, u_int fontsize) {
  u_int count;

  if (shape_glyphs == NULL) {
    hb_codepoint_t g;

    for (count = 0; count < src_len; count++) {
#if HB_VERSION_ATLEAST(1,2,3)
      hb_font_get_nominal_glyph(hbfont, src[count], &g);
#else
      hb_font_get_glyph(hbfont, src[count], 0, &g);
#endif
      noshape_glyphs[count] = g;
    }

    return src_len;
  } else {
    u_int32_t code;
    hb_script_t hbscript;
    hb_script_t cur_hbscript;
    hb_script_t default_hbscript;
    int is_rtl;
    int is_cur_rtl;
    hb_feature_t *hbfeatures;
    u_int hbfeatures_num;
    u_int num = 0;

    if (fontsize > 0) {
      u_int scale = fontsize << 6; /* fontsize x 64 */

#ifdef USE_WIN32GUI
      FT_Set_Pixel_Sizes(hb_ft_font_get_face(hbfont), fontsize, fontsize);
#endif

      hb_font_set_scale(hbfont, scale, scale);
    }

    hbfeatures = get_hb_features(features, &hbfeatures_num);
    default_hbscript = HB_TAG(script[0] & ~0x20, /* Upper case */
                              script[1] | 0x20,  /* Lower case */
                              script[2] | 0x20,  /* Lower case */
                              script[3] | 0x20); /* Lower case */

    cur_hbscript = get_hb_script(src[0], &is_cur_rtl, default_hbscript);

    for (count = 1; count < src_len; count++) {
      code = src[count];

      hbscript = get_hb_script(code, &is_rtl, default_hbscript);

      if (hbscript != cur_hbscript) {
        u_int count2 = count;
        u_int n;

        if (is_cur_rtl) {
          while (1) {
            if (code <= 0x7f) {
              if (isalpha(code)) {
                break;
              } else {
                /* Regard neutral ascii characters in RTL context as RTL. */
              }
            } else if (hbscript != cur_hbscript) {
              break;
            }

            count2++;

            if (code <= 0x7f) {
              /* Do not regard neutral ascii characters at the end of line as RTL. */
            } else {
              count = count2;
            }
            if (count2 == src_len) {
              count2--; /* for count++ */
              break;
            }

            code = src[count2];
            hbscript = get_hb_script(code, &is_rtl, default_hbscript);
          }
        }

        n = convert_text_to_glyphs(hbfont, shape_glyphs, num_shape_glyphs, xoffsets, yoffsets,
                                   advances, src, count, cur_hbscript, hbfeatures, hbfeatures_num);
        shape_glyphs += n;
        num_shape_glyphs -= n;
        xoffsets += n;
        yoffsets += n;
        advances += n;
        num += n;
        if (noshape_glyphs) {
          noshape_glyphs += count;
        }
        src += count;
        src_len -= count;
        count = count2 - count;

        cur_hbscript = hbscript;
        is_cur_rtl = is_rtl;
      }
    }

    num += convert_text_to_glyphs(hbfont, shape_glyphs, num_shape_glyphs, xoffsets, yoffsets,
                                  advances, src, count, cur_hbscript, hbfeatures, hbfeatures_num);

    return num;
  }
}
