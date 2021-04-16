/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_font.h"

#include <stdio.h>
#include <string.h> /* memset/strncasecmp */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_str.h> /* bl_str_to_int */
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

#ifdef USE_OT_LAYOUT
#include <otl.h>
#endif

#define DEFAULT_FONT "Noto Mono"

#include "beos.h"

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

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold, int letter_space) {
  ui_font_t *font;
  ef_charset_t cs;
  char *font_family;
  u_int percent;
  u_int cols;
  int is_italic = 0;
  int is_bold = 0;

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

  font->xfont = font + 1;

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
    double fontsize_d = 0;

    if ((p = alloca(strlen(fontname) + 1)) == NULL) {
      free(font);

      return NULL;
    }

    parse_font_name(&font_family, &fontsize_d, &is_bold, &is_italic, &percent,
                    strcpy(p, fontname));

    if (fontsize_d > 0.0) {
      fontsize = fontsize_d;
    }
  } else {
    /* Default font */
    font_family = DEFAULT_FONT;
  }

  if (font->id & FONT_BOLD) {
    is_bold = 1;
  }

  if (font->id & FONT_ITALIC) {
    is_italic = 1;
  }

  if (size_attr >= DOUBLE_HEIGHT_TOP) {
    fontsize *= 2;
    col_width *= 2;
  }

load_bfont:
  while (!(font->xfont->fid = beos_create_font(font_family, fontsize, is_italic, is_bold))) {
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

  font->xfont->size = fontsize;

  {
    u_int width;
    u_int height;
    u_int ascent;

    beos_font_get_metrics(font->xfont->fid, &width, &height, &ascent);
    font->width = width;
    font->height = height;
    font->ascent = ascent;
  }

  /*
   * Following processing is same as ui_font.c:set_xfont()
   */

  font->x_off = 0;

  if (col_width == 0) {
    /* standard(usascii) font */

    if (!font->is_var_col_width) {
      u_int ch_width;

      if (percent > 0) {
        if (font->is_vertical) {
          /* The width of full and half character font is the same. */
          letter_space *= 2;
          ch_width = font->width * percent / 50;
        } else {
          ch_width = font->width * percent / 100;
        }
      } else {
        ch_width = font->width;

        if (font->is_vertical) {
          /* The width of full and half character font is the same. */
          letter_space *= 2;
          ch_width *= 2;
        }
      }

      if (letter_space > 0 || ch_width > -letter_space) {
        ch_width += letter_space;
      }

      if (ch_width != font->width) {
        font->is_proportional = 1;

        if (font->width < ch_width) {
          font->x_off = (ch_width - font->width) / 2;
        }

        font->width = ch_width;
      }
    }
  } else {
    /* not a standard(usascii) font */

    font->width *= cols;

    if (font->is_vertical) {
      /*
       * The width of full and half character font is the same.
       * is_var_col_width is always false if is_vertical is true.
       */
      if (font->width != col_width) {
        bl_msg_printf("Font(id %x) width(%d) doesn't fit column width(%d).\n",
                      font->id, font->width, col_width);

        font->is_proportional = 1;

        if (font->width < col_width) {
          font->x_off = (col_width - font->width) / 2;
        }

        font->width = col_width;
      }
    } else if (font->width != col_width * cols) {
      if (font->width != (col_width - letter_space /* XXX */) * cols &&
          is_bold && font->double_draw_gap == 0) {
        /* Retry */
        is_bold = 0;
        font->double_draw_gap = 1;
        beos_release_font(font->xfont->fid);

        bl_msg_printf("Use double drawing to show bold glyphs (font id %x).\n", font->id);

        goto load_bfont;
      }

      bl_msg_printf("Font(id %x) width(%d) doesn't fit column width(%d).\n",
                    font->id, font->width, col_width * cols);

      font->is_proportional = 1;

      if (!font->is_var_col_width) {
        if (font->width < col_width * cols) {
          font->x_off = (col_width * cols - font->width) / 2;
        }

        font->width = col_width * cols;
      }
    }
  }

  if (size_attr == DOUBLE_WIDTH) {
    font->width *= 2;
    font->x_off += (font->width / 4);
    font->is_proportional = 1;
    font->is_var_col_width = 0;

    font->size_attr = size_attr;
  }

  if (font->is_proportional && !font->is_var_col_width) {
    bl_msg_printf("Font(id %x): Draw one char at a time to fit column width.\n", font->id);
  }

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

  beos_release_font(font->xfont->fid);

  free(font);
}

#ifdef USE_OT_LAYOUT
int ui_font_has_ot_layout_table(ui_font_t *font) {
#if 0
  if (!font->ot_font) {
    char *family;

    if (font->ot_font_not_found) {
      return 0;
    }

    family = beos_get_font_family(font->xfont->fid);
    font->ot_font = otl_open(family);
    free(family);

    if (!font->ot_font) {
      font->ot_font_not_found = 1;

      return 0;
    }
  }

  return 1;
#else
  if (!font->ot_font_not_found) {
    bl_msg_printf("Open Type Layout is not supported on HaikuOS which doesn't "
                  "provide API to show characters by glyph indeces for now.\n");
    font->ot_font_not_found = 1;
  }

  return 0;
#endif
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
    if (font->is_var_col_width) {
#ifdef USE_OT_LAYOUT
      if (font->use_ot_layout /* && font->ot_font */) {
        return beos_font_get_advance(font->xfont->fid,
                                     font->size_attr, NULL, 0, ch);
      } else
#endif
      {
        u_int16_t utf16[2];
        u_int len;

        if ((len = ui_convert_ucs4_to_utf16(utf16, ch) / 2) > 0) {
          return beos_font_get_advance(font->xfont->fid,
                                       font->size_attr, utf16, len, 0);
        } else {
          return 0;
        }
      }
    }

    if (draw_alone) {
      *draw_alone = 1;
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
  bl_msg_printf("id %x: Font %p width %d height %d ascent %d",
                font->id, font->xfont->fid, font->width, font->height, font->ascent);

  if (font->is_proportional) {
    bl_msg_printf(" (proportional)");
  }
  bl_msg_printf("\n");
}

#endif
