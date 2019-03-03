/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_font.h"

#include <stdio.h>
#include <string.h> /* memset/strncasecmp */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* bl_snprintf */
#include <pobl/bl_mem.h>    /* alloca */
#include <pobl/bl_str.h>    /* bl_str_sep/bl_str_to_int */
#include <pobl/bl_locale.h> /* bl_get_lang() */
#include <mef/ef_ucs4_map.h>
#include <mef/ef_ucs_property.h>
#include <vt_char_encoding.h> /* ui_convert_to_xft_ucs4 */

#ifdef USE_OT_LAYOUT
#include "otl.h"
#endif

#define FOREACH_FONT_ENCODINGS(csinfo, font_encoding_p) \
  for ((font_encoding_p) = &csinfo->encoding_names[0]; *(font_encoding_p); (font_encoding_p)++)

#if 0
#define __DEBUG
#endif

typedef struct wincs_info {
  DWORD cs;
  vt_char_encoding_t encoding;

} wincs_info_t;

typedef struct cs_info {
  ef_charset_t cs;
  DWORD wincs;

} cs_info_t;

/* --- static variables --- */

static wincs_info_t wincs_info_table[] = {
    {
     DEFAULT_CHARSET, VT_UNKNOWN_ENCODING,
    },
    {
     SYMBOL_CHARSET, VT_UNKNOWN_ENCODING,
    },
    {
     OEM_CHARSET, VT_UNKNOWN_ENCODING,
    },
    {
     ANSI_CHARSET, VT_CP1252,
    },
    {
     RUSSIAN_CHARSET, VT_CP1251,
    },
    {
     GREEK_CHARSET, VT_CP1253,
    },
    {
     TURKISH_CHARSET, VT_CP1254,
    },
    {
     BALTIC_CHARSET, VT_CP1257,
    },
    {
     HEBREW_CHARSET, VT_CP1255,
    },
    {
     ARABIC_CHARSET, VT_CP1256,
    },
    {
     SHIFTJIS_CHARSET, VT_SJIS,
    },
    {
     HANGEUL_CHARSET, VT_UHC,
    },
    {
     GB2312_CHARSET, VT_GBK,
    },
    {CHINESEBIG5_CHARSET, VT_BIG5},
    {
     JOHAB_CHARSET, VT_JOHAB,
    },
    {
     THAI_CHARSET, VT_TIS620,
    },
    {
     EASTEUROPE_CHARSET, VT_ISO8859_3,
    },
    {
     MAC_CHARSET, VT_UNKNOWN_ENCODING,
    },
};

static cs_info_t cs_info_table[] = {
    {
     ISO10646_UCS4_1, DEFAULT_CHARSET,
    },
    {
     ISO10646_UCS4_1_V, DEFAULT_CHARSET,
    },

    {
     DEC_SPECIAL, SYMBOL_CHARSET,
    },
    {
     ISO8859_1_R, ANSI_CHARSET,
    },
    {
     ISO8859_2_R, DEFAULT_CHARSET,
    },
    {
     ISO8859_3_R, EASTEUROPE_CHARSET,
    },
    {
     ISO8859_4_R, DEFAULT_CHARSET,
    },
    {
     ISO8859_5_R, RUSSIAN_CHARSET,
    },
    {
     ISO8859_6_R, ARABIC_CHARSET,
    },
    {
     ISO8859_7_R, GREEK_CHARSET,
    },
    {
     ISO8859_8_R, HEBREW_CHARSET,
    },
    {
     ISO8859_9_R, TURKISH_CHARSET,
    },
    {
     ISO8859_10_R, DEFAULT_CHARSET,
    },
    {
     TIS620_2533, THAI_CHARSET,
    },
    {
     ISO8859_13_R, DEFAULT_CHARSET,
    },
    {
     ISO8859_14_R, DEFAULT_CHARSET,
    },
    {
     ISO8859_15_R, DEFAULT_CHARSET,
    },
    {
     ISO8859_16_R, DEFAULT_CHARSET,
    },
    {
     TCVN5712_3_1993, VIETNAMESE_CHARSET,
    },
    {
     ISCII_ASSAMESE, DEFAULT_CHARSET,
    },
    {
     ISCII_BENGALI, DEFAULT_CHARSET,
    },
    {
     ISCII_GUJARATI, DEFAULT_CHARSET,
    },
    {
     ISCII_HINDI, DEFAULT_CHARSET,
    },
    {
     ISCII_KANNADA, DEFAULT_CHARSET,
    },
    {
     ISCII_MALAYALAM, DEFAULT_CHARSET,
    },
    {
     ISCII_ORIYA, DEFAULT_CHARSET,
    },
    {
     ISCII_PUNJABI, DEFAULT_CHARSET,
    },
    {
     ISCII_TAMIL, DEFAULT_CHARSET,
    },
    {
     ISCII_TELUGU, DEFAULT_CHARSET,
    },
    {
     VISCII, VIETNAMESE_CHARSET,
    },
    {
     KOI8_R, RUSSIAN_CHARSET,
    },
    {
     KOI8_U, RUSSIAN_CHARSET,
    },
    {
     KOI8_T, RUSSIAN_CHARSET,
    },
    {
     GEORGIAN_PS, DEFAULT_CHARSET,
    },
    {
     CP1250, EASTEUROPE_CHARSET,
    },
    {
     CP1251, RUSSIAN_CHARSET,
    },
    {
     CP1252, ANSI_CHARSET,
    },
    {
     CP1253, GREEK_CHARSET,
    },
    {
     CP1254, TURKISH_CHARSET,
    },
    {
     CP1255, HEBREW_CHARSET,
    },
    {
     CP1256, ARABIC_CHARSET,
    },
    {
     CP1257, BALTIC_CHARSET,
    },
    {
     CP1258, VIETNAMESE_CHARSET,
    },

    {
     JISX0201_KATA, SHIFTJIS_CHARSET,
    },
    {
     JISX0201_ROMAN, SHIFTJIS_CHARSET,
    },
    {
     JISC6226_1978, SHIFTJIS_CHARSET,
    },
    {
     JISX0208_1983, SHIFTJIS_CHARSET,
    },
    {
     JISX0208_1990, SHIFTJIS_CHARSET,
    },
    {
     JISX0212_1990, SHIFTJIS_CHARSET,
    },
    {
     JISX0213_2000_1, SHIFTJIS_CHARSET,
    },
    {
     JISX0213_2000_2, SHIFTJIS_CHARSET,
    },

    {
     KSC5601_1987, HANGEUL_CHARSET,
    },
    {
     UHC, HANGEUL_CHARSET,
    },
    {
     JOHAB, JOHAB_CHARSET,
    },

    {
     GB2312_80, GB2312_CHARSET,
    },
    {
     GBK, GB2312_CHARSET,
    },
    {
     BIG5, CHINESEBIG5_CHARSET,
    },
    {
     HKSCS, DEFAULT_CHARSET,
    },
    {
     CNS11643_1992_1, GB2312_CHARSET,
    },
    {
     CNS11643_1992_2, GB2312_CHARSET,
    },
    {
     CNS11643_1992_3, GB2312_CHARSET,
    },
    {
     CNS11643_1992_4, GB2312_CHARSET,
    },
    {
     CNS11643_1992_5, GB2312_CHARSET,
    },
    {
     CNS11643_1992_6, GB2312_CHARSET,
    },
    {
     CNS11643_1992_7, GB2312_CHARSET,
    },

};

static GC display_gc;

static int use_point_size;

/* --- static functions --- */

static wincs_info_t *get_wincs_info(ef_charset_t cs) {
  int count;

  for (count = 0; count < sizeof(cs_info_table) / sizeof(cs_info_t); count++) {
    if (cs_info_table[count].cs == cs) {
      DWORD wincs;

      wincs = cs_info_table[count].wincs;

      for (count = 0; count < sizeof(wincs_info_table) / sizeof(wincs_info_t); count++) {
        if (wincs_info_table[count].cs == wincs) {
          return &wincs_info_table[count];
        }
      }

      break;
    }
  }

#ifdef DEBUG
  bl_warn_printf(BL_DEBUG_TAG " not supported cs(%x).\n", cs);
#endif

  return NULL;
}

static int parse_font_name(
    char **font_family, int *font_weight, /* if weight is not specified in
                                             font_name , not changed. */
    int *is_italic,    /* if slant is not specified in font_name , not changed. */
    double *font_size, /* if size is not specified in font_name , not changed. */
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
 * Following is the same as libtype/ui_font_ft.c and fb/ui_font.c.
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
        struct {
          char *style;
          int weight;
          int is_italic;

        } styles[] = {
            /*
             * Portable styles.
             */
            /* slant */
            {
             "italic", 0, 1,
            },
            /* weight */
            {
             "bold", FW_BOLD, 0,
            },

            /*
             * Hack for styles which can be returned by
             * gtk_font_selection_dialog_get_font_name().
             */
            /* slant */
            {
             "oblique", /* XXX This style is ignored. */
             0, 0,
            },
            /* weight */
            {
             "light", /* e.g. "Bookman Old Style Light" */
             FW_LIGHT, 0,
            },
            {
             "semi-bold", FW_SEMIBOLD, 0,
            },
            {
             "heavy", /* e.g. "Arial Black Heavy" */
             FW_HEAVY, 0,
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
            } else if (styles[count].is_italic) {
              *is_italic = 1;
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

static u_int calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs) {
  SIZE sz;

  if (!display_gc) {
    /*
     * Cached as far as ui_caculate_char_width is called.
     * display_gc is destroyed in ui_font_new or ui_font_destroy.
     */
    display_gc = CreateIC("Display", NULL, NULL, NULL);
  }

  SelectObject(display_gc, font->xfont->fid);

  if (cs != US_ASCII && !IS_ISCII(cs)) {
    u_int32_t ucs4_code;
    u_char utf16[4];
    int len;
    BOOL ret;

    if (IS_ISO10646_UCS4(cs)) {
      ucs4_code = ch;
    } else {
      ef_char_t non_ucs;
      ef_char_t ucs4;

      non_ucs.size = CS_SIZE(cs);
      non_ucs.property = 0;
      non_ucs.cs = cs;
      ef_int_to_bytes(non_ucs.ch, non_ucs.size, ch);

      if (vt_is_msb_set(cs)) {
        u_int count;

        for (count = 0; count < non_ucs.size; count++) {
          non_ucs.ch[count] &= 0x7f;
        }
      }

      if (ef_map_to_ucs4(&ucs4, &non_ucs)) {
        ucs4_code = ef_bytes_to_int(ucs4.ch, 4);
      } else {
        return 0;
      }
    }

    len = ui_convert_ucs4_to_utf16(utf16, ucs4_code) / 2;

#ifdef USE_OT_LAYOUT
    if (font->use_ot_layout /* && font->ot_font */) {
#ifdef USE_WIN32API
      /* GetTextExtentPointI doesn't exist on mingw-i686-pc-mingw */
      static BOOL (*func)(HDC, LPWORD, int, LPSIZE);

      if (!func) {
        func = GetProcAddress(GetModuleHandleA("GDI32.DLL"), "GetTextExtentPointI");
      }

      ret = (*func)(display_gc, utf16, len, &sz);
#else
      ret = GetTextExtentPointI(display_gc, utf16, len, &sz);
#endif
    } else
#endif
    {
      ret = GetTextExtentPoint32W(display_gc, utf16, len, &sz);
    }

    if (!ret) {
      return 0;
    }
  } else {
    u_char c;

    c = ch;
    if (!GetTextExtentPoint32A(display_gc, &c, 1, &sz)) {
      return 0;
    }
  }

  if (sz.cx < 0) {
    return 0;
  } else {
    return sz.cx;
  }
}

/* --- global functions --- */

void ui_compose_dec_special_font(void) {
  /* Do nothing for now in win32. */
}

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold,
                       u_int letter_space /* Ignored for now. */
                       ) {
  ui_font_t *font;
  u_int cols;
  wincs_info_t *wincsinfo;
  char *font_family;
  int weight;
  int is_italic;
  double fontsize_d;
  u_int percent;

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

  if (IS_ISCII(FONT_CS(font->id)) || FONT_CS(font->id) == ISO10646_UCS4_1_V) {
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

  if ((wincsinfo = get_wincs_info(FONT_CS(font->id))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " charset(0x%.2x) is not supported.\n", FONT_CS(font->id));
#endif

    free(font);

    return NULL;
  }

  if (font->id & FONT_BOLD) {
#if 0
    weight = FW_BOLD;
#else
    /*
     * XXX
     * Width of bold font is not necessarily the same as
     * that of normal font in win32.
     * So ignore weight and if font->id is bold use double-
     * drawing method for now.
     */
    weight = FW_DONTCARE;
#endif
  } else {
    weight = FW_MEDIUM;
  }

  if (font->id & FONT_ITALIC) {
    is_italic = TRUE;
  } else {
    is_italic = FALSE;
  }

  if (size_attr) {
    if (size_attr >= DOUBLE_HEIGHT_TOP) {
      fontsize *= 2;
    }

    col_width *= 2;
  }

  font_family = NULL;
  percent = 0;
  fontsize_d = (double)fontsize;

  if (FONT_CS(font->id) == DEC_SPECIAL) {
    font_family = "Tera Special";
  } else if (fontname) {
    char *p;

    if ((p = alloca(strlen(fontname) + 1)) == NULL) {
      free(font);

      return NULL;
    }
    strcpy(p, fontname);

    parse_font_name(&font_family, &weight, &is_italic, &fontsize_d, &percent, p);
  } else {
    /* Default font */
    font_family = "Courier New";
  }

  if (!display_gc) {
    display_gc = CreateIC("Display", NULL, NULL, NULL);
  }

  font->xfont->fid =
      CreateFont(use_point_size ? /* Height */
                     -MulDiv((int)fontsize_d, GetDeviceCaps(display_gc, LOGPIXELSY), 72)
                                : (int)fontsize_d,
#if 0
                 col_width ? (font->is_vertical ? col_width / 2 : col_width) : (int)fontsize_d / 2,
#else
                 size_attr == DOUBLE_WIDTH ? (use_point_size ? -MulDiv((int)fontsize_d, GetDeviceCaps(display_gc, LOGPIXELSX), 72) : (int)fontsize_d) : 0, /* Width (0=auto) */
#endif
                 0,         /* text angle */
                 0,         /* char angle */
                 weight,    /* weight */
                 is_italic, /* italic */
                 FALSE,     /* underline */
                 FALSE,     /* eraseline */
                 wincsinfo->cs, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                 (font_present & FONT_AA) ? ANTIALIASED_QUALITY : PROOF_QUALITY,
                 FIXED_PITCH | FF_MODERN, font_family);

  if (!font->xfont->fid) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateFont failed.\n");
    free(font);
#endif

    return NULL;
  } else {
    TEXTMETRIC tm;
    SIZE w_sz;
    SIZE l_sz;

    SelectObject(display_gc, font->xfont->fid);

    GetTextMetrics(display_gc, &tm);

    /*
     * Note that fixed pitch font containing both Hankaku and Zenkaku characters
     * like
     * MS Gothic is regarded as VARIABLE_PITCH. (tm.tmPitchAndFamily)
     * So "w" and "l" width is compared to check if font is proportional or not.
     */
    GetTextExtentPoint32A(display_gc, "w", 1, &w_sz);
    GetTextExtentPoint32A(display_gc, "l", 1, &l_sz);

#if 0
    bl_debug_printf(
        "Family %s Size %d CS %x => AveCharWidth %d Maui.harWidth %d 'w' width "
        "%d 'l' width %d Height %d Ascent %d ExLeading %d InLeading %d "
        "Pitch&Family %d Weight %d\n",
        font_family, fontsize, wincsinfo->cs, tm.tmAveCharWidth, tm.tmMaui.harWidth, w_sz.cx,
        l_sz.cx, tm.tmHeight, tm.tmAscent, tm.tmExternalLeading, tm.tmInternalLeading,
        tm.tmPitchAndFamily, tm.tmWeight);
#endif

    if (w_sz.cx != l_sz.cx) {
      font->is_proportional = 1;
      font->width = tm.tmAveCharWidth * cols;
    } else {
      SIZE sp_sz;
      WORD sp = 0x3000; /* wide space */

      if (cols == 2 && GetTextExtentPoint32W(display_gc, &sp, 1, &sp_sz) &&
          sp_sz.cx != l_sz.cx * 2) {
        font->is_proportional = 1;
        font->width = sp_sz.cx;
      } else {
        font->width = w_sz.cx * cols;
      }
    }

    font->height = tm.tmHeight;
    font->ascent = tm.tmAscent;

    if ((font->id & FONT_BOLD) && tm.tmWeight <= FW_MEDIUM) {
      font->double_draw_gap = 1;
    }
  }

  font->xfont->size = fontsize;

  /*
   * Following processing is same as ui_font.c:set_xfont()
   */

  if (col_width == 0) {
    /* standard(usascii) font */

    if (percent > 0) {
      u_int ch_width;

      if (font->is_vertical) {
        /*
         * !! Notice !!
         * The width of full and half character font is the same.
         */
        ch_width = fontsize * percent / 100;
      } else {
        ch_width = fontsize * percent / 200;
      }

      if (!font->is_var_col_width && font->width != ch_width) {
        font->is_proportional = 1;

        if (font->width < ch_width) {
          font->x_off = (ch_width - font->width) / 2;
        }

        font->width = ch_width;
      }
    } else if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      font->is_proportional = 1;
      font->x_off = font->width / 2;
      font->width *= 2;
    }

    /* letter_space is ignored in variable column width mode. */
    if (!font->is_var_col_width && letter_space > 0) {
      font->is_proportional = 1;
      font->width += letter_space;
      font->x_off += (letter_space / 2);
    }
  } else {
    /* not a standard(usascii) font */

    /*
     * XXX hack
     * forcibly conforming non standard font width to standard font width.
     */

    if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      if (font->width != col_width) {
        font->is_proportional = 1;

        /* is_var_col_width is always false if is_vertical is true. */
        if (/* ! font->is_var_col_width && */ font->width < col_width) {
          font->x_off = (col_width - font->width) / 2;
        }

        font->width = col_width;
      }
    } else {
      if (font->width != col_width * cols) {
        bl_msg_printf(
            "Font width(%d) is not matched with "
            "standard width(%d).\n",
            font->width, col_width * cols);

        font->is_proportional = 1;

        if (!font->is_var_col_width && font->width < col_width * cols) {
          font->x_off = (col_width * cols - font->width) / 2;
        }

        font->width = col_width * cols;
      }
    }
  }

  font->size_attr = size_attr;

  if (wincsinfo->cs != ANSI_CHARSET && wincsinfo->cs != SYMBOL_CHARSET &&
      !IS_ISO10646_UCS4(FONT_CS(font->id))) {
    if (!(font->xfont->conv = vt_char_encoding_conv_new(wincsinfo->encoding))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " vt_char_encoding_conv_new(font id %x) failed.\n", font->id);
#endif
    }
  }

  if (font->is_proportional && !font->is_var_col_width) {
    bl_msg_printf("Characters (cs %x) are drawn *one by one *to arrange column width.\n",
                  FONT_CS(font->id));
  }

  return font;
}

void ui_font_destroy(ui_font_t *font) {
#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  if (font->xfont->fid) {
    DeleteObject(font->xfont->fid);
  }

  if (font->xfont->conv) {
    (*font->xfont->conv->destroy)(font->xfont->conv);
  }

  free(font);

  if (display_gc) {
    DeleteDC(display_gc);
    display_gc = None;
  }
}

#ifdef USE_OT_LAYOUT
int ui_font_has_ot_layout_table(ui_font_t *font) {
  if (!font->ot_font) {
    u_char buf[4];
    void *font_data;
    u_int size;
    int is_ttc;

    if (font->ot_font_not_found) {
      return 0;
    }

    if (!display_gc) {
      /*
       * Cached as far as ui_caculate_char_width is called.
       * display_gc is destroyed in ui_font_new or ui_font_destroy.
       */
      display_gc = CreateIC("Display", NULL, NULL, NULL);
    }

    SelectObject(display_gc, font->xfont->fid);

#define TTC_TAG ('t' << 0) + ('t' << 8) + ('c' << 16) + ('f' << 24)

    if (GetFontData(display_gc, TTC_TAG, 0, &buf, 1) == 1) {
      is_ttc = 1;
      size = GetFontData(display_gc, TTC_TAG, 0, NULL, 0);
    } else {
      is_ttc = 0;
      size = GetFontData(display_gc, 0, 0, NULL, 0);
    }

    if (!(font_data = malloc(size))) {
      font->ot_font = NULL;
    } else {
      if (is_ttc) {
        GetFontData(display_gc, TTC_TAG, 0, font_data, size);
      } else {
        GetFontData(display_gc, 0, 0, font_data, size);
      }

      font->ot_font = otl_open(font_data, size);
    }

    if (!font->ot_font) {
      font->ot_font_not_found = 1;

      return 0;
    }
  }

  return 1;
}

u_int ui_convert_text_to_glyphs(ui_font_t *font, u_int32_t *shaped, u_int shaped_len,
                                int8_t *offsets, u_int8_t *widths, u_int32_t *cmapped,
                                u_int32_t *src, u_int src_len, const char *script,
                                const char *features) {
  u_int size;

  if (use_point_size) {
    if (!display_gc) {
      display_gc = CreateIC("Display", NULL, NULL, NULL);
    }

    size = MulDiv((int)font->xfont->size, GetDeviceCaps(display_gc, LOGPIXELSY), 72);
  } else {
    size = font->xfont->size;
  }

  return otl_convert_text_to_glyphs(font->ot_font, shaped, shaped_len, offsets, widths, cmapped,
                                    src, src_len, script, features,
                                    size * (font->size_attr >= DOUBLE_WIDTH ? 2 : 1));
}
#endif /* USE_OT_LAYOUT */

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int *draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

  if (font->is_proportional) {
    if (font->is_var_col_width) {
      u_int width;

      return width = calculate_char_width(font, ch, cs);
    }

    if (draw_alone) {
      *draw_alone = 1;
    }
  } else if (draw_alone &&
             /* ISO10646_UCS4_1_V is always proportional */
             cs == ISO10646_UCS4_1
#ifdef USE_OT_LAYOUT
             && (!font->use_ot_layout /* || ! font->ot_font */)
#endif
                 ) {
    if (ef_get_ucs_property(ch) & EF_AWIDTH) {
      if (calculate_char_width(font, ch, cs) != font->width) {
        *draw_alone = 1;
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
  bl_msg_printf("  id %x: Font %p", font->id, font->xfont->fid);

  if (font->is_proportional) {
    bl_msg_printf(" (proportional)");
  }
  bl_msg_printf("\n");
}

#endif
