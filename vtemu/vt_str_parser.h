/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_STR_PARSER_H__
#define __VT_STR_PARSER_H__

#include <mef/ef_parser.h>
#include <pobl/bl_types.h>

#include "vt_char.h"

ef_parser_t* vt_str_parser_new(void);

void vt_str_parser_set_str(ef_parser_t* ef_parser, vt_char_t* str, u_int size);

#endif
