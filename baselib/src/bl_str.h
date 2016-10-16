/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_STR_H__
#define __BL_STR_H__

#include <string.h> /* strlen/strsep/strdup */

#include "bl_types.h" /* size_t */
#include "bl_def.h"
#include "bl_mem.h" /* alloca */

#ifdef HAVE_STRSEP

#define bl_str_sep(strp, delim) strsep(strp, delim)

#else

#define bl_str_sep(strp, delim) __bl_str_sep(strp, delim)

char* __bl_str_sep(char** strp, const char* delim);

#endif

/*
 * cpp doesn't necessarily process variable number of arguments.
 */
int bl_snprintf(char* str, size_t size, const char* format, ...);

#ifdef BL_DEBUG

#define strdup(str) bl_str_dup(str, __FILE__, __LINE__, __FUNCTION__)

#endif

char* bl_str_dup(const char* str, const char* file, int line, const char* func);

#define bl_str_alloca_dup(src) __bl_str_copy(alloca(strlen(src) + 1), (src))

char* __bl_str_copy(char* dst, const char* src);

size_t bl_str_tabify(u_char* dst, size_t dst_len, const u_char* src, size_t src_len,
                     size_t tab_len);

char* bl_str_chop_spaces(char* str);

int bl_str_n_to_uint(u_int* i, const char* s, size_t n);

int bl_str_n_to_int(int* i, const char* s, size_t n);

int bl_str_to_uint(u_int* i, const char* s);

int bl_str_to_int(int* i, const char* s);

u_int bl_count_char_in_str(const char* str, char ch);

int bl_compare_str(const char* str1, const char* str2);

char* bl_str_replace(const char* str, const char* orig, const char* new);

char* bl_str_unescape(const char* str);

#endif
