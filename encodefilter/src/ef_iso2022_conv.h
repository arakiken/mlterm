/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_ISO2022_CONV_H__
#define __EF_ISO2022_CONV_H__

#include <pobl/bl_types.h> /* size_t */

#include "ef_conv.h"

typedef struct ef_iso2022_conv {
  ef_conv_t conv;

  ef_charset_t* gl;
  ef_charset_t* gr;

  ef_charset_t g0;
  ef_charset_t g1;
  ef_charset_t g2;
  ef_charset_t g3;

} ef_iso2022_conv_t;

size_t ef_iso2022_illegal_char(ef_conv_t* conv, u_char* dst, size_t dst_size, int* is_full,
                                ef_char_t* ch);

void ef_iso2022_remap_unsupported_charset(ef_char_t* ch);

#endif
