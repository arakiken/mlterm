/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_FONT_H__
#define __UI_FONT_H__

/* X11/Xlib.h must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include "ui.h"

#include <pobl/bl_types.h>   /* u_int */
#include <vt_font.h>

#include "ui_type_engine.h"

#define FONT_VERTICAL (FONT_VERT_LTR|FONT_VERT_RTL)

typedef enum ui_font_present {
  FONT_VERT_LTR = 0x1, /* == VERT_LTR */
  FONT_VERT_RTL = 0x2, /* == VERT_RTL */
  FONT_VAR_WIDTH = 0x4,
  FONT_AA = 0x8,
  FONT_NOAA = 0x10, /* Don't specify with FONT_AA */

} ui_font_present_t;

typedef struct _XftFont *xft_font_ptr_t;
typedef struct _cairo_scaled_font *cairo_scaled_font_ptr_t;
typedef struct _FcCharSet *fc_charset_ptr_t;
typedef struct _FcPattern *fc_pattern_ptr_t;

#ifdef USE_XLIB
/* defined in xlib/ui_decsp_font.h */
typedef struct ui_decsp_font *ui_decsp_font_ptr_t;
#endif

typedef struct ui_font {
  /*
   * Private
   */
  Display *display;

  /*
   * Public(readonly)
   */
  vt_font_t id;

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
  xft_font_ptr_t xft_font;
#endif
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
  cairo_scaled_font_ptr_t cairo_font;
  struct {
    fc_charset_ptr_t charset;
    void *next;

  } * compl_fonts;
  fc_pattern_ptr_t pattern;
#endif
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
  XFontStruct *xfont;
#endif

#ifdef USE_XLIB
  ui_decsp_font_ptr_t decsp_font;
#endif

#ifdef USE_OT_LAYOUT
  /* ot_font == NULL and use_ot_layout == true is possible in ISO10646_UCS4_1_V
   * font. */
  void *ot_font;
  int8_t ot_font_not_found;
  int8_t use_ot_layout;
#endif

  /*
   * These members are never zero.
   */
  u_int16_t width;
  u_int16_t height;
  u_int16_t ascent;

  /* This is not zero only when is_proportional is true and xfont is set. */
  int8_t x_off;

  /*
   * If is_var_col_width is false and is_proportional is true,
   * draw one character at a time to fit column width. (see {xft_}draw_str())
   */
  int8_t is_var_col_width;
  int8_t is_proportional;
  int8_t is_vertical;
  int8_t double_draw_gap;
  int8_t size_attr;
#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
  int8_t init_pattern_for_compl;
#endif

} ui_font_t;

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
void ui_font_use_fontconfig(void);
#endif

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold, int letter_space);

ui_font_t *ui_font_new_for_decsp(Display *display, vt_font_t id, u_int width, u_int height,
                                 u_int ascent);

void ui_font_destroy(ui_font_t *font);

int ui_font_load_xft_font(ui_font_t *font, char *fontname, u_int fontsize, u_int col_width,
                          int use_medium_for_bold);

int ui_font_load_xfont(ui_font_t *font, char *fontname, u_int fontsize, u_int col_width,
                       int use_medium_for_bold);

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int is_awidth,
                              int *draw_alone);

int ui_font_has_ot_layout_table(ui_font_t *font);

u_int ui_convert_text_to_glyphs(ui_font_t *font, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                u_int32_t *noshape_glyphs, u_int32_t *src, u_int src_len,
                                const char *script, const char *features);

#ifdef USE_XLIB
char **ui_font_get_encoding_names(ef_charset_t cs);
#else
#define ui_font_get_encoding_names(cs) (0)
#endif

#if defined(USE_XLIB) || defined(USE_WAYLAND)
/* For mlterm-libvte */
void ui_font_set_dpi_for_fc(double dpi);
#else
#define ui_font_set_dpi_for_fc(dpi) (0)
#endif

#ifdef SUPPORT_POINT_SIZE_FONT
void ui_font_use_point_size(int use);
#else
#define ui_font_use_point_size(bool) (0)
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
void ui_use_cp932_ucs_for_xft(void);

u_int32_t ui_convert_to_xft_ucs4(u_int32_t ch, ef_charset_t cs);
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
size_t ui_convert_ucs4_to_utf16(u_char *utf16, u_int32_t ucs4);
#endif

#ifdef DEBUG
void ui_font_dump(ui_font_t *font);
#endif

#endif
