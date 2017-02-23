/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include "../ui_display.h"

#ifdef __FreeBSD__
#include <sys/kbio.h> /* NLKED */
#else
#define CLKED 1
#define NLKED 2
#define SLKED 4
#define ALKED 8
#endif

#define KeyPress 2      /* Private in fb/ */
#define ButtonPress 4   /* Private in fb/ */
#define ButtonRelease 5 /* Private in fb/ */
#define MotionNotify 6  /* Private in fb/ */

#ifdef USE_LIBSIXEL
void ui_display_output_picture(ui_display_t *disp, u_char *picture, u_int width, u_int height);
#else
#define ui_display_output_picture(disp, picture, width, height) (0)
#endif

#endif
