/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_H__
#define __UI_H__

#if defined(USE_WIN32GUI)

#include "win32/ui.h"
#define GUI_TYPE "win32"

#elif defined(USE_CONSOLE)

#include "console/ui.h"
#define GUI_TYPE "console"

#elif defined(USE_FRAMEBUFFER)

#include "fb/ui.h"
#define GUI_TYPE "fb"

#elif defined(USE_QUARTZ)

#include "quartz/ui.h"
#define GUI_TYPE "quartz"

#elif defined(USE_WAYLAND)

#include "wayland/ui.h"
#define GUI_TYPE "wayland"

#else

#include "xlib/ui.h"
#define USE_XLIB
#define GUI_TYPE "xlib"

#endif

/*
 * Note for XStringToKeysym(char *str)
 * The values of 'a'-'z' keysyms are always 0x61-0x7a, but the values of other
 * keysyms (signs, digits etc) aren't necessarily their ascii values.
 */

/*
 * Xlib utility definitions.
 */

#define ModMask (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)

#ifndef Button6
#define Button6 6
#endif
#ifndef Button6Mask
#define Button6Mask (Button5Mask << 1)
#endif
#ifndef Button7
#define Button7 7
#endif
#ifndef Button7Mask
#define Button7Mask (Button5Mask << 2)
#endif
#define ButtonMask \
  (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask | Button6Mask | Button7Mask)

#endif
