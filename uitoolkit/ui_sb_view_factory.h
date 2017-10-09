/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SB_VIEW_FACTORY_H__
#define __UI_SB_VIEW_FACTORY_H__

#include "ui_sb_view.h"

ui_sb_view_t *ui_sb_view_new(const char *name);

ui_sb_view_t *ui_transparent_sb_view_new(const char *name);

void ui_unload_scrollbar_view_lib(const char *name);

#endif
