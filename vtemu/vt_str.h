/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_STR_H__
#define __VT_STR_H__

#include <pobl/bl_mem.h> /* alloca */

#include "vt_char.h"

int vt_str_init(vt_char_t* str, u_int size);

vt_char_t* __vt_str_init(vt_char_t* str, u_int size);

#define vt_str_alloca(size) __vt_str_init(alloca(sizeof(vt_char_t) * (size)), (size))

vt_char_t* vt_str_new(u_int size);

int vt_str_final(vt_char_t* str, u_int size);

int vt_str_delete(vt_char_t* str, u_int size);

int vt_str_copy(vt_char_t* dst, vt_char_t* src, u_int size);

u_int vt_str_cols(vt_char_t* chars, u_int len);

int vt_str_equal(vt_char_t* str1, vt_char_t* str2, u_int len);

int vt_str_bytes_equal(vt_char_t* str1, vt_char_t* str2, u_int len);

#ifdef DEBUG

void vt_str_dump(vt_char_t* chars, u_int len);

#endif

#endif
