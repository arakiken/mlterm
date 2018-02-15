/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_def.h>

#ifdef USE_WIN32API

#include <windows.h>
#include "../win32/ui_connect_dialog.c"

#else

#include "../fb/ui_connect_dialog.c"

#endif
