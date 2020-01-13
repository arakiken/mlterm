/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <otf.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#ifdef USE_WIN32GUI
#include <windows.h>
#endif

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void *otl_open(void *obj) {
  OTF* otf;

#if defined(USE_WIN32GUI)

  FT_Library ftlib;
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

  if (FT_Init_FreeType(&ftlib) != 0) {
    free(font_data);

    return NULL;
  }

  if (FT_New_Memory_Face(ftlib, font_data, size, 0, &face) != 0) {
    otf = NULL;
  } else {
    if ((otf = OTF_open_ft_face(face))) {
      if (OTF_get_table(otf, "GSUB") != 0 || OTF_get_table(otf, "cmap") != 0) {
        OTF_close(otf);
        otf = NULL;
      }
    }

    FT_Done_Face(face);
  }

  free(font_data);
  FT_Done_FreeType(ftlib);

#else

#if defined(USE_QUARTZ) || defined(USE_BEOS)
  if ((otf = OTF_open(obj)))
#else
  if ((otf = OTF_open_ft_face(obj)))
#endif
  {
    if (OTF_check_table(otf, "GSUB") != 0 || OTF_check_table(otf, "cmap") != 0) {
      OTF_close(otf);
      otf = NULL;
    }
  }

#endif

  return otf;
}

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void otl_close(void *otf) {
  OTF_close(otf);
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
u_int otl_convert_text_to_glyphs(void *otf, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                 int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                 u_int32_t *noshape_glyphs, u_int32_t *src, u_int src_len,
                                 const char *script, const char *features, u_int fontsize) {
  static OTF_Glyph *glyphs;
  OTF_GlyphString otfstr;
  u_int count;

  otfstr.size = otfstr.used = src_len;

/* Avoid bl_mem memory management */
#undef realloc
  if (!(otfstr.glyphs = realloc(glyphs, otfstr.size * sizeof(*otfstr.glyphs)))) {
    return 0;
  }
#ifdef BL_DEBUG
#define realloc(ptr, size) bl_mem_realloc(ptr, size, __FILE__, __LINE__, __FUNCTION__)
#endif

  glyphs = otfstr.glyphs;
  memset(otfstr.glyphs, 0, otfstr.size * sizeof(*otfstr.glyphs));

  if (shape_glyphs == NULL || noshape_glyphs == NULL) {
    for (count = 0; count < src_len; count++) {
      otfstr.glyphs[count].c = src[count];
    }

    OTF_drive_cmap(otf, &otfstr);

    if (shape_glyphs == NULL) {
      for (count = 0; count < otfstr.used; count++) {
        noshape_glyphs[count] = otfstr.glyphs[count].glyph_id;
      }

      return otfstr.used;
    }
  } else {
    for (count = 0; count < src_len; count++) {
      otfstr.glyphs[count].glyph_id = noshape_glyphs[count];
    }
  }

  OTF_drive_gdef(otf, &otfstr);
  OTF_drive_gsub(otf, &otfstr, script, NULL, features);

  if (otfstr.used > num_shape_glyphs) {
    otfstr.used = num_shape_glyphs;
  }

  if (xoffsets /* && yoffsets && advances */) {
#if 0
    OTF_drive_gpos2(otf, &otfstr, script, NULL, features);

    for (count = 0; count < otfstr.used; count++) {
      bl_debug_printf("glyph %x: pos type %d ", src[count], otfstr.glyphs[count].glyph_id,
                      otfstr.glyphs[count].positioning_type);
      switch(otfstr.glyphs[count].positioning_type) {
      case 0:
        bl_msg_printf("from %d to %d", otfstr.glyphs[count].f.index.from,
                      otfstr.glyphs[count].f.index.to);
        break;
      case 1:
      case 2:
        bl_msg_printf("x %d y %d a %d", otfstr.glyphs[count].f.f1.value->XPlacement,
                      otfstr.glyphs[count].f.f1.value->YPlacement,
                      otfstr.glyphs[count].f.f1.value->XAdvance);
        break;
      }
      bl_msg_printf("\n");
    }
#endif

    memset(xoffsets, 0, num_shape_glyphs * sizeof(*xoffsets));
    memset(yoffsets, 0, num_shape_glyphs * sizeof(*yoffsets));
    /* 0xff: dummy code which gets OTL_IS_COMB() to be false */
    memset(advances, 0xff, num_shape_glyphs * sizeof(*advances));
  }

  for (count = 0; count < otfstr.used; count++) {
    shape_glyphs[count] = otfstr.glyphs[count].glyph_id;
  }

  return otfstr.used;
}
