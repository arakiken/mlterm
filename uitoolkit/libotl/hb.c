/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef HB_HACK_FOR_QUARTZ
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-coretext.h>
#include <harfbuzz/hb-ot.h>
#else
#include <hb.h>
#ifdef USE_QUARTZ
#include <hb-coretext.h>
#include <hb-ot.h>
#else
#include <hb-ft.h>
#endif
#endif
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>

/* --- static functions --- */

static hb_feature_t* get_hb_features(const char* features, u_int* num) {
  static const char* prev_features;
  static hb_feature_t* hbfeatures;
  static u_int num_of_features;

  if (features != prev_features) {
    char* str;
    void* p;

    if ((str = alloca(strlen(features) + 1)) &&
        (p = realloc(hbfeatures,
                     sizeof(hb_feature_t) * (bl_count_char_in_str(features, ',') + 1)))) {
      hbfeatures = p;

      strcpy(str, features);
      num_of_features = 0;
      while ((p = bl_str_sep(&str, ","))) {
        if (hb_feature_from_string(p, -1, &hbfeatures[num_of_features])) {
          num_of_features++;
        }
      }

      if (num_of_features == 0) {
        free(hbfeatures);
        hbfeatures = NULL;
      }
    }

    prev_features = features;
  }

  *num = num_of_features;

  return hbfeatures;
}

#ifdef USE_WIN32GUI

#include <freetype/ftmodapi.h>

static FT_Library ftlib;
static u_int ref_count;

static void done_ft_face(void* p) {
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
    void*
    otl_open(void* obj, u_int size) {
#if defined(USE_WIN32GUI)

  FT_Face face;
  hb_font_t* hbfont;

  if (!ftlib) {
    if (FT_Init_FreeType(&ftlib) != 0) {
      free(obj);

      return NULL;
    }
  }

  if (FT_New_Memory_Face(ftlib, obj, size, 0, &face) == 0) {
    if ((hbfont = hb_ft_font_create(face, done_ft_face))) {
      face->generic.data = obj;
      ref_count++;

      return hbfont;
    }

    FT_Done_Face(face);
  }

  free(obj);

  if (ref_count == 0) {
    FT_Done_FreeType(ftlib);
    ftlib = NULL;
  }

  return NULL;

#elif defined(USE_QUARTZ)

  hb_face_t* face;

  if ((face = hb_coretext_face_create(obj /* CGFont */))) {
    hb_font_t* font;
    if ((font = hb_font_create(face))) {
      hb_ot_font_set_funcs(font);
      hb_face_destroy(face);

      return font;
    }
  }

  return NULL;

#else
  return hb_ft_font_create(obj, NULL);
#endif
}

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
    void
    otl_close(void* hbfont) {
  hb_font_destroy(hbfont);
}

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
    u_int
    otl_convert_text_to_glyphs(void* hbfont, u_int32_t* shaped, u_int shaped_len, int8_t* offsets,
                               u_int8_t* widths, u_int32_t* cmapped, u_int32_t* src, u_int src_len,
                               const char* script, const char* features, u_int fontsize) {
  if (src && cmapped) {
    memcpy(cmapped, src, sizeof(*src) * src_len);

    return src_len;
  } else {
    static hb_buffer_t* buf;
    hb_glyph_info_t* info;
    hb_glyph_position_t* pos;
    u_int count;
    u_int num;
    hb_feature_t* hbfeatures;

    if (fontsize > 0) {
      u_int scale = fontsize << 6; /* fontsize x 64 */

#ifdef USE_WIN32GUI
      FT_Set_Pixel_Sizes(hb_ft_font_get_face(hbfont), fontsize, fontsize);
#endif

      hb_font_set_scale(hbfont, scale, scale);
    }

    if (!buf) {
      buf = hb_buffer_create();
    } else {
      hb_buffer_reset(buf);
    }

    if (cmapped) {
      src = cmapped;
    }

    hb_buffer_add_utf32(buf, src, src_len, 0, src_len);

#if 0
    hb_buffer_guess_segment_properties(buf);
#else
    {
      u_int32_t code;
      hb_script_t hbscript;

      hbscript = HB_TAG(script[0] & ~0x20, /* Upper case */
                        script[1] | 0x20,  /* Lower case */
                        script[2] | 0x20,  /* Lower case */
                        script[3] | 0x20); /* Lower case */

      for (count = 0; count < src_len; count++) {
        code = src[count];

        if (0x900 <= code && code <= 0xd7f) {
          if (code <= 0x97f) {
            hbscript = HB_SCRIPT_DEVANAGARI;
          } else if (/* 0x980 <= code && */ code <= 0x9ff) {
            hbscript = HB_SCRIPT_BENGALI;
          } else if (/* 0xa00 <= code && */ code <= 0xa7f) {
            /* PUNJABI */
            hbscript = HB_SCRIPT_GURMUKHI;
          } else if (/* 0xa80 <= code && */ code <= 0xaff) {
            hbscript = HB_SCRIPT_GUJARATI;
          } else if (/* 0xb00 <= code && */ code <= 0xb7f) {
            hbscript = HB_SCRIPT_ORIYA;
          } else if (/* 0xb80 <= code && */ code <= 0xbff) {
            hbscript = HB_SCRIPT_TAMIL;
          } else if (/* 0xc00 <= code && */ code <= 0xc7f) {
            hbscript = HB_SCRIPT_TELUGU;
          } else if (/* 0xc80 <= code && */ code <= 0xcff) {
            hbscript = HB_SCRIPT_KANNADA;
          } else if (0xd00 <= code) {
            hbscript = HB_SCRIPT_MALAYALAM;
          }
        }
      }

      hb_buffer_set_script(buf, hbscript);
      hb_buffer_set_direction(buf, hb_script_get_horizontal_direction(hbscript));
      hb_buffer_set_language(buf, hb_language_get_default());
    }
#endif

    hbfeatures = get_hb_features(features, &num);
    hb_shape(hbfont, buf, hbfeatures, num);

    info = hb_buffer_get_glyph_infos(buf, &num);
    pos = hb_buffer_get_glyph_positions(buf, &num);

    if (!cmapped) {
      int32_t prev_offset;

      prev_offset = 0;
      shaped[0] = info[0].codepoint;
      if (offsets && widths) {
        offsets[0] = widths[0] = 0;
      }

      for (count = 1; count < num; count++) {
        shaped[count] = info[count].codepoint;

#if 0
        if (!offsets || !widths) {
          /* do nothing */
        } else
#endif
            if (abs(pos[count].x_offset) >= 64) {
          int32_t offset;

          prev_offset = offset = pos[count].x_offset + pos[count - 1].x_advance + prev_offset;

          if (offset >= 0) {
            offset = ((offset >> 6) & 0x7f);
          } else {
            offset = ((offset >> 6) | 0x80);
          }

          offsets[count] = offset;
          widths[count] = ((pos[count].x_advance >> 6) & 0xff);

          if (offsets[count] == 0 && widths[count] == 0) {
            offsets[count] = -1; /* XXX */
          }
        } else {
          offsets[count] = widths[count] = 0;
          prev_offset = 0;
        }
      }
    } else {
      u_int minus;

      minus = 0;
      for (count = 1; count < num; count++) {
        if (abs(pos[count].x_offset) >= 64) {
          minus++;
        }
      }

      num -= minus;
    }

#if 0
    hb_buffer_destroy(buf);
#endif

    return num;
  }
}
