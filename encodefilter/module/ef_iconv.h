/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_ICONV_H__
#define __EF_ICONV_H__

#include <iconv.h>
#include <stdio.h> /* NULL */

#define ICONV_OPEN(cd, dstname, srcname) \
  { \
    static int tried; \
    if ((cd) == NULL) { \
      if (tried) { \
        return 0; \
      } else if (((cd) = iconv_open(dstname, srcname)) == NULL) { \
        tried = 1; \
        return 0; \
      } \
    } \
  }

/* It is assumed that src_len and dst_len should be immediate. */
#define ICONV(cd, src, src_len, dst, dst_len) \
  { \
    const char *src2 = src; \
    size_t src_len2 = src_len; \
    char *dst2 = dst; \
    size_t dst_len2 = 4; \
    if (iconv(cd, &src2, &src_len2, &dst2, &dst_len2) < 0 || \
        4 - dst_len2 != dst_len) {                           \
      return 0; \
    } \
  }

#endif
