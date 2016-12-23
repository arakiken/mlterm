/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_font.h"

#include <stdlib.h> /* strtod */
#ifdef USE_TYPE_XFT
#include <X11/Xft/Xft.h>
#endif
#ifdef USE_TYPE_CAIRO
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h> /* FcChar32 */
#include <cairo/cairo-xlib.h>
#endif
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* realloc */
#include <pobl/bl_str.h> /* bl_str_sep/bl_str_to_int/memset/strncasecmp */
#include <vt_char.h>     /* UTF_MAX_SIZE */

#ifdef USE_OT_LAYOUT
#include <otl.h>
#endif

#define DIVIDE_ROUNDING(a, b) (((int)((a)*10 + (b)*5)) / ((int)((b)*10)))
#define DIVIDE_ROUNDINGUP(a, b) (((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)))

/* Be careful not to round down 5.99999... to 5 */
#define DOUBLE_ROUNDUP_TO_INT(a) ((int)((a) + 0.9))

/*
 * XXX
 * cairo always uses double drawing fow now, because width of normal font is not
 * always the same as that of bold font in cairo.
 */
#if 1
#define CAIRO_FORCE_DOUBLE_DRAWING
#endif

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static const char* fc_size_type = FC_PIXEL_SIZE;
static double dpi_for_fc;

/* --- static functions --- */

static int parse_fc_font_name(
    char** font_family, int* font_weight, /* if weight is not specified in
                                             font_name , not changed. */
    int* font_slant,      /* if slant is not specified in font_name , not changed. */
    double* font_size,    /* if size is not specified in font_name , not changed. */
    char** font_encoding, /* if encoding is not specified in font_name , not
                             changed. */
    u_int* percent,       /* if percent is not specified in font_name , not changed. */
    char* font_name       /* modified by this function. */
    ) {
  char* p;
  size_t len;

#if 1
  /*
   * Compat with mlterm 3.6.3 or before: ... [SIZE]-[Encoding]:[Percentage]
   *                                               ^^^^^^^^^^^
   */
  if ((p = strstr(font_name, "-iso10646-1"))) {
    memmove(p, p + 11, strlen(p + 11) + 1);
  }
#endif

  /*
   * XftFont format.
   * [Family]( [WEIGHT] [SLANT] [SIZE]:[Percentage])
   */

  *font_family = font_name;

  p = font_name;
  while (1) {
    if (*p == '\\' && *(p + 1)) {
      /* Compat with 3.6.3 or before. (e.g. Foo\-Bold-iso10646-1) */

      /* skip backslash */
      p++;
    } else if (*p == '\0') {
      /* encoding and percentage is not specified. */

      *font_name = '\0';

      break;
    } else if (*p == ':') {
      /* Parsing ":[Percentage]" */

      *font_name = '\0';

      if (!bl_str_to_uint(percent, p + 1)) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " Percentage(%s) is illegal.\n", p + 1);
#endif
      }

      break;
    }

    *(font_name++) = *(p++);
  }

/*
 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
 * Following is the same as ui_font_win32.c:parse_font_name()
 * except FC_*.
 */

#if 0
  bl_debug_printf("Parsing %s for [Family] [Weight] [Slant]\n", *font_family);
#endif

  p = bl_str_chop_spaces(*font_family);
  len = strlen(p);
  while (len > 0) {
    size_t step = 0;

    if (*p == ' ') {
      char* orig_p;

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
        struct {
          char* style;
          int weight;
          int slant;

        } styles[] = {
            /*
             * Portable styles.
             */
            /* slant */
            {
             "italic", 0, FC_SLANT_ITALIC,
            },
            /* weight */
            {
             "bold", FC_WEIGHT_BOLD, 0,
            },

            /*
             * Hack for styles which can be returned by
             * gtk_font_selection_dialog_get_font_name().
             */
            /* slant */
            {
             "oblique", 0, FC_SLANT_OBLIQUE,
            },
            /* weight */
            {
             "light", /* e.g. "Bookman Old Style Light" */
             FC_WEIGHT_LIGHT, 0,
            },
            {
             "semi-bold", FC_WEIGHT_DEMIBOLD, 0,
            },
            {
             "heavy", /* e.g. "Arial Black Heavy" */
             FC_WEIGHT_BLACK, 0,
            },
            /* other */
            {
             "semi-condensed", /* XXX This style is ignored. */
             0, 0,
            },
        };

        for (count = 0; count < sizeof(styles) / sizeof(styles[0]); count++) {
          size_t len_v;

          len_v = strlen(styles[count].style);

          /* XXX strncasecmp is not portable? */
          if (len >= len_v && strncasecmp(p, styles[count].style, len_v) == 0) {
            *orig_p = '\0';
            step = len_v;
            if (styles[count].weight) {
              *font_weight = styles[count].weight;
            } else if (styles[count].slant) {
              *font_slant = styles[count].slant;
            }

            goto next_char;
          }
        }

        if (*p != '0' ||      /* In case of "DevLys 010" font family. */
            *(p + 1) == '\0') /* "MS Gothic 0" => "MS Gothic" + "0" */
        {
          char* end;
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

static u_int get_fc_col_width(ui_font_t* font, double fontsize_d, u_int percent,
                              u_int letter_space) {
  if (percent == 0) {
    if (letter_space == 0 || font->is_var_col_width) {
      return 0;
    }

    percent = 100;
  }

  if (strcmp(fc_size_type, FC_SIZE) == 0) {
    double dpi;

    if (dpi_for_fc) {
      dpi = dpi_for_fc;
    } else {
      double widthpix;
      double widthmm;

      widthpix = DisplayWidth(font->display, DefaultScreen(font->display));
      widthmm = DisplayWidthMM(font->display, DefaultScreen(font->display));

      dpi = (widthpix * 254) / (widthmm * 10);
    }

    return DIVIDE_ROUNDINGUP(dpi * fontsize_d * font->cols * percent, 72 * 100 * 2) + letter_space;
  } else {
    return DIVIDE_ROUNDINGUP(fontsize_d * font->cols * percent, 100 * 2) + letter_space;
  }
}

static FcPattern* fc_pattern_create(char* family,                        /* can be NULL */
                                    double size, char* encoding,         /* can be NULL */
                                    int weight, int slant, int ch_width, /* can be 0 */
                                    int aa_opt) {
  FcPattern* pattern;

  if (!(pattern = FcPatternCreate())) {
    return NULL;
  }

  if (family) {
    FcPatternAddString(pattern, FC_FAMILY, family);
  }
  FcPatternAddDouble(pattern, fc_size_type, size);
  if (weight >= 0) {
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
  }
  if (slant >= 0) {
    FcPatternAddInteger(pattern, FC_SLANT, slant);
  }
#ifdef USE_TYPE_XFT
  if (ch_width > 0) {
    FcPatternAddInteger(pattern, FC_SPACING, FC_CHARCELL);
    /* XXX FC_CHAR_WIDTH doesn't make effect in cairo ... */
    FcPatternAddInteger(pattern, FC_CHAR_WIDTH, ch_width);
  }
#endif

  if (aa_opt) {
    FcPatternAddBool(pattern, FC_ANTIALIAS, aa_opt == 1 ? True : False);
  }
  if (dpi_for_fc) {
    FcPatternAddDouble(pattern, FC_DPI, dpi_for_fc);
  }
#ifdef USE_TYPE_XFT
  if (encoding) {
    /* no meaning on xft2 */
    FcPatternAddString(pattern, XFT_ENCODING, encoding);
  }
#endif
#if 0
  FcPatternAddBool(pattern, "embeddedbitmap", True);
#endif

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);

  return pattern;
}

#ifdef USE_TYPE_XFT

static XftFont* xft_font_open(ui_font_t* font, char* family, /* can be NULL */
                              double size, char* encoding,   /* can be NULL */
                              int weight, int slant, int ch_width, int aa_opt) {
  FcPattern* pattern;
  FcPattern* match;
  FcResult result;
  XftFont* xfont;

  if (!(pattern = fc_pattern_create(family, size, encoding, weight, slant, ch_width, aa_opt))) {
    return NULL;
  }

  if (IS_ISCII(FONT_CS(font->id))) {
    /* no meaning on xft2 */
    FcPatternAddString(pattern, XFT_ENCODING, "apple-roman");
  }

  match = XftFontMatch(font->display, DefaultScreen(font->display), pattern, &result);
  FcPatternDestroy(pattern);
  if (!match) {
    return NULL;
  }

#if 0
  FcPatternPrint(match);
#endif

  if (!(xfont = XftFontOpenPattern(font->display, match))) {
    FcPatternDestroy(match);

    return NULL;
  }

#if 1
  if (IS_ISCII(FONT_CS(font->id))) {
    FT_Face face;
    int count;

    face = XftLockFace(xfont);

    for (count = 0; count < face->num_charmaps; count++) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " ISCII font encoding %c%c%c%c\n",
                      ((face->charmaps[count]->encoding) >> 24) & 0xff,
                      ((face->charmaps[count]->encoding) >> 16) & 0xff,
                      ((face->charmaps[count]->encoding) >> 8) & 0xff,
                      (face->charmaps[count]->encoding & 0xff));
#endif

      if (face->charmaps[count]->encoding == FT_ENCODING_APPLE_ROMAN) {
        FT_Set_Charmap(face, face->charmaps[count]);
        break;
      }
    }

    XftUnlockFace(xfont);
  }
#endif

  return xfont;
}

#endif

#ifdef USE_TYPE_CAIRO

static int is_same_family(FcPattern* pattern, const char* family) {
  int count;
  FcValue val;

  for (count = 0; FcPatternGet(pattern, FC_FAMILY, count, &val) == FcResultMatch; count++) {
    if (strcmp(family, val.u.s) == 0) {
      return 1;
    }
  }

  return 0;
}

static cairo_scaled_font_t* cairo_font_open_intern(cairo_t* cairo, FcPattern* match,
                                                   cairo_font_options_t* options) {
  cairo_font_face_t* font_face;
  double pixel_size;
  int pixel_size2;
  cairo_matrix_t font_matrix;
  cairo_matrix_t ctm;
  cairo_scaled_font_t* scaled_font;

  font_face = cairo_ft_font_face_create_for_pattern(match);

  FcPatternGetDouble(match, FC_PIXEL_SIZE, 0, &pixel_size);
  /*
   * 10.5 / 2.0 = 5.25 ->(roundup) 6 -> 6 * 2 = 12
   * 11.5 / 2.0 = 5.75 ->(roundup) 6 -> 6 * 2 = 12
   *
   * If half width is 5.25 -> 6 and full width is 5.25 * 2 = 10.5 -> 11,
   * half width char -> ui_bearing = 1 / width 5
   * full width char -> ui_bearing = 1 / width 10.
   * This results in gap between chars.
   */
  pixel_size2 = DIVIDE_ROUNDINGUP(pixel_size, 2.0) * 2;

  cairo_matrix_init_scale(&font_matrix, pixel_size2, pixel_size2);
  cairo_get_matrix(cairo, &ctm);

  scaled_font = cairo_scaled_font_create(font_face, &font_matrix, &ctm, options);

  cairo_destroy(cairo);
  cairo_font_options_destroy(options);
  cairo_font_face_destroy(font_face);

  return scaled_font;
}

static cairo_scaled_font_t* cairo_font_open(ui_font_t* font, char* family, /* can be NULL */
                                            double size, char* encoding,   /* can be NULL */
                                            int weight, int slant, int ch_width, int aa_opt) {
  cairo_font_options_t* options;
  cairo_t* cairo;
  FcPattern* pattern;
  FcPattern* match;
  FcResult result;
  cairo_scaled_font_t* xfont;
  FcCharSet* charset;
  ef_charset_t cs;

  if (!(pattern = fc_pattern_create(family, size, encoding, weight, slant, ch_width, aa_opt))) {
    return NULL;
  }

  if (!(cairo = cairo_create(cairo_xlib_surface_create(
            font->display, DefaultRootWindow(font->display),
            DefaultVisual(font->display, DefaultScreen(font->display)),
            DisplayWidth(font->display, DefaultScreen(font->display)),
            DisplayHeight(font->display, DefaultScreen(font->display)))))) {
    goto error1;
  }

  options = cairo_font_options_create();
  cairo_get_font_options(cairo, options);
#ifndef CAIRO_FORCE_DOUBLE_DRAWING
  /*
   * XXX
   * CAIRO_HINT_METRICS_OFF has bad effect, but CAIRO_HINT_METRICS_ON
   * disarranges
   * column width by boldening etc.
   */
  cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
#else
  /* For performance */
  cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
#endif
#if 0
  cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
  cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_RGB);
#endif
  cairo_ft_font_options_substitute(options, pattern);

  /* Supply default values for underspecified options */
  FcDefaultSubstitute(pattern);

  if (!(match = FcFontMatch(NULL, pattern, &result))) {
    cairo_destroy(cairo);
    cairo_font_options_destroy(options);

    goto error1;
  }

#if 0
  FcPatternPrint(match);
#endif

  if (!(xfont = cairo_font_open_intern(cairo, match, options))) {
    goto error2;
  }

  if (cairo_scaled_font_status(xfont)) {
    cairo_scaled_font_destroy(xfont);

    goto error2;
  }

  cs = FONT_CS(font->id);

#if 1
  if (IS_ISCII(cs)) {
    FT_Face face;
    int count;

    FcPatternDestroy(pattern);

    face = cairo_ft_scaled_font_lock_face(xfont);

    for (count = 0; count < face->num_charmaps; count++) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " ISCII font encoding %c%c%c%c\n",
                      ((face->charmaps[count]->encoding) >> 24) & 0xff,
                      ((face->charmaps[count]->encoding) >> 16) & 0xff,
                      ((face->charmaps[count]->encoding) >> 8) & 0xff,
                      (face->charmaps[count]->encoding & 0xff));
#endif

      if (face->charmaps[count]->encoding == FT_ENCODING_APPLE_ROMAN) {
        FT_Set_Charmap(face, face->charmaps[count]);
      }
    }

    cairo_ft_scaled_font_unlock_face(xfont);
  } else
#endif
      if (cs != US_ASCII && cs != ISO8859_1_R &&
          FcPatternGetCharSet(match, FC_CHARSET, 0, &charset) == FcResultMatch &&
          (font->compl_fonts = malloc(sizeof(*font->compl_fonts)))) {
    FcValue val;
    int count;

    font->compl_fonts[0].charset = FcCharSetCopy(charset);
    font->compl_fonts[0].next = NULL;

    count = 0;
    while (FcPatternGet(pattern, FC_FAMILY, count, &val) == FcResultMatch) {
      if (is_same_family(match, val.u.s)) {
        /* Remove not only matched name but also alias names */
        FcPatternRemove(pattern, FC_FAMILY, count);
      } else {
        int count2 = ++count;
        FcValue val2;
        while (FcPatternGet(pattern, FC_FAMILY, count2, &val2) == FcResultMatch) {
          if (strcmp(val.u.s, val2.u.s) == 0) {
            FcPatternRemove(pattern, FC_FAMILY, count2);
          } else {
            count2++;
          }
        }
      }
    }

    font->pattern = pattern;
  } else {
    FcPatternDestroy(pattern);
  }

  FcPatternDestroy(match);

  return xfont;

error2:
  FcPatternDestroy(match);

error1:
  FcPatternDestroy(pattern);

  return NULL;
}

#if 0
static void print_family(FcPattern* pattern) {
  int count;
  FcValue val;

  for (count = 0; FcPatternGet(pattern, FC_FAMILY, count, &val) == FcResultMatch; count++) {
    bl_debug_printf("Match(%d): %s\n", count, val.u.s);
  }
}
#endif

#define MAX_CHARSET_CACHE_SIZE 100

static struct {
  char *family;
  FcCharSet *charset;
} charset_cache[MAX_CHARSET_CACHE_SIZE];
static u_int charset_cache_size;

static int search_nearest_pos_in_cache(const char *family, int beg, int end) {
  int count = 0;
  while (1) {
    if (beg + 1 == end) {
      return beg;
    } else {
      int pos = (beg + end) / 2;
      switch (strcmp(family, charset_cache[pos].family)) {
      case 0:
        return pos;
      case 1:
        beg = pos;
        break;
      default: /* -1 */
        end = pos;
        break;
      }
    }
    count++;
  }
}

static FcCharSet *add_charset_to_cache(const char *family, FcCharSet *charset) {
  if (charset_cache_size < MAX_CHARSET_CACHE_SIZE) {
    int pos;
    if (charset_cache_size == 0) {
      pos = 0;
    } else {
      pos = search_nearest_pos_in_cache(family, 0, charset_cache_size);
      if (strcmp(family, charset_cache[pos].family) > 0) {
        pos++;
      }
    }

    memmove(charset_cache + pos + 1, charset_cache + pos,
            (charset_cache_size - pos) * sizeof(charset_cache[0]));

    charset_cache[pos].family = strdup(family);
    charset_cache[pos].charset = FcCharSetCopy(charset);
    charset_cache_size++;

    return charset_cache[pos].charset;
  } else {
    return NULL;
  }
}

static FcCharSet *get_cached_charset(const char *family) {
  if (charset_cache_size > 0) {
    int pos = search_nearest_pos_in_cache(family, 0, charset_cache_size);
    if (strcmp(family, charset_cache[pos].family) == 0) {
      return charset_cache[pos].charset;
    }
  }

  return NULL;
}

static int cairo_compl_font_open(ui_font_t* font, int num_of_compl_fonts, FcPattern* orig_pattern,
                                 int ch) {
  FcValue val;
  cairo_t* cairo;
  cairo_font_options_t* options;
  FcPattern* pattern;
  FcPattern* match = NULL;
  FcResult result;
  cairo_scaled_font_t* xfont;
  FcCharSet* charset;
  int count;
  int ret = 0;

  if (!(pattern = FcPatternDuplicate(orig_pattern))) {
    return 0;
  }

#if 0
  FcPatternPrint(orig_pattern);
#endif

  for (count = 0; FcPatternGet(pattern, FC_FAMILY, 0, &val) == FcResultMatch; count++) {
    void* p;

    if ((charset = get_cached_charset(val.u.s))) {
      if (!FcCharSetHasChar(charset, ch)) {
        FcPatternRemove(pattern, FC_FAMILY, 0);
        continue;
      }

      if (!(match = FcFontMatch(NULL, pattern, &result))) {
        break;
      }
      FcPatternRemove(pattern, FC_FAMILY, 0);
    } else {
      FcResult ret;

      if (!(match = FcFontMatch(NULL, pattern, &result))) {
        break;
      }

      ret = FcPatternGetCharSet(match, FC_CHARSET, 0, &charset);
      if (!(charset = add_charset_to_cache(val.u.s, charset))) {
        break;
      }
      FcPatternRemove(pattern, FC_FAMILY, 0);

      if (ret != FcResultMatch || !FcCharSetHasChar(charset, ch)) {
        FcPatternDestroy(match);
        continue;
      }
    }

#if 0
    print_family(match);
#endif

    FcPatternRemove(orig_pattern, FC_FAMILY, count++);

    while (FcPatternGet(orig_pattern, FC_FAMILY, count, &val) == FcResultMatch) {
      if (is_same_family(match, val.u.s)) {
        FcPatternRemove(orig_pattern, FC_FAMILY, count);
      } else {
        count++;
      }
    }

    if ((p = realloc(font->compl_fonts, sizeof(*font->compl_fonts) * (num_of_compl_fonts + 1)))) {
      font->compl_fonts = p;
    } else {
      FcPatternDestroy(match);

      break;
    }

#if 0
    FcPatternPrint(match);
#endif

    if (!(cairo = cairo_create(cairo_xlib_surface_create(
              font->display, DefaultRootWindow(font->display),
              DefaultVisual(font->display, DefaultScreen(font->display)),
              DisplayWidth(font->display, DefaultScreen(font->display)),
              DisplayHeight(font->display, DefaultScreen(font->display)))))) {
      break;
    }

    options = cairo_font_options_create();
    cairo_get_font_options(cairo, options);
    /* For performance */
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);

    if (!(xfont = cairo_font_open_intern(cairo, match, options))) {
      break;
    }

    if (cairo_scaled_font_status(xfont)) {
      cairo_scaled_font_destroy(xfont);

      break;
    }

    font->compl_fonts[num_of_compl_fonts - 1].next = xfont;
    font->compl_fonts[num_of_compl_fonts].charset = charset;
    font->compl_fonts[num_of_compl_fonts].next = NULL;

    ret = 1;

    break;
  }

  FcPatternDestroy(pattern);

  if (match) {
    FcPatternDestroy(match);
  }

  return ret;
}

#endif

static void* ft_font_open(ui_font_t* font, char* family, double size, char* encoding, int weight,
                          int slant, int ch_width, int aa_opt, int use_xft) {
  if (use_xft) {
#ifdef USE_TYPE_XFT
    return xft_font_open(font, family, size, encoding, weight, slant, ch_width, aa_opt);
#else
    return NULL;
#endif
  } else {
#ifdef USE_TYPE_CAIRO
    return cairo_font_open(font, family, size, encoding, weight, slant, ch_width, aa_opt);
#else
    return NULL;
#endif
  }
}

u_int xft_calculate_char_width(ui_font_t* font, u_int32_t ch);
u_int cairo_calculate_char_width(ui_font_t* font, u_int32_t ch);
int xft_unset_font(ui_font_t* font);

static int fc_set_font(ui_font_t* font, const char* fontname, u_int fontsize,
                       u_int col_width, /* if usascii font wants to be set , 0 will be set. */
                       u_int letter_space, int aa_opt, /* 0 = default , 1 = enable , -1 = disable */
                       int use_xft) {
  char* font_encoding;
  int weight;
  int slant;
  u_int ch_width;
  void* xfont;

  /*
   * encoding, weight and slant can be modified in parse_fc_font_name().
   */

  font_encoding = NULL;

  if (
#ifdef CAIRO_FORCE_DOUBLE_DRAWING
      use_xft &&
#endif
      (font->id & FONT_BOLD)) {
    weight = FC_WEIGHT_BOLD;
  } else {
    weight = -1; /* use default value */
  }

  if (font->id & FONT_ITALIC) {
#ifdef USE_TYPE_XFT
    /*
     * XXX
     * FC_CHAR_WIDTH=ch_width and FC_SPACING=FC_MONO make
     * the width of italic kochi font double of ch_width
     * on xft if slant >= 0. (They works fine for Dejavu
     * Sans Mono Italic, though.)
     */
    font->is_var_col_width = 1;
#endif

    slant = FC_SLANT_ITALIC;
  } else {
    slant = -1; /* use default value */
  }

  /*
   * x_off related to percent is set before ft_font_open while
   * x_off related to is_vertical and letter_space is set after.
   */

  if (fontname) {
    char* p;
    char* font_family;
    double fontsize_d;
    u_int percent;

    if ((p = bl_str_alloca_dup(fontname)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

      return 0;
    }

    fontsize_d = (double)fontsize;
    percent = 0;
    if (parse_fc_font_name(&font_family, &weight, &slant, &fontsize_d, &font_encoding, &percent,
                           p)) {
#ifdef USE_TYPE_XFT
      /*
       * XXX
       * FC_CHAR_WIDTH=ch_width and FC_SPACING=FC_MONO make
       * the width of italic kochi font double of ch_width
       * on xft if slant >= 0. (They works fine for Dejavu
       * Sans Mono Italic, though.)
       */
      if (slant >= 0) {
        font->is_var_col_width = 1;
      }
#endif

      if (col_width == 0) {
        /* basic font (e.g. usascii) width */

        /* if font->is_var_col_width is true, 0 is returned. */
        ch_width = get_fc_col_width(font, fontsize_d, percent, letter_space);

        if (percent > 100) {
          /*
           * Centering
           * (fontsize * percent / 100 + letter_space = ch_width
           *  -> fontsize = (ch_width - letter_space) * 100 / percent
           *  -> fontsize * (percent - 100) / 100
           *     = (ch_width - letter_space) * (percent - 100)
           *       / percent)
           */
          font->x_off = (ch_width - letter_space) * (percent - 100) / percent / 2;
        }

        if (font->is_vertical) {
          /*
           * !! Notice !!
           * The width of full and half character font is the same.
           */
          ch_width *= 2;
        }
      } else {
        if (font->is_var_col_width) {
          ch_width = 0;
        } else if (font->is_vertical) {
          /*
           * !! Notice !!
           * The width of full and half character font is the same.
           */
          ch_width = col_width;
        } else {
          ch_width = col_width * font->cols;
        }
      }

#ifdef DEBUG
      bl_debug_printf(
          "Loading font %s%s%s %f %d%s\n", font_family,
          weight == FC_WEIGHT_BOLD ? ":Bold" : weight == FC_WEIGHT_LIGHT ? " Light" : "",
          slant == FC_SLANT_ITALIC ? ":Italic" : "", fontsize_d, ch_width,
          font->is_var_col_width ? "(varcolwidth)" : "");
#endif

      if ((xfont = ft_font_open(font, font_family, fontsize_d, font_encoding, weight, slant,
                                ch_width, aa_opt, use_xft))) {
        goto font_found;
      }

      font->x_off = 0;
    }

    bl_msg_printf("Font %s (for size %f) couldn't be loaded.\n", fontname, fontsize_d);
  }

  if (col_width == 0) {
    /* basic font (e.g. usascii) width */

    ch_width = get_fc_col_width(font, (double)fontsize, 0, letter_space);

    if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */
      ch_width *= 2;
    }
  } else {
    if (font->is_var_col_width) {
      ch_width = 0;
    } else if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */
      ch_width = col_width;
    } else {
      ch_width = col_width * font->cols;
    }
  }

  if ((xfont = ft_font_open(font, NULL, (double)fontsize, font_encoding, weight, slant, ch_width,
                            aa_opt, use_xft))) {
    goto font_found;
  }

#ifdef DEBUG
  bl_warn_printf(BL_DEBUG_TAG " ft_font_open(%s) failed.\n", fontname);
#endif

  return 0;

font_found:

  if (use_xft) {
#ifdef USE_TYPE_XFT
#ifndef FC_EMBOLDEN /* Synthetic emboldening (fontconfig >= 2.3.0) */
    if (weight == FC_WEIGHT_BOLD &&
        XftPatternGetInteger(xfont->pattern, FC_WEIGHT, 0, &weight) == XftResultMatch &&
        weight != FC_WEIGHT_BOLD) {
      font->double_draw_gap = 1;
    }
#endif /* FC_EMBOLDEN */

    font->xft_font = xfont;

    font->height = font->xft_font->height;
    font->ascent = font->xft_font->ascent;

    if (ch_width == 0) {
      /*
       * (US_ASCII)
       *  font->is_var_col_width is true or letter_space == 0.
       *  (see get_fc_col_width())
       * (Other CS)
       *  font->is_var_col_width is true.
       */

      font->width = xft_calculate_char_width(font, 'W');

      if (font->width != xft_calculate_char_width(font, 'l') ||
          (col_width > 0 && font->width != col_width * font->cols)) {
        /* Regard it as proportional. */

        if (font->is_var_col_width) {
          font->is_proportional = 1;
        } else {
          u_int new_width;

          new_width = xft_calculate_char_width(font, 'M');
          if (font->is_vertical) {
            new_width *= 2;
          }

          xft_unset_font(font);

          /* reloading it as mono spacing. */
          return fc_set_font(font, fontname, fontsize, new_width, letter_space, aa_opt, use_xft);
        }
      }
    } else {
      /* Always mono space */
      font->width = ch_width;
    }

    font->x_off += (letter_space * font->cols / 2);

    if (font->is_vertical && font->cols == 1) {
      font->x_off += (font->width / 4); /* Centering */
    }
#endif /* USE_TYPE_XFT */
  } else {
#ifdef USE_TYPE_CAIRO
    cairo_font_extents_t extents;

#ifdef CAIRO_FORCE_DOUBLE_DRAWING
    if (font->id & FONT_BOLD) {
      font->double_draw_gap = 1;
    }
#endif

    font->cairo_font = xfont;

    cairo_scaled_font_extents(font->cairo_font, &extents);
    font->height = DOUBLE_ROUNDUP_TO_INT(extents.height);
    font->ascent = DOUBLE_ROUNDUP_TO_INT(extents.ascent);

    if (font->cols == 2) {
      font->width = DOUBLE_ROUNDUP_TO_INT(extents.max_x_advance);
    } else {
      font->width = cairo_calculate_char_width(font, 'W');

      if (font->is_vertical) {
        font->is_proportional = 1;
        font->width *= 2;
        font->x_off = font->width / 4; /* Centering */
      } else if (font->width != cairo_calculate_char_width(font, 'l')) {
        if (!font->is_var_col_width) {
#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
          font->width = cairo_calculate_char_width(font, 'N');
#else
          font->width = cairo_calculate_char_width(font, 'M');
#endif
        }

        /* Regard it as proportional. */
        font->is_proportional = 1;
      }
    }

    if (!font->is_var_col_width) {
      /*
       * Set letter_space here because cairo_font_open() ignores it.
       * (FC_CHAR_WIDTH doesn't make effect in cairo.)
       * Note that letter_space is ignored in variable column width mode.
       */
      if (letter_space > 0) {
        font->is_proportional = 1;

        if (font->is_vertical) {
          letter_space *= 2;
        } else {
          letter_space *= font->cols;
        }

        font->width += letter_space;
        font->x_off += (letter_space / 2); /* Centering */
      }

      if (ch_width > 0 && ch_width != font->width) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, ch_width);

        /*
         * XXX
         * Note that ch_width = 12 and extents.max_x_advance = 12.28
         * (dealt as 13 though should be dealt as 12) may happen.
         */

        font->is_proportional = 1;

        if (font->width < ch_width) {
          font->x_off += (ch_width - font->width) / 2;
        }

        font->width = ch_width;
      }

#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
      if (font->is_proportional && !font->is_var_col_width) {
        font->is_proportional = 0;
      }
#endif
    } else {
      if (col_width > 0 && font->width != col_width * font->cols) {
        font->is_proportional = 1;
      }
    }
#endif /* USE_TYPE_CAIRO */
  }

  /*
   * checking if font height/ascent member is sane.
   * font width must be always sane.
   */

  if (font->height == 0) {
    /* XXX this may be inaccurate. */
    font->height = fontsize;
  }

  if (font->ascent == 0) {
    /* XXX this may be inaccurate. */
    font->ascent = fontsize;
  }

  return 1;
}

/* --- global functions --- */

#ifdef USE_TYPE_XFT

int xft_set_font(ui_font_t* font, const char* fontname, u_int fontsize,
                 u_int col_width, /* if usascii font wants to be set , 0 will be set. */
                 u_int letter_space, int aa_opt, /* 0 = default , 1 = enable , -1 = disable */
                 int use_point_size, double dpi) {
  if (use_point_size) {
    fc_size_type = FC_SIZE;
  } else {
    fc_size_type = FC_PIXEL_SIZE;
  }

  dpi_for_fc = dpi;

  return fc_set_font(font, fontname, fontsize, col_width, letter_space, aa_opt, 1);
}

int xft_unset_font(ui_font_t* font) {
#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  XftFontClose(font->display, font->xft_font);
  font->xft_font = NULL;

  return 1;
}

int xft_set_ot_font(ui_font_t* font) {
#ifdef USE_OT_LAYOUT
  font->ot_font = otl_open(XftLockFace(font->xft_font), 0);
  XftUnlockFace(font->xft_font);

  return (font->ot_font != NULL);
#else
  return 0;
#endif
}

u_int xft_calculate_char_width(ui_font_t* font, u_int32_t ch /* US-ASCII or Unicode */
                               ) {
  XGlyphInfo extents;

#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->ot_font */) {
    if (sizeof(FT_UInt) != sizeof(u_int32_t)) {
      FT_UInt idx;

      idx = ch;

      XftGlyphExtents(font->display, font->xft_font, &idx, 1, &extents);
    } else {
      XftGlyphExtents(font->display, font->xft_font, &ch, 1, &extents);
    }
  } else
#endif
      if (ch < 0x100) {
    u_char c;

    c = ch;
    XftTextExtents8(font->display, font->xft_font, &c, 1, &extents);
  } else {
    XftTextExtents32(font->display, font->xft_font, &ch, 1, &extents);
  }

  if (extents.xOff < 0) {
    /* Some (indic) fonts could return minus value as text width. */
    return 0;
  } else {
    return extents.xOff;
  }
}

#endif

#ifdef USE_TYPE_CAIRO

int ui_search_next_cairo_font(ui_font_t* font, int ch) {
  int count;

  if (!font->compl_fonts) {
    return -1;
  }

  for (count = 0; font->compl_fonts[count].next; count++) {
    if (FcCharSetHasChar(font->compl_fonts[count + 1].charset, ch)) {
      return count;
    }
  }

  if (cairo_compl_font_open(font, count + 1, font->pattern, ch)) {
    return count;
  } else {
    /* To avoid to search it again. */
    FcCharSetAddChar(font->compl_fonts[0].charset, ch);

    return -1;
  }
}

int cairo_set_font(ui_font_t* font, const char* fontname, u_int fontsize,
                   u_int col_width, /* if usascii font wants to be set , 0 will be set. */
                   u_int letter_space, int aa_opt, /* 0 = default , 1 = enable , -1 = disable */
                   int use_point_size, double dpi) {
  if (use_point_size) {
    fc_size_type = FC_SIZE;
  } else {
    fc_size_type = FC_PIXEL_SIZE;
  }
  dpi_for_fc = dpi;

  return fc_set_font(font, fontname, fontsize, col_width, letter_space, aa_opt, 0);
}

int cairo_unset_font(ui_font_t* font) {
#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  cairo_scaled_font_destroy(font->cairo_font);
  font->cairo_font = NULL;

  if (font->compl_fonts) {
    int count;
    cairo_scaled_font_t* xfont;

    for (count = 0;; count++) {
      FcCharSetDestroy(font->compl_fonts[count].charset);
      if (!(xfont = font->compl_fonts[count].next)) {
        break;
      }
      cairo_scaled_font_destroy(xfont);
    }

    free(font->compl_fonts);
  }

  if (font->pattern) {
    FcPatternDestroy(font->pattern);
  }

  return 1;
}

int cairo_set_ot_font(ui_font_t* font) {
#ifdef USE_OT_LAYOUT
  font->ot_font = otl_open(cairo_ft_scaled_font_lock_face(font->cairo_font), 0);
  cairo_ft_scaled_font_unlock_face(font->cairo_font);

  return (font->ot_font != NULL);
#else
  return 0;
#endif
}

size_t ui_convert_ucs4_to_utf8(u_char* utf8, /* size of utf8 should be greater than 5. */
                               u_int32_t ucs) {
  /* ucs is unsigned */
  if (/* 0x00 <= ucs && */ ucs <= 0x7f) {
    *utf8 = ucs;

    return 1;
  } else if (ucs <= 0x07ff) {
    *(utf8++) = ((ucs >> 6) & 0xff) | 0xc0;
    *utf8 = (ucs & 0x3f) | 0x80;

    return 2;
  } else if (ucs <= 0xffff) {
    *(utf8++) = ((ucs >> 12) & 0x0f) | 0xe0;
    *(utf8++) = ((ucs >> 6) & 0x3f) | 0x80;
    *utf8 = (ucs & 0x3f) | 0x80;

    return 3;
  } else if (ucs <= 0x1fffff) {
    *(utf8++) = ((ucs >> 18) & 0x07) | 0xf0;
    *(utf8++) = ((ucs >> 12) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 6) & 0x3f) | 0x80;
    *utf8 = (ucs & 0x3f) | 0x80;

    return 4;
  } else if (ucs <= 0x03ffffff) {
    *(utf8++) = ((ucs >> 24) & 0x03) | 0xf8;
    *(utf8++) = ((ucs >> 18) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 12) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 6) & 0x3f) | 0x80;
    *utf8 = (ucs & 0x3f) | 0x80;

    return 5;
  } else if (ucs <= 0x7fffffff) {
    *(utf8++) = ((ucs >> 30) & 0x01) | 0xfc;
    *(utf8++) = ((ucs >> 24) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 18) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 12) & 0x3f) | 0x80;
    *(utf8++) = ((ucs >> 6) & 0x3f) | 0x80;
    *utf8 = (ucs & 0x3f) | 0x80;

    return 6;
  } else {
    return 0;
  }
}

u_int cairo_calculate_char_width(ui_font_t* font, u_int32_t ch) {
  u_char utf8[UTF_MAX_SIZE + 1];
  cairo_text_extents_t extents;
  int width;

#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->ot_font */) {
    cairo_glyph_t glyph;

    glyph.index = ch;
    glyph.x = glyph.y = 0;
    cairo_scaled_font_glyph_extents(font->cairo_font, &glyph, 1, &extents);
  } else
#endif
  {
    int idx;

    utf8[ui_convert_ucs4_to_utf8(utf8, ch)] = '\0';

    if (font->compl_fonts &&
        !FcCharSetHasChar(font->compl_fonts[0].charset, ch) &&
        (idx = ui_search_next_cairo_font(font, ch)) >= 0) {
      cairo_scaled_font_text_extents(font->compl_fonts[idx].next, utf8, &extents);
    } else {
      cairo_scaled_font_text_extents(font->cairo_font, utf8, &extents);
    }
  }

#if 0
  bl_debug_printf(BL_DEBUG_TAG " CHAR(%x) ui_bearing %f width %f x_advance %f\n", ch,
                  extents.ui_bearing, extents.width, extents.x_advance);
#endif

  if ((width = DOUBLE_ROUNDUP_TO_INT(extents.x_advance)) < 0) {
    return 0;
  } else {
    /* Some (indic) fonts could return minus value as text width. */
    return width;
  }
}

#endif

u_int ft_convert_text_to_glyphs(ui_font_t* font, u_int32_t* shaped, u_int shaped_len,
                                int8_t* offsets, u_int8_t* widths, u_int32_t* cmapped,
                                u_int32_t* src, u_int src_len, const char* script,
                                const char* features) {
#ifdef USE_OT_LAYOUT
  return otl_convert_text_to_glyphs(font->ot_font, shaped, shaped_len, offsets, widths, cmapped,
                                    src, src_len, script, features, 0);
#else
  return 0;
#endif
}
