/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_util.h"

#include <string.h>
#include <stdlib.h> /* getenv */
#ifdef HAVE_GETPWUID
#include <pwd.h>
#endif

#include "bl_unistd.h" /* bl_getuid */
#include "bl_debug.h"

/* --- static functions --- */

static int hex_to_int(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else {
    c &= 0xdf;

    if ('A' <= c && c <= 'F') {
      return c - 'A' + 10;
    }
  }

  return -1;
}

static inline int get_pc(int h, u_int32_t m1, u_int32_t m2) {
  if (h < 60) {
    return m1 + (m2 - m1) * h / 60;
  } else if (h < 180) {
    return m2;
  } else if (h < 240) {
    return m1 + (m2 - m1) * (240 - h) / 60;
  } else {
    return m1;
  }
}

/* --- global functions --- */

size_t bl_hex_decode(char *decoded, const char *encoded, size_t e_len) {
  size_t pos;
  char *p = decoded;

  for (pos = 0; e_len > pos + 1; pos += 2) {
    int i1 = hex_to_int(encoded[pos]);
    int i2 = hex_to_int(encoded[pos + 1]);

    if (i1 < 0 || i2 < 0) {
      break;
    }

    *(p++) = i1 * 16 + i2;
  }

  return (p - decoded);
}


size_t bl_hex_encode(char *encoded, const char *decoded, size_t d_len) {
  size_t pos;
  char *p = encoded;

  for (pos = 0; d_len > pos; pos ++) {
    int h1 = (decoded[pos] >> 4) & 0xf;
    int h2 = decoded[pos] & 0xf;

    *(p++) = (h1 > 9) ? ('a' + h1 - 10) : ('0' + h1);
    *(p++) = (h2 > 9) ? ('a' + h2 - 10) : ('0' + h2);
  }

  return (p - encoded);
}

size_t bl_base64_decode(char *decoded, const char *encoded, size_t e_len) {
  size_t d_pos;
  size_t e_pos;
  /* ASCII -> Base64 order */
  int8_t conv_tbl[] = {/* 0x2b - */
                       62, -1, -1, -1, 63,
                       /* 0x30 - */
                       52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
                       /* 0x40 - */
                       -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                       /* 0x50 - */
                       15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
                       /* 0x60 - */
                       -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                       /* 0x70 - 7a */
                       41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

  d_pos = e_pos = 0;

  while (e_len >= e_pos + 4) {
    size_t count;
    int8_t bytes[4];

    for (count = 0; count < 4; e_pos++) {
      if (encoded[e_pos] < 0x2b || 0x7a < encoded[e_pos] ||
          (bytes[count] = conv_tbl[encoded[e_pos] - 0x2b]) == -1) {
#ifdef DEBUG
        if (encoded[e_pos] != '\x0d' && encoded[e_pos] != '\x0a') {
          bl_debug_printf(BL_DEBUG_TAG " ignoring %c in base64\n", encoded[e_pos]);
        }
#endif

        if (e_len <= e_pos + 1) {
          goto end;
        }
      } else {
        count++;
      }
    }

    decoded[d_pos++] = (((bytes[0] << 2) & 0xfc) | ((bytes[1] >> 4) & 0x3));

    if (bytes[2] != -2) {
      decoded[d_pos++] = (((bytes[1] << 4) & 0xf0) | ((bytes[2] >> 2) & 0xf));
    } else {
      break;
    }

    if (bytes[3] != -2) {
      decoded[d_pos++] = (((bytes[2] << 6) & 0xc0) | (bytes[3] & 0x3f));
    } else {
      break;
    }
  }

end:
#ifdef DEBUG
  decoded[d_pos] = '\0';
  if (strlen(encoded) < 1000) {
    bl_debug_printf(BL_DEBUG_TAG " Base64 Decode %s => %s\n", encoded, decoded);
  }
#endif

  return d_pos;
}

size_t bl_base64_encode(char *encoded, const char *decoded, size_t d_len) {
  /* XXX Not implemented */
}

void bl_hls_to_rgb(int *r, int *g, int *b,
                   int h /* 0-360 */, int l /* 0-100 */, int s /* 0-100 */) {
  if (s == 0) {
    *r = *g = *b = l * 255 / 100;
  } else {
    u_int32_t m1;
    u_int32_t m2;

    if (l < 50) {
      m2 = l * (100 + s);
    } else {
      m2 = (l + s) * 100 - l * s;
    }
    m1 = l * 200 - m2;

    *r = get_pc(h, m1, m2) * 255 / 10000;
    if ((h -= 120) < 0) {
      h += 360;
    }
    *g = get_pc(h, m1, m2) * 255 / 10000;
    if ((h -= 120) < 0) {
      h += 360;
    }
    *b = get_pc(h, m1, m2) * 255 / 10000;
  }
}

void bl_rgb_to_hls(int *h, int *l, int *s,
                   int r /* 0-255 */, int g /* 0-255 */, int b /* 0-255 */) {
  int max;
  int min;

  if (r > g) {
    if (g > b) {
      /* r > g > b */
      max = r;
      min = b;
    } else {
      min = g;
      if (r > b) {
        /* r > b >= g */
        max = r;
      } else {
        /* b >= r > g */
        max = b;
      }
    }
  } else if (b > g) {
    /* b > g >= r */
    max = b;
    min = r;
  } else {
    max = g;
    if (r > b) {
      /* g >= r > b */
      min = b;
    } else {
      /* g >= b >= r */
      min = r;
    }
  }

  *l = (max + min) * 100 / 255 / 2;

  if (max == min) {
    /* r == g == b */
    *h = 0;
    *s = 0;
  } else {
    if (max == r) {
      *h = 60 * (g - b) / (max - min);
    } else if (max == g) {
      *h = 60 * (b - r) / (max - min) + 120;
    } else /* if (max == b) */ {
      *h = 60 * (r - g) / (max - min) + 240;
    }

    if (*h < 0) {
      *h += 360;
    }

    if (max + min < 255) {
      *s = (max - min) * 100 / (max + min);
    } else {
      *s = (max - min) * 100 / (510 - max - min);
    }
  }
}

const char *bl_get_user_name(void) {
  char *user;

#ifdef HAVE_WINDOWS_H
  if (!(user = getenv("USERNAME")))
#endif
  {
    if (!(user = getenv("USER"))) {
      user = getenv("LOGNAME");

#ifdef HAVE_GETPWUID
      if (!user) {
        struct passwd *p;

        if ((p = getpwuid(bl_getuid()))) {
          user = p->pw_name;
        }
      }
#endif
    }
  }

  return user;
}
