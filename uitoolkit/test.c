/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef BL_DEBUG

#include "ui.h" /* USE_XLIB */

void TEST_bl_args(void);
void TEST_bl_map(void);
void TEST_bl_path(void);
void TEST_bl_str(void);
void TEST_encodefilter(void);
void TEST_ui_font_config(void);
void TEST_ui_emoji(void);
void TEST_vt_parser(void);
void TEST_vt_termcap(void);
#ifdef USE_OT_LAYOUT
void TEST_vt_shape(void);
#endif

#ifndef NO_IMAGE
void TEST_sixel_realloc_pixels(void);
#endif

#ifndef USE_XLIB
void TEST_xstringtokeysym(void);
#endif

/* -- global functions --- */

void test(void) {
  TEST_bl_args();
  TEST_bl_map();
  TEST_bl_path();
  TEST_bl_str();
  TEST_encodefilter();
  TEST_ui_font_config();
  TEST_ui_emoji();
  TEST_vt_parser();
  TEST_vt_termcap();
#ifdef USE_OT_LAYOUT
  TEST_vt_shape();
#endif

#ifndef NO_IMAGE
  TEST_sixel_realloc_pixels();
#endif

#ifndef USE_XLIB
  TEST_xstringtokeysym();
#endif
}

#endif /* BL_DEBUG */
