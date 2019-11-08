/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef BL_DEBUG

#include "ui.h" /* USE_XLIB */

void TEST_bl_args(void);
void TEST_bl_map(void);
void TEST_bl_path(void);
void TEST_ui_font_config(void);

#ifndef USE_XLIB
void TEST_xstringtokeysym(void);
#endif

/* -- global functions --- */

void test(void) {
	TEST_bl_args();
	TEST_bl_map();
	TEST_bl_path();
	TEST_ui_font_config();

#ifndef USE_XLIB
	TEST_xstringtokeysym();
#endif
}

#endif /* BL_DEBUG */
