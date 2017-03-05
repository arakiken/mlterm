/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

/* This must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include <X11/Xlib.h>
#include <X11/keysym.h>     /* XK_xxx */
#include <X11/Xatom.h>      /* XA_xxx */
#include <X11/Xutil.h>      /* IsKeypadKey */
#include <X11/cursorfont.h> /* for cursor shape */

typedef Pixmap PixmapMask;

#ifdef XK_F21
#define XK_FMAX XK_F35
#else
#define XK_FMAX XK_F20
#endif

/* === Platform dependent options === */

#define UI_COLOR_HAS_RGB
#define SUPPORT_TRUE_TRANSPARENT_BG
#undef TYPE_XCORE_SCALABLE
#undef MANAGE_ROOT_WINDOWS_BY_MYSELF
#undef MANAGE_SUB_WINDOWS_BY_MYSELF
#undef INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#define SUPPORT_POINT_SIZE_FONT
#undef XIM_SPOT_IS_LINE_TOP
#define USE_GC
#define CHANGEABLE_CURSOR
#undef PLUGIN_MODULE_SUFFIX
#undef KEY_REPEAT_BY_MYSELF
#undef ROTATABLE_DISPLAY
#undef PSEUDO_COLOR_DISPLAY
#undef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
#define SUPPORT_URGENT_BELL
#undef FORCE_UNICODE
#define NEED_DISPLAY_SYNC_EVERY_TIME
#undef DRAW_SCREEN_IN_PIXELS
#undef NO_DRAW_IMAGE_STRING
/*
 * for ui_picture.c
 * Xlib doesn't work on threading without XInitThreads().
 * (libpthread is not linked to mlterm explicitly for now.)
 */
#undef HAVE_PTHREAD

#endif
