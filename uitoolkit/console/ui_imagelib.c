/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef NO_IMAGE

#include "../ui_imagelib.h"

#include <stdio.h>  /* sprintf */
#include <unistd.h> /* write , STDIN_FILENO */
#ifdef DLOPEN_LIBM
#include <pobl/bl_dlfcn.h> /* dynamically loading pow */
#else
#include <math.h> /* pow */
#endif
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_path.h> /* BL_LIBEXECDIR */

#include "ui_display.h" /* ui_cmap_get_closest_color */

/* Trailing "/" is appended in value_table_refresh(). */
#ifndef LIBMDIR
#define LIBMDIR "/lib"
#endif

#if 1
#define BUILTIN_SIXEL
#endif

/* --- static functions --- */

static void value_table_refresh(u_char *value_table, /* 256 bytes */
                                ui_picture_modifier_t *mod) {
  int i, tmp;
  double real_gamma, real_brightness, real_contrast;
  static double (*pow_func)(double, double);

  real_gamma = (double)(mod->gamma) / 100;
  real_contrast = (double)(mod->contrast) / 100;
  real_brightness = (double)(mod->brightness) / 100;

  if (!pow_func) {
#ifdef DLOPEN_LIBM
    bl_dl_handle_t handle;

    if ((!(handle = bl_dl_open(LIBMDIR "/", "m")) && !(handle = bl_dl_open("", "m"))) ||
        !(pow_func = bl_dl_func_symbol(handle, "pow"))) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Failed to load pow in libm.so\n");
#endif

      if (handle) {
        bl_dl_close(handle);
      }

      /*
       * gamma, contrast and brightness options are ignored.
       * (alpha option still survives.)
       */
      for (i = 0; i < 256; i++) {
        value_table[i] = i;
      }

      return;
    }

    bl_dl_close_at_exit(handle);
#else  /* DLOPEN_LIBM */
    pow_func = pow;
#endif /* DLOPEN_LIBM */
  }

  for (i = 0; i < 256; i++) {
    tmp = real_contrast * (255 * (*pow_func)(((double)i + 0.5) / 255, real_gamma) - 128) +
          128 * real_brightness;
    if (tmp >= 255) {
      break;
    } else if (tmp < 0) {
      value_table[i] = 0;
    } else {
      value_table[i] = tmp;
    }
  }

  for (; i < 256; i++) {
    value_table[i] = 255;
  }
}

static void modify_pixmap(Display *display, Pixmap pixmap, ui_picture_modifier_t *pic_mod,
                          u_int depth) {
  u_char *value_table;
  u_int32_t *src;
  u_char *dst;
  u_int num_pixels;
  u_int count;
  u_char r, g, b, a;
  u_long pixel;

  if (!ui_picture_modifier_is_normal(pic_mod) && (value_table = alloca(256))) {
    value_table_refresh(value_table, pic_mod);
  } else {
    value_table = NULL;
  }

  src = dst = pixmap->image;
  num_pixels = pixmap->width * pixmap->height;

  for (count = 0; count < num_pixels; count++) {
    pixel = *(src++);

    a = (pixel >> 24) & 0xff;
    r = (pixel >> 16) & 0xff;
    g = (pixel >> 8) & 0xff;
    b = pixel & 0xff;

    if (value_table) {
      r = (value_table[r] * (255 - pic_mod->alpha) + pic_mod->blend_red * pic_mod->alpha) / 255;
      g = (value_table[g] * (255 - pic_mod->alpha) + pic_mod->blend_green * pic_mod->alpha) / 255;
      b = (value_table[b] * (255 - pic_mod->alpha) + pic_mod->blend_blue * pic_mod->alpha) / 255;
    }

    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;

    dst += 4;
  }
}

#ifdef BUILTIN_SIXEL

#include <string.h>
#include <pobl/bl_util.h>
#include <pobl/bl_def.h> /* SSIZE_MAX */

/*
 * This function resizes the sixel image to the specified size and shrink
 * pixmap->image.
 * It frees pixmap->image in failure.
 * Call resize_sixel() after load_sixel_from_file() because it returns at least
 * 1024*1024 pixels memory even if the actual image size is less than 1024*1024.
 */
static int resize_sixel(Pixmap pixmap, u_int width, u_int height, u_int bytes_per_pixel) {
  void *p;
  size_t line_len;
  size_t old_line_len;
  size_t image_len;
  size_t old_image_len;
  u_char *dst;
  u_char *src;
  int y;
  u_int min_height;

  p = NULL;

  if ((width == 0 || width == pixmap->width) && (height == 0 || height == pixmap->height)) {
    goto end;
  }

  if (width > SSIZE_MAX / bytes_per_pixel / height) {
    goto error;
  }

  old_line_len = pixmap->width * bytes_per_pixel;
  line_len = width * bytes_per_pixel;
  image_len = line_len * height;
  old_image_len = old_line_len * pixmap->height;

  if (image_len > old_image_len) {
    if (!(p = realloc(pixmap->image, image_len))) {
      goto error;
    }

    pixmap->image = p;
  }

  /* Tiling */

  min_height = BL_MIN(height, pixmap->height);

  if (width > pixmap->width) {
    size_t surplus;
    u_int num_copy;
    u_int count;
    u_char *dst_next;

    y = min_height - 1;
    src = pixmap->image + old_line_len * y;
    dst = pixmap->image + line_len * y;

    surplus = line_len % old_line_len;
    num_copy = line_len / old_line_len - 1;

    for (; y >= 0; y--) {
      dst_next = memmove(dst, src, old_line_len);

      for (count = num_copy; count > 0; count--) {
        memcpy((dst_next += old_line_len), dst, old_line_len);
      }
      memcpy(dst_next + old_line_len, dst, surplus);

      dst -= line_len;
      src -= old_line_len;
    }
  } else if (width < pixmap->width) {
    src = pixmap->image + old_line_len;
    dst = pixmap->image + line_len;

    for (y = 1; y < min_height; y++) {
      memmove(dst, src, old_line_len);
      dst += line_len;
      src += old_line_len;
    }
  }

  if (height > pixmap->height) {
    y = pixmap->height;
    src = pixmap->image;
    dst = src + line_len * y;

    for (; y < height; y++) {
      memcpy(dst, src, line_len);
      dst += line_len;
      src += line_len;
    }
  }

  bl_msg_printf("Resize sixel from %dx%d to %dx%d\n", pixmap->width, pixmap->height, width, height);

  pixmap->width = width;
  pixmap->height = height;

end:
  /* Always realloate pixmap->image according to its width, height and
   * bytes_per_pixel. */
  if (!p && (p = realloc(pixmap->image, pixmap->width * pixmap->height * bytes_per_pixel))) {
    pixmap->image = p;
  }

  return 1;

error:
  free(pixmap->image);

  return 0;
}

#define CARD_HEAD_SIZE 0
#include "../../common/c_sixel.c"

#endif /* BUILTIN_SIXEL */

static int load_file(Display *display, char *path, u_int width, u_int height, int keep_aspect,
                     ui_picture_modifier_t *pic_mod, u_int depth, Pixmap *pixmap,
                     PixmapMask *mask, int *transparent) {
  pid_t pid;
  int fds[2];
  ssize_t size;
  u_int32_t tmp;

  if (!path || !*path) {
    return 0;
  }

#ifdef BUILTIN_SIXEL
  if (strcasecmp(path + strlen(path) - 4, ".six") == 0 &&
/* For old machines and Android (not to use mlimgloader) */
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__ANDROID__)
      width == 0 && height == 0 &&
#endif
      (*pixmap = calloc(1, sizeof(**pixmap)))) {
    if (((*pixmap)->image = load_sixel_from_file(path, &(*pixmap)->width, &(*pixmap)->height,
                                                 NULL)) &&
        /* resize_sixel() frees pixmap->image in failure. */
        resize_sixel(*pixmap, width, height, 4)) {
      goto loaded;
    } else {
      free(*pixmap);
    }
  }
#endif

  if (transparent) {
    *transparent = 0;
  }

#ifdef __ANDROID__
  if (!(*pixmap = calloc(1, sizeof(**pixmap)))) {
    return 0;
  }

  (*pixmap)->width = width;
  (*pixmap)->height = height;
  if (!((*pixmap)->image = ui_display_get_bitmap(path, &(*pixmap)->width, &(*pixmap)->height))) {
    free(*pixmap);

    return 0;
  }
#else
  if (pipe(fds) == -1) {
    return 0;
  }

  pid = fork();
  if (pid == -1) {
    close(fds[0]);
    close(fds[1]);

    return 0;
  }

  if (pid == 0) {
    /* child process */

    char *args[8];
    char width_str[DIGIT_STR_LEN(u_int) + 1];
    char height_str[DIGIT_STR_LEN(u_int) + 1];

    args[0] = BL_LIBEXECDIR("mlterm") "/mlimgloader";
    args[1] = "0";
    sprintf(width_str, "%u", width);
    args[2] = width_str;
    sprintf(height_str, "%u", height);
    args[3] = height_str;
    args[4] = path;
    args[5] = "stdout";
    if (keep_aspect) {
      args[6] = "-a";
      args[7] = NULL;
    } else {
      args[6] = NULL;
    }

    close(fds[0]);
    if (dup2(fds[1], STDOUT_FILENO) != -1) {
      execv(args[0], args);
    }

    bl_msg_printf("Failed to exec %s.\n", args[0]);

    exit(1);
  }

  close(fds[1]);

  if (!(*pixmap = calloc(1, sizeof(**pixmap)))) {
    goto error;
  }

  if (read(fds[0], &tmp, sizeof(u_int32_t)) != sizeof(u_int32_t)) {
    goto error;
  }

  size = ((*pixmap)->width = tmp) * sizeof(u_int32_t);

  if (read(fds[0], &tmp, sizeof(u_int32_t)) != sizeof(u_int32_t)) {
    goto error;
  }

  size *= ((*pixmap)->height = tmp);

  if (!((*pixmap)->image = malloc(size))) {
    goto error;
  } else {
    u_char *p;
    ssize_t n_rd;

    p = (*pixmap)->image;
    while ((n_rd = read(fds[0], p, size)) > 0) {
      p += n_rd;
      size -= n_rd;
    }

    if (size > 0) {
      goto error;
    }
  }

  close(fds[0]);
#endif

loaded:
  if (mask) {
    *mask = None;
  }

  modify_pixmap(display, *pixmap, pic_mod, depth);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s(w %d h %d) is loaded%s.\n", path, (*pixmap)->width,
                  (*pixmap)->height, (mask && *mask) ? " (has mask)" : "");
#endif

  return 1;

error:
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Failed to load %s\n", path);
#endif

  if (*pixmap) {
    free((*pixmap)->image);
    free(*pixmap);
  }

  close(fds[0]);

  return 0;
}

/* --- global functions --- */

void ui_imagelib_display_opened(Display *display) {}

void ui_imagelib_display_closed(Display *display) {}

Pixmap ui_imagelib_load_file_for_background(ui_window_t *win, char *path,
                                            ui_picture_modifier_t *pic_mod) {
  Pixmap pixmap;

  if (!load_file(win->disp->display, path, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win), 0, pic_mod,
                 win->disp->depth, &pixmap, NULL, NULL)) {
    pixmap = None;
  }

  return pixmap;
}

int ui_imagelib_root_pixmap_available(Display *display) { return 0; }

Pixmap ui_imagelib_get_transparent_background(ui_window_t *win, ui_picture_modifier_t *pic_mod) {
  return None;
}

int ui_imagelib_load_file(ui_display_t *disp, char *path, int keep_aspect, u_int32_t **cardinal,
                          Pixmap *pixmap, PixmapMask *mask, u_int *width, u_int *height,
                          int *transparent) {
  if (cardinal) {
    return 0;
  }

  if (!load_file(disp->display, path, *width, *height, keep_aspect, NULL,
                 disp->depth, pixmap, mask, transparent)) {
    return 0;
  }

  if (*width == 0 || *height == 0 || keep_aspect) {
    *width = (*pixmap)->width;
    *height = (*pixmap)->height;
  }

  return 1;
}

void ui_destroy_image(Display *display, Pixmap pixmap) {
  free(pixmap->image);
  free(pixmap);
}

void ui_destroy_mask(Display *display, PixmapMask mask /* can be NULL */) {
  if (mask) {
    free(mask);
  }
}

#endif /* NO_IMAGE */
