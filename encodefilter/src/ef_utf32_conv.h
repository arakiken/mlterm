/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_UTF32_CONV_H__
#define __EF_UTF32_CONV_H__

#include "ef_conv.h"

ef_conv_t *ef_utf32_conv_new(void);

ef_conv_t *ef_utf32le_conv_new(void);

int ef_utf32_conv_use_bom(ef_conv_t *conv);

#endif
