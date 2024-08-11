/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef COCOA_TOUCH
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "../ui_font.h"

#include <stdio.h>
#include <string.h> /* memset/strncasecmp */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_str.h> /* bl_str_to_uint */
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */
#include <mef/ef_ucs4_map.h>
#include <mef/ef_ucs_property.h>

#ifdef USE_OT_LAYOUT
#include <otl.h>
#endif

#ifdef COCOA_TOUCH
#define DEFAULT_FONT "Courier"
#else
#define DEFAULT_FONT "Menlo"
#endif

#include "cocoa.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static int use_point_size;

/* --- static functions --- */

static int parse_font_name(
    char **font_family,
    double *font_size, /* if size is not specified in font_name, not changed. */
    int *is_bold,      /* if bold is not specified in font_name, not changed. */
    int *is_italic,    /* if italic is not specified in font_name, not changed. */
    u_int *percent,    /* if percent is not specified in font_name , not changed. */
    char *font_name    /* modified by this function. */
    ) {
  char *p;
  size_t len;

  /*
   * Format.
   * [Family]( [WEIGHT] [SLANT] [SIZE]:[Percentage])
   */

  *font_family = font_name;

  if ((p = strrchr(font_name, ':'))) {
    /* Parsing ":[Percentage]" */
    if (bl_str_to_uint(percent, p + 1)) {
      *p = '\0';
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " Percentage(%s) is illegal.\n", p + 1);
    }
#endif
  }

/*
 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
 * Following is the same as ui_font.c:parse_xft_font_name()
 * except FW_* and is_italic.
 */

#if 0
  bl_debug_printf("Parsing %s for [Family] [Weight] [Slant]\n", *font_family);
#endif

  p = bl_str_chop_spaces(*font_family);
  len = strlen(p);
  while (len > 0) {
    size_t step = 0;

    if (*p == ' ') {
      char *orig_p;

      orig_p = p;
      do {
        p++;
        len--;
      } while (*p == ' ');

      if (len == 0) {
        *orig_p = '\0';

        break;
      } else {
        int count;
        /*
         * XXX
         * "medium" is necessary because GTK font selection dialog (mlconfig)
         * returns "Osaka medium"
         */
        char *styles[] = { "italic",  "bold", "medium", "oblique", "light", "semi-bold", "heavy",
                           "semi-condensed", };

        for (count = 0; count < BL_ARRAY_SIZE(styles); count++) {
          size_t len_v;

          len_v = strlen(styles[count]);

          /* XXX strncasecmp is not portable? */
          if (len >= len_v && strncasecmp(p, styles[count], len_v) == 0) {
            /* [WEIGHT] [SLANT] */
            *orig_p = '\0';
            step = len_v;

            if (count <= 1) {
              if (count == 0) {
                *is_italic = 1;
              } else {
                *is_bold = 1;
              }
            }

            goto next_char;
          }
        }

        if (*p != '0' ||      /* In case of "DevLys 010" font family. */
            *(p + 1) == '\0') /* "MS Gothic 0" => "MS Gothic" + "0" */
        {
          char *end;
          double size;

          size = strtod(p, &end);
          if (*end == '\0') {
            /* p has no more parameters. */

            *orig_p = '\0';
            if (size > 0) {
              *font_size = size;
            }

            break;
          }
        }

        step = 1;
      }
    } else {
      step = 1;
    }

  next_char:
    p += step;
    len -= step;
  }

  return 1;
}

/* --- global functions --- */

/* Undocumented */
bool CGFontGetGlyphsForUnichars(CGFontRef, u_int16_t[], CGGlyph[], size_t);

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold, int letter_space) {
  ui_font_t *font;
  ef_charset_t cs;
  char *font_family;
  u_int percent;
  u_int cols;

  cs = FONT_CS(id);
  if (cs == DEC_SPECIAL && fontname == NULL) {
    return NULL;
  }

  if (type_engine != TYPE_XCORE ||
      (font = calloc(1, sizeof(ui_font_t) + sizeof(XFontStruct))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return NULL;
  }

  font->xfont = (XFontStruct*)(font + 1);

  font->display = display;
  font->id = id;

  if (font->id & FONT_FULLWIDTH) {
    cols = 2;
  } else {
    cols = 1;
  }

  if (IS_ISCII(cs) || cs == ISO10646_UCS4_1_V) {
    /*
     * For exampe, 'W' width and 'l' width of OR-TTSarala font for ISCII_ORIYA
     * are the same by chance, though it is actually a proportional font.
     */
    font->is_var_col_width = font->is_proportional = 1;
  } else if (font_present & FONT_VAR_WIDTH) {
    font->is_var_col_width = 1;
  }

  if (font_present & FONT_VERTICAL) {
    font->is_vertical = 1;
  }

  font_family = NULL;
  percent = 0;

  if (fontname) {
    char *p;

    if ((p = alloca(strlen(fontname) + 1)) == NULL) {
      free(font);

      return NULL;
    }
    strcpy(p, fontname);

    double fontsize_d = 0;
    int is_bold = 0;
    int is_italic = 0;

    parse_font_name(&font_family, &fontsize_d, &is_bold, &is_italic, &percent, p);

    if (fontsize_d > 0.0) {
      fontsize = fontsize_d;
    }

    if (is_bold) {
      font->double_draw_gap = 1;
    }

    if (is_italic) {
      font->xfont->is_italic = 1;
    }
  } else {
    /* Default font */
    font_family = DEFAULT_FONT;
  }

  if (font->id & FONT_BOLD) {
    font->double_draw_gap = 1;
  }

  if (font->id & FONT_ITALIC) {
    font->xfont->is_italic = 1;
  }

  while (!(font->xfont->cg_font = cocoa_create_font(font_family))) {
    bl_warn_printf("%s font is not found.\n", font_family);

    if (col_width == 0 && strcmp(font_family, DEFAULT_FONT) != 0) {
      /*
       * standard(usascii) font
       * Fall back to default font.
       */
      font_family = DEFAULT_FONT;
    } else {
      free(font);

      return NULL;
    }
  }

#if 0
  bl_debug_printf("%d %d %d %d %d %d\n", CGFontGetAscent(font->xfont->cg_font),
                  CGFontGetDescent(font->xfont->cg_font), CGFontGetLeading(font->xfont->cg_font),
                  CGFontGetCapHeight(font->xfont->cg_font), CGFontGetXHeight(font->xfont->cg_font),
                  CGFontGetUnitsPerEm(font->xfont->cg_font));
#endif

  font->xfont->size = fontsize;

  int ascent = CGFontGetAscent(font->xfont->cg_font);
  int descent = CGFontGetDescent(font->xfont->cg_font); /* minus value */
  int units = CGFontGetUnitsPerEm(font->xfont->cg_font);

  font->height = ((float)fontsize * (ascent - descent)) / ((float)units) + 0.5;
  font->ascent = (font->height * ascent) / (ascent - descent);

  CGGlyph glyphs[1];
  u_int16_t ch = 'M';
  int advance;
  CGFontGetGlyphsForUnichars(font->xfont->cg_font, &ch, glyphs, 1);
  CGFontGetGlyphAdvances(font->xfont->cg_font, glyphs, 1, &advance);
  fontsize = ((float)fontsize * advance * 2) / ((float)units) + 0.5;

  /*
   * Following processing is similar to ui_font.c:set_xfont()
   */

  font->x_off = 0;

  if (col_width == 0) {
    /* standard(usascii) font */

    if (percent > 0) {
      if (font->is_vertical) {
        /* The width of full and half character font is the same. */
        letter_space *= 2;
        font->width = font->height * percent / 100;
        if (font->height / 2 < font->width) {
          font->x_off += ((font->width - font->height / 2) / 2);
        }
      } else {
        font->width = font->height * percent / 200;
      }
    } else if (font->is_vertical) {
      /* The width of full and half character font is the same. */
      letter_space *= 2;
      font->width = fontsize;
      font->x_off += (fontsize / 4);
    } else {
      font->width = fontsize * cols / 2;
    }

    if (letter_space > 0 || font->width > -letter_space) {
      font->width += letter_space;

      if (letter_space > 0 || font->x_off * 2 > -letter_space) {
        font->x_off += letter_space / 2;
      }
    }
  } else {
    /* not a standard(usascii) font */

    if (font->is_vertical) {
      /* The width of full and half character font is the same. */
      font->width = col_width;
      if (letter_space > 0) {
        font->x_off = letter_space;
      }
    } else {
      font->width = col_width * cols;
      if (letter_space > 0) {
        font->x_off = letter_space * cols / 2;
      }
    }
  }

  if (size_attr) {
    font->width *= 2;
    font->x_off *= 2;

    if (size_attr >= DOUBLE_HEIGHT_TOP) {
      font->height *= 2;
      font->ascent *= 2;
    }
  }

  font->size_attr = size_attr;

#ifdef DEBUG
  ui_font_dump(font);
#endif

  return font;
}

ui_font_t *ui_font_new_for_decsp(Display *display, vt_font_t id, u_int width, u_int height,
                                 u_int ascent) {
  return NULL;
}

void ui_font_destroy(ui_font_t *font) {
#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  cocoa_release_font(font->xfont->cg_font);

  free(font);
}

#ifdef USE_OT_LAYOUT
int ui_font_has_ot_layout_table(ui_font_t *font) {
  if (!font->ot_font) {
#ifdef USE_HARFBUZZ
    if (font->ot_font_not_found) {
      return 0;
    }

    font->ot_font = otl_open(font->xfont->cg_font);
#else
    char *path;

    if (font->ot_font_not_found || !(path = cocoa_get_font_path(font->xfont->cg_font))) {
      return 0;
    }

    font->ot_font = otl_open(path);

    free(path);
#endif

    if (!font->ot_font) {
      font->ot_font_not_found = 1;

      return 0;
    }
  }

  return 1;
}

u_int ui_convert_text_to_glyphs(ui_font_t *font, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                u_int32_t *noshape_glyphs, u_int32_t *src, u_int src_len,
                                const char *script, const char *features) {
  return otl_convert_text_to_glyphs(font->ot_font, shape_glyphs, num_shape_glyphs,
                                    xoffsets, yoffsets, advances, noshape_glyphs,
                                    src, src_len, script, features,
                                    font->xfont->size * (font->size_attr >= DOUBLE_WIDTH ? 2 : 1));
}
#endif /* USE_OT_LAYOUT */

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int is_awidth,
                              int *draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

  if (font->is_proportional) {
#ifdef USE_OT_LAYOUT
    if (font->use_ot_layout /* && font->ot_font */) {
      return cocoa_font_get_advance(font->xfont->cg_font, font->xfont->size,
                                    font->size_attr, NULL, 0, ch);
    } else
#endif
    {
      u_int16_t utf16[2];
      u_int len;

      if ((len = ui_convert_ucs4_to_utf16((u_char*)utf16, ch) / 2) > 0) {
        return cocoa_font_get_advance(font->xfont->cg_font, font->xfont->size,
                                      font->size_attr, utf16, len, 0);
      } else {
        return 0;
      }
    }
  }

  return font->width;
}

void ui_font_use_point_size(int use) { use_point_size = use; }

/* Return written size */
size_t ui_convert_ucs4_to_utf16(u_char *dst, /* 4 bytes. Little endian. */
                                u_int32_t src) {
  if (src < 0x10000) {
    dst[1] = (src >> 8) & 0xff;
    dst[0] = src & 0xff;

    return 2;
  } else if (src < 0x110000) {
    /* surrogate pair */

    u_char c;

    src -= 0x10000;
    c = (u_char)(src / (0x100 * 0x400));
    src -= (c * 0x100 * 0x400);
    dst[1] = c + 0xd8;

    c = (u_char)(src / 0x400);
    src -= (c * 0x400);
    dst[0] = c;

    c = (u_char)(src / 0x100);
    src -= (c * 0x100);
    dst[3] = c + 0xdc;
    dst[2] = (u_char)src;

    return 4;
  }

  return 0;
}

#ifdef DEBUG

void ui_font_dump(ui_font_t *font) {
  bl_msg_printf("  id %x: Font %p", font->id, font->xfont->cg_font);

  if (font->is_proportional) {
    bl_msg_printf(" (proportional)");
  }
  bl_msg_printf("\n");
}

#endif
