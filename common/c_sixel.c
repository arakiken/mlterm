/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <string.h>       /* memcpy */
#include <sys/stat.h>     /* fstat */
#include <pobl/bl_util.h> /* BL_MIN */
#include <pobl/bl_mem.h>  /* alloca */

/*
 * The size of the base palettes.
 * SIXEL_PALETTE_SIZE should be less than 1024.
 * (See the block of if ((color = params[0]) >= SIXEL_PALETTE_SIZE))
 */
#ifndef SIXEL_PALETTE_SIZE
#define SIXEL_PALETTE_SIZE 256
#endif

/*
 * Support sixel graphics extension which "sayaka --ormode on" outputs.
 * (See https://github.com/isaki68k/sayaka/,
 *  https://github.com/isaki68k/sayaka/blob/master/vala/sixel.native.c#L113)
 */
#if 0
#define SIXEL_ORMODE
#endif

#ifdef SIXEL_1BPP

#define correct_height correct_height_1bpp
#define load_sixel_from_data load_sixel_from_data_1bpp
#define SIXEL_RGB100(r, g, b) ((9 * (r) * (r) + 30 * (g) * (g) + (b) * (b) > \
                                9 * 50 * 50 + 30 * 50 * 50 + 50 * 50) ? 1 : 0)
#define SIXEL_RGB255(r, g, b) ((9 * (r) * (r) + 30 * (g) * (g) + (b) * (b) > \
                                9 * 127 * 127 + 30 * 127 * 127 + 127 * 127) ? 1 : 0)
#define CARD_HEAD_SIZE 0
#define pixel_t u_int8_t

#elif defined(SIXEL_SHAREPALETTE) /* Both sixel and system uses same palette (works on fb alone) */

#define correct_height correct_height_sharepalette
#define load_sixel_from_data load_sixel_from_data_sharepalette
#define CARD_HEAD_SIZE 0
#ifdef USE_GRF
#define pixel_t u_int16_t
#else
#define pixel_t u_int8_t
#endif

#else

#define SIXEL_RGB100(r, g, b) ((((r)*255 / 100) << 16) | (((g)*255 / 100) << 8) | ((b)*255 / 100))
#ifndef CARD_HEAD_SIZE
#ifdef GDK_PIXBUF_VERSION
#define CARD_HEAD_SIZE 0
#else
#define CARD_HEAD_SIZE 8
#endif
#endif /* CARD_HEAD_SIZE */
#define pixel_t u_int32_t

#endif /* SIXEL_1BPP */

#define PIXEL_SIZE sizeof(pixel_t)

/* --- static variables --- */

#if !defined(SIXEL_1BPP) && !defined(SIXEL_SHAREPALETTE)
static pixel_t *custom_palette;
#endif

/* --- static functions --- */

#ifndef __GET_PARAMS__
#define __GET_PARAMS__
static u_int get_params(int *params, u_int max_params, const char **p) {
  u_int count;
  const char *start;

  memset(params, 0, sizeof(int) * max_params);

  for (start = *p, count = 0; count < max_params; count++) {
    while (1) {
      if ('0' <= **p && **p <= '9') {
        params[count] = params[count] * 10 + (**p - '0');
      } else if (**p == ';') {
        (*p)++;
        break;
      } else {
        if (start == *p) {
          return 0;
        } else {
          (*p)--;

          return count + 1;
        }
      }

      (*p)++;
    }
  }

  (*p)--;

  return count;
}
#endif /* __GET_PARAMS__ */

#ifndef __READ_SIXEL_FILE__
#define __READ_SIXEL_FILE__
static char *read_sixel_file(const char *path) {
  FILE* fp;
  struct stat st;
  char * data;
  size_t len;

  if (!(fp = fopen(path, "r"))) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " failed to open %s\n.", path);
#endif

    return NULL;
  }

  fstat(fileno(fp), &st);

  /*
   * - malloc() should be used here because alloca() could return insufficient
   * memory.
   * - Cast to char* is necessary because this function can be compiled by g++.
   */
  if (!(data = (char*)malloc(st.st_size + 1))) {
    fclose(fp);

    return NULL;
  }

  len = fread(data, 1, st.st_size, fp);
  fclose(fp);
  data[len] = '\0';

  return data;
}
#endif

#ifndef __REALLOC_PIXELS_INTERN__
#define __REALLOC_PIXELS_INTERN__
static int realloc_pixels_intern(u_char **pixels, size_t new_stride, int new_height,
                                 size_t cur_stride, int cur_height, int n_copy_rows) {
  u_char *p;
  int y;

  if (new_stride < cur_stride) {
    if (new_height > cur_height) {
      /* Not supported */
#ifdef DEBUG
      bl_error_printf(BL_DEBUG_TAG
                      " Sixel width bytes is shrunk (%d->%d) but height is lengthen (%d->%d)\n",
                      cur_stride, cur_height, new_stride, new_height);
#endif

      return 0;
    } else /* if( new_height < cur_height) */ {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG
                      " Sixel data: %d bytes %d rows -> shrink %d bytes %d rows (No alloc)\n",
                      cur_stride, cur_height, new_stride, new_height);
#endif

      for (y = 1; y < n_copy_rows; y++) {
        memmove(*pixels + (y * new_stride), *pixels + (y * cur_stride), new_stride);
      }

      return 1;
    }
  }
  /* 'new_stride == cur_stride && new_height == cur_height' is checked before calling this. */
  else if (new_stride == cur_stride && new_height < cur_height) {
    /* do nothing */

    return 1;
  }

  if (new_stride > (SSIZE_MAX - CARD_HEAD_SIZE) / new_height) {
    /* integer overflow */
    return 0;
  }

  if (new_stride == cur_stride /* && new_height > cur_height */) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Sixel data: %d bytes %d rows -> realloc %d bytes %d rows\n",
                    cur_stride, cur_height, new_stride, new_height);
#endif

    /* Cast to u_char* is necessary because this function can be compiled by g++. */
    if ((p = (u_char*)realloc(*pixels - CARD_HEAD_SIZE,
                              CARD_HEAD_SIZE + new_stride * new_height))) {
      p += CARD_HEAD_SIZE;
    } else {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " realloc failed.\n.");
#endif
      return 0;
    }

    memset(p + cur_stride * cur_height, 0, new_stride * (new_height - cur_height));
  } else if (new_stride * new_height <= cur_stride * cur_height) {
    /* cur_stride < new_stride, but cur_stride > new_height */
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " Sixel data: %d bytes %d rows -> calloc %d bytes %d rows (No alloc)\n",
                    cur_stride, cur_height, new_stride, new_height);
#endif

    for (y = 1; y < n_copy_rows; y++) {
      memmove(*pixels + (y * new_stride), *pixels + (y * cur_stride), cur_stride);
      memset(*pixels + (y * new_stride) + cur_stride, 0, new_stride - cur_stride);
    }

    return 1;
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Sixel data: %d bytes %d rows -> calloc %d bytes %d rows\n",
                    cur_stride, cur_height, new_stride, new_height);
#endif

    /* Cast to u_char* is necessary because this function can be compiled by g++. */
    if ((p = (u_char*)calloc(CARD_HEAD_SIZE + new_stride * new_height, 1))) {
      p += CARD_HEAD_SIZE;
    } else {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " calloc failed.\n.");
#endif
      return 0;
    }

    for (y = 0; y < n_copy_rows; y++) {
      memcpy(p + (y * new_stride), (*pixels) + (y * cur_stride), cur_stride);
    }

    if (*pixels) {
      free((*pixels) - CARD_HEAD_SIZE);
    }
  }

  *pixels = p;

  return 1;
}
#endif

/* realloc_pixels() might not be inlined because it is called twice. */
#define realloc_pixels(pixels, new_width, new_height, cur_width, cur_height, n_copy_rows) \
  ((new_width) == (cur_width) && (new_height) == (cur_height) ? \
    1 : realloc_pixels_intern(pixels, new_width * PIXEL_SIZE, new_height, \
                              cur_width * PIXEL_SIZE, cur_height, n_copy_rows))


/*
 * Correct the height which is always multiple of 6, but this doesn't
 * necessarily work.
 */
static void correct_height(pixel_t *pixels, int width, int *height /* multiple of 6 */) {
  int x;
  int y;

  pixels += (width * (*height - 1));

  for (y = 0; y < 5; y++) {
    for (x = 0; x < width; x++) {
      if (pixels[x]) {
        return;
      }
    }

    (*height)--;
    pixels -= width;
  }
}

/*
 * load_sixel_from_file() returns at least 1024*1024 pixels memory even if
 * the actual image size is less than it.
 * It is the caller that should shrink (realloc) it.
 */
static u_char *load_sixel_from_data(const char *file_data, u_int *width_ret, u_int *height_ret) {
  const char *p = file_data;
  u_char *pixels;
  int params[6];
  u_int n; /* number of params */
  int init_width;
  int pix_x;
  int pix_y;
  int stride;
  int cur_width;
  int cur_height;
  int width;
  int height;
  int rep;
  int color;
  int asp_x;
#ifdef SIXEL_ORMODE
  int ormode = 0;
#endif
#ifdef SIXEL_SHAREPALETTE
  u_int8_t color_indexes[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
      48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
      64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
      80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
      96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
      112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
      128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
      144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
      160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
      176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
      192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
      208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
      224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
      240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 };
#else
  /* VT340 Default Color Map */
  static pixel_t default_palette[] = {
      SIXEL_RGB100(0, 0, 0),    /* BLACK */
      SIXEL_RGB100(20, 20, 80), /* BLUE */
      SIXEL_RGB100(80, 13, 13), /* RED */
      SIXEL_RGB100(20, 80, 20), /* GREEN */
      SIXEL_RGB100(80, 20, 80), /* MAGENTA */
      SIXEL_RGB100(20, 80, 80), /* CYAN */
      SIXEL_RGB100(80, 80, 20), /* YELLOW */
      SIXEL_RGB100(53, 53, 53), /* GRAY 50% */
      SIXEL_RGB100(26, 26, 26), /* GRAY 25% */
      SIXEL_RGB100(33, 33, 60), /* BLUE* */
      SIXEL_RGB100(60, 26, 26), /* RED* */
      SIXEL_RGB100(33, 60, 33), /* GREEN* */
      SIXEL_RGB100(60, 33, 60), /* MAGENTA* */
      SIXEL_RGB100(33, 60, 60), /* CYAN* */
      SIXEL_RGB100(60, 60, 33), /* YELLOW* */
      SIXEL_RGB100(80, 80, 80)  /* GRAY 75% */
  };
  pixel_t *palette;
  pixel_t *base_palette;
  pixel_t *ext_palette = NULL;
#endif

  pixels = NULL;
  init_width = 0;
  cur_width = cur_height = width = height = 0;

#ifndef SIXEL_SHAREPALETTE
#ifndef SIXEL_1BPP
  if (custom_palette) {
    base_palette = palette = custom_palette;
    if (palette[SIXEL_PALETTE_SIZE] == 0) /* No active palette */ {
      memcpy(palette, default_palette, sizeof(default_palette));
      memset(palette + 16, 0, sizeof(pixel_t) * SIXEL_PALETTE_SIZE - sizeof(default_palette));
    }
  } else
#endif
  {
    base_palette = palette = (pixel_t*)alloca(sizeof(pixel_t) * SIXEL_PALETTE_SIZE);
    memcpy(palette, default_palette, sizeof(default_palette));
    memset(palette + 16, 0, sizeof(pixel_t) * SIXEL_PALETTE_SIZE - sizeof(default_palette));
  }
#endif

restart:
  while (1) {
    if (*p == '\0') {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Illegal format\n.");
#endif

      goto end;
    } else if (*p == '\x90') {
      break;
    } else if (*p == '\x1b') {
      if (*(++p) == 'P') {
        break;
      }
    } else {
      p++;
    }
  }

  if (*(++p) != ';') {
    /* P1 */
    switch (*p) {
    case 'q':
      /* The default value. (2:1 is documented though) */
      asp_x = 1;
#if 0
      asp_y = 1;
#endif
      goto body;

#if 0
    case '0':
    case '1':
    case '5':
    case '6':
      asp_x = 1;
      asp_y = 2;
      break;

    case '2':
      asp_x = 1;
      asp_y = 5;
      break;

    case '3':
    case '4':
      asp_x = 1;
      asp_y = 3;
      break;

    case '7':
    case '8':
    case '9':
      asp_x = 1;
      asp_y = 1;
      break;

    default:
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
      goto end;
#else
    case '\0':
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
      goto end;

    default:
      asp_x = 1; /* XXX */
#endif
    }

    if (p[1] == ';') {
      p++;
    }
  } else {
    /* P1 is omitted. */
    asp_x = 1; /* V:H=2:1 */
#if 0
    asp_y = 2;
#endif
  }

  if (*(++p) != ';') {
    /* P2 */
    switch (*p) {
    case 'q':
      goto body;
#ifdef SIXEL_ORMODE
    case '5':
      ormode = 1;
      break;
#endif
#if 0
    case '0':
    case '2':
      ...
      break;

    default:
#else
    case '\0':
#endif
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
      goto end;
    }

    if (p[1] == ';') {
      p++;
    }
  }
#if 0
  else {
    /* P2 is omitted. */
  }
#endif

  /* Ignoring P3 */
  while (*(++p) != 'q') {
    if (*p == '\0') {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
      goto end;
    }
  }

body:
#ifdef SIXEL_SHAREPALETTE
  {
    char *p2;

    /* 'p + 2' is to skip '#' of "#0;9;%d;%d;%d" appended just after "DCS ... q" in vt_parser.c */
    if (!(p2 = memchr(p + 2, '#', 50)) || (p2[2] != ';' && p2[3] != ';' && p2[4] != ';')) {
      /* If color definition is not found, load_sixel_from_data_sharepalette() exits. */
      goto error;
    }
  }
#endif

  rep = asp_x;
  pix_x = pix_y = 0;
  stride = width * PIXEL_SIZE;
  color = 0;

  while (*(++p) != '\0') {
    if (*p >= '?' && *p <= '\x7E') {
      u_int new_width;
      u_int new_height;
      int a;
      int b;
      int y;
      u_char *line;

      if (height < pix_y + 6) {
        new_height = height + 516 /* 6*86 */;
      } else {
        new_height = height;
      }

      if (width < pix_x + rep) {
        u_int h;

        new_width = width + 512;
        stride += (512 * PIXEL_SIZE);
        h = width * height / new_width;
        /*
         * h=17, pix_y=6
         * h=17/6*6=12 == pix_y + 6
         */
        if (h >= pix_y + 11) {
          h = h / 6 * 6;
          new_height = h;
        }
      } else {
        new_width = width;
      }

      if (!realloc_pixels(&pixels, new_width, new_height, width, height, pix_y + 6)) {
        break;
      }

      width = new_width;
      height = new_height;

      b = *p - '?';
      a = 0x01;
      line = pixels + pix_y * stride + pix_x * PIXEL_SIZE;

#ifdef SIXEL_ORMODE
      if (ormode) {
        for (y = 0; y < 6; y++) {
          if ((b & a) != 0) {
            int x;

            for (x = 0; x < rep; x++) {
              ((pixel_t*)line)[x] |= color;
            }
          }

          a <<= 1;
          line += stride;
        }
      } else
#endif
      {
#ifdef SIXEL_SHAREPALETTE
        for (y = 0; y < 6; y++) {
          if ((b & a) != 0) {
#ifdef USE_GRF
            int x;

            for (x = 0; x < rep; x++) {
              ((u_int16_t*)line)[x] = color_indexes[color];
            }
#else
            memset(line, color_indexes[color], rep);
#endif
          }
          a <<= 1;
          line += stride;
        }
#else
        for (y = 0; y < 6; y++) {
          if ((b & a) != 0) {
            int x;

            for (x = 0; x < rep; x++) {
#ifdef SIXEL_1BPP
              /* 0x80 is opaque mark */
              ((pixel_t*)line)[x] = 0x80 | palette[color];
#else
              ((pixel_t*)line)[x] = palette[color];
#endif
            }
          }

          a <<= 1;
          line += stride;
        }
#endif /* SIXEL_SHAREPALETTE */
      }

      pix_x += rep;

      if (cur_width < pix_x) {
        cur_width = pix_x;
      }

      if (cur_height < pix_y + 6) {
        cur_height = pix_y + 6;
      }

      rep = asp_x;
    } else if (*p == '!') {
      /* ! Pn Ch */
      if (*(++p) == '\0') {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif

        break;
      }

      if (get_params(params, 1, &p) > 0) {
        if ((rep = params[0]) < 1) {
          rep = 1;
        }

        rep *= asp_x;
      }
    } else if (*p == '$' || *p == '-') {
      pix_x = 0;
      rep = asp_x;

      if (*p == '-') {
        if (!init_width && width > cur_width && cur_width > 0) {
          int y;

#ifdef DEBUG
          bl_debug_printf("Sixel width is shrunk (%d -> %d)\n", width, cur_width);
#endif

          for (y = 1; y < cur_height; y++) {
            memmove(pixels + y * cur_width * PIXEL_SIZE, pixels + y * width * PIXEL_SIZE,
                    cur_width * PIXEL_SIZE);
          }
          memset(pixels + y * cur_width * PIXEL_SIZE, 0,
                 (cur_height * (width - cur_width) * PIXEL_SIZE));

          width = cur_width;
          stride = width * PIXEL_SIZE;
          init_width = 1;
        }

        pix_y += 6;
      }
    } else if (*p == '#') /* # Pc ; Pu; Px; Py; Pz */ {
      if (*(++p) == '\0') {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
        break;
      }

      n = get_params(params, sizeof(params) / sizeof(params[0]), &p);

      if (n > 0) {
        if ((color = params[0]) >= SIXEL_PALETTE_SIZE) {
#ifdef SIXEL_SHAREPALETTE
          color = SIXEL_PALETTE_SIZE - 1;
#else
          if (color >= 1024) {
            color = 0;
            goto use_base_palette;
          } else {
            color -= SIXEL_PALETTE_SIZE;

            if (!ext_palette) {
              if (!(ext_palette = (pixel_t*)alloca(sizeof(pixel_t) *
                                                   (1024 - SIXEL_PALETTE_SIZE)))) {
                color = 0;
                goto use_base_palette;
              }
              memset(ext_palette, 0, 1024 - SIXEL_PALETTE_SIZE);
            }

            palette = ext_palette;
          }
#endif
        } else {
          if (color < 0) {
            color = 0;
          }

#ifndef SIXEL_SHAREPALETTE
        use_base_palette:
          palette = base_palette;
#endif
        }
      }

      if (n > 4) {
        int rgba[4]; /* The arguments of bl_hls_to_rgb() are 'int*' */

        rgba[3] = 255;

        if (params[1] == 1) {
          /* HLS */
          bl_hls_to_rgb(&rgba[0], &rgba[1], &rgba[2],
                        BL_MIN(params[2], 360), BL_MIN(params[3], 100), BL_MIN(params[4], 100));

          if (n > 5 && params[5] < 100) {
            rgba[3] = params[5] * 255 / 100;
          }
        } else if (params[1] == 2
#ifndef SIXEL_SHAREPALETTE
                   || params[1] == 9 /* see vt_parser.c */
#endif
                   ) {
          /* RGB */
          rgba[0] = params[2] >= 100 ? 255 : params[2] * 255 / 100;
          rgba[1] = params[3] >= 100 ? 255 : params[3] * 255 / 100;
          rgba[2] = params[4] >= 100 ? 255 : params[4] * 255 / 100;

          if (n > 5 && params[5] < 100) {
            rgba[3] = params[5] * 255 / 100;
          }
        } else if (params[1] == 3) {
          /* R8G8B8 */
          rgba[0] = params[2] >= 255 ? 255 : params[2];
          rgba[1] = params[3] >= 255 ? 255 : params[3];
          rgba[2] = params[4] >= 255 ? 255 : params[4];

          if (n > 5 && params[5] < 255) {
            rgba[3] = params[5];
          }
        } else {
          continue;
        }

#if defined(SIXEL_1BPP)
        palette[color] = SIXEL_RGB255(rgba[0], rgba[1], rgba[2]);
#elif defined(SIXEL_SHAREPALETTE)
        {
          u_int8_t r, g, b;

          /* fb/ui_display.h which defines ui_cmap_get_pixel_rgb() is included from ui_imagelib.c */
          if (!ui_cmap_get_pixel_rgb(&r, &g, &b, color) ||
              abs(r - rgba[0]) >= 0x10 || abs(g - rgba[1]) >= 0x10 || abs(b - rgba[2]) >= 0x10) {
            u_long closest;

            /*
             * fb/ui_display.h which defines ui_cmap_get_closest_color() is included
             * from ui_imagelib.c
             */
            if (ui_cmap_get_closest_color(&closest, rgba[0], rgba[1], rgba[2])) {
              color_indexes[color] = closest;
            } else {
              goto error;
            }
          }
        }
#else
#if defined(GDK_PIXBUF_VERSION) || defined(USE_QUARTZ)
        /* RGBA */
        ((u_char*)(palette + color))[0] = rgba[0];
        ((u_char*)(palette + color))[1] = rgba[1];
        ((u_char*)(palette + color))[2] = rgba[2];
        ((u_char*)(palette + color))[3] = rgba[3];
#else
        /* ARGB */
        palette[color] = (rgba[3] << 24) | (rgba[0] << 16) | (rgba[1] << 8) | rgba[2];
#endif

        if (palette == custom_palette && palette[SIXEL_PALETTE_SIZE] <= color) {
          /*
           * Set max active palette number for NetBSD/OpenBSD.
           * (See load_file() in fb/ui_imagelib.c)
           */
          palette[SIXEL_PALETTE_SIZE] = color + 1;
        }
#endif

#ifdef __DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Set rgb %x for color %d.\n", palette[color], color);
#endif
      }
    } else if (*p == '"') /* " Pan ; Pad ; Ph ; Pv */ {
      u_int new_width = 0;
      u_int new_height = 0;
      int need_resize = 0;

      if (*(++p) == '\0') {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " Illegal format.\n.");
#endif
        break;
      }

      if ((n = get_params(params, 4, &p)) == 1) {
        params[1] = 1;
        n = 2;
      }

      switch (n) {
      case 4:
        new_height = (params[3] + 5) / 6 * 6;
      case 3:
        new_width = params[2];
#if 0
      case 2:
        /* V:H=params[0]:params[1] */
#if 0
        asp_x = params[1];
        asp_y = params[0];
#else
        rep /= asp_x;
        if ((asp_x = params[1] / params[0]) == 0) {
          asp_x = 1; /* XXX */
        }
        rep *= asp_x;
#endif
#endif
      }

      if (width < new_width) {
        /* w200 h100 -> w400 h0 case is ignored. */
        if (new_height > 0) {
          need_resize = 1;
          if (height > new_height) {
            new_height = height;
          }
        }
      }

      if (height < new_height) {
        /* w200 h100 -> w0 h200 case is ignored. */
        if (new_width > 0) {
          need_resize = 1;
          if (width > new_width) {
            new_width = width;
          }
        }
      }

      if (need_resize &&
          realloc_pixels(&pixels, new_width, new_height, width, height, pix_y + 6)) {
        width = new_width;
        stride = new_width * PIXEL_SIZE;
        height = new_height;
      }
    } else if (*p == '\x1b') {
      if (*(++p) == '\\') {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " EOF.\n.");
#endif

        if (*(p + 1) != '\0') {
          goto restart;
        }

        break;
      } else if (*p == '\0') {
        break;
      }
    } else if (*p == '\x9c') {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " EOF.\n.");
#endif

      if (*(p + 1) != '\0') {
        goto restart;
      }

      break;
    }
  }

end:
#if !defined(SIXEL_1BPP) && !defined(SIXEL_SHAREPALETTE)
  custom_palette = NULL;
#endif

  if (cur_width > 0 && realloc_pixels(&pixels, cur_width, cur_height, width, height, cur_height)) {
    correct_height((pixel_t*)pixels, cur_width, &cur_height);

#ifdef DEBUG
    bl_debug_printf("Shrink size w %d h %d -> w %d h %d\n", width, height, cur_width, cur_height);
#endif

#ifdef SIXEL_ORMODE
    if (ormode) {
      int x, y;
      pixel_t *p = pixels;

      for (y = 0; y < cur_height; y++) {
        for (x = 0; x < cur_width; x++) {
#ifdef SIXEL_SHAREPALETTE
          *p = color_indexes[*p];
#else
          *p = palette[*p];
          p++;
#endif
        }
      }
    }
#endif

    *width_ret = cur_width;
    *height_ret = cur_height;

    return pixels;
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Nothing is drawn.\n");
#endif

#ifdef SIXEL_SHAREPALETTE
error:
#endif
  free(pixels - CARD_HEAD_SIZE);

  return NULL;
}

#if !defined(SIXEL_1BPP) && !defined(SIXEL_SHAREPALETTE)

static u_char *load_sixel_from_file(const char *path, u_int *width_ret, u_int *height_ret) {
  char *file_data;
  u_char *pixels;

  if ((file_data = read_sixel_file(path))) {
    pixels = load_sixel_from_data(file_data, width_ret, height_ret);
    free(file_data);

    return pixels;
  } else {
    return NULL;
  }
}

pixel_t *ui_set_custom_sixel_palette(pixel_t *palette /* NULL -> Create new palette */
                                     ) {
  if (!palette) {
    palette = (pixel_t*)calloc(sizeof(pixel_t), SIXEL_PALETTE_SIZE + 1);
  }

  return (custom_palette = palette);
}

#endif

#undef realloc_pixels
#undef correct_height
#undef load_sixel_from_data
#undef SIXEL_RGB100
#undef SIXEL_RGB255
#undef CARD_HEAD_SIZE
#undef pixel_t
#undef PIXEL_SIZE
