/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_UTIL_H__
#define __VT_UTIL_H__

#include <sys/types.h>

size_t vt_hex_encode(char *encoded, const char *decoded, size_t e_len);

size_t vt_hex_decode(char *decoded, const char *encoded, size_t e_len);

size_t vt_base64_decode(char *decoded, const char *encoded, size_t e_len);

#endif
