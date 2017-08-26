/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SELECTION_ENCODING_H__
#define __UI_SELECTION_ENCODING_H__

#include <mef/ef_parser.h>
#include <mef/ef_conv.h>

#include "ui.h"

#ifdef USE_XLIB
void ui_set_big5_selection_buggy(int buggy);
#else
#define ui_set_big5_selection_buggy(buggy) (0)
#endif

ef_parser_t *ui_get_selection_parser(int utf);

ef_conv_t *ui_get_selection_conv(int utf);

#endif
