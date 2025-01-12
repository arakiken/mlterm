/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef NO_IMAGE

#ifdef COCOA_TOUCH
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "../ui_imagelib.h"

#include <stdio.h> /* sprintf */
#include <math.h>  /* pow */
#include <strings.h> /* strcasecmp */

#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_conf_io.h> /* bl_get_user_rc_path */

#include "cocoa.h"

#if 1
#define BUILTIN_SIXEL
#endif

/* --- static functions --- */

#define CARD_HEAD_SIZE 0
#include "../../common/c_sixel.c"
#include "../../common/c_regis.c"
#include "../../common/c_animgif.c"

static void value_table_refresh(u_char *value_table, /* 256 bytes */
                                ui_picture_modifier_t *mod) {
  int i, tmp;
  double real_gamma, real_brightness, real_contrast;

  real_gamma = (double)(mod->gamma) / 100;
  real_contrast = (double)(mod->contrast) / 100;
  real_brightness = (double)(mod->brightness) / 100;

  for (i = 0; i < 256; i++) {
    tmp = real_contrast * (255 * pow(((double)i + 0.5) / 255, real_gamma) - 128) +
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

static void adjust_pixmap(u_char *image, u_int width, u_int height,
                          ui_picture_modifier_t *pic_mod) {
  u_char *value_table;
  u_int y;
  u_int x;
  u_char r, g, b, a;
  u_int32_t pixel;

  if (!ui_picture_modifier_is_normal(pic_mod) && (value_table = alloca(256))) {
    value_table_refresh(value_table, pic_mod);
  } else {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      pixel = *(((u_int32_t *)image) + (y * width + x));

      a = (pixel >> 24) & 0xff;
      r = (pixel >> 16) & 0xff;
      g = (pixel >> 8) & 0xff;
      b = pixel & 0xff;

      r = (value_table[r] * (255 - pic_mod->alpha) + pic_mod->blend_red * pic_mod->alpha) / 255;
      g = (value_table[g] * (255 - pic_mod->alpha) + pic_mod->blend_green * pic_mod->alpha) / 255;
      b = (value_table[b] * (255 - pic_mod->alpha) + pic_mod->blend_blue * pic_mod->alpha) / 255;

      pixel = (a << 24) | (r << 16) | (g << 8) | b;
      *(((u_int32_t *)image) + (y * width + x)) = pixel;
    }
  }
}

static void adjust_cgimage(u_char *image, u_int width, u_int height,
                           ui_picture_modifier_t *pic_mod) {
  u_char *value_table;
  u_int y;
  u_int x;
  u_char r, g, b;

  if (!ui_picture_modifier_is_normal(pic_mod) && (value_table = alloca(256))) {
    value_table_refresh(value_table, pic_mod);
  } else {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      r = image[0];
      g = image[1];
      b = image[2];

      image[0] =
          (value_table[r] * (255 - pic_mod->alpha) + pic_mod->blend_red * pic_mod->alpha) / 255;
      image[1] =
          (value_table[g] * (255 - pic_mod->alpha) + pic_mod->blend_green * pic_mod->alpha) / 255;
      image[2] =
          (value_table[b] * (255 - pic_mod->alpha) + pic_mod->blend_blue * pic_mod->alpha) / 255;

      image += 4;
    }
  }
}

static int check_has_alpha(u_char *image, u_int width, u_int height) {
  u_int x;
  u_int y;
  u_char *p = image + 3;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++, p += 4) {
      if (*p < 0x80) {
        return 1;
      }
    }
  }

  return 0;
}

static int load_file(char *path, /* must be UTF-8 */
                     u_int *width, u_int *height, ui_picture_modifier_t *pic_mod,
                     Pixmap *pixmap, PixmapMask *mask, int *transparent) {
  char *suffix;
  u_char *image;
  CGImageAlphaInfo info;

  suffix = path + strlen(path) - 4;
#ifdef BUILTIN_SIXEL
  if (strcasecmp(suffix, ".six") == 0 && *width == 0 && *height == 0 &&
      (image = load_sixel_from_file(path, width, height, transparent))) {
    adjust_pixmap(image, *width, *height, pic_mod);

    info = check_has_alpha(image, *width, *height) ? kCGImageAlphaPremultipliedLast
                                                   : kCGImageAlphaNoneSkipLast;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(image, *width, *height, 8, *width * 4, cs, info);
    CGColorSpaceRelease(cs);

    *pixmap = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
  } else
#endif
  {
    if (strcmp(suffix, ".gif") == 0) {
      char *dir;

      if (!strstr(path, "mlterm/anim") && /* is animation gif */
          (dir = bl_get_user_rc_path("mlterm/"))) {
        split_animation_gif(path, dir, hash_path(path));
        free(dir);
      }
    } else if (strcasecmp(suffix, ".rgs") == 0) {
      convert_regis_to_bmp(path);
    }

    /* XXX pic_mod is ignored */
    if (!(*pixmap = cocoa_load_image(path, width, height))) {
      return 0;
    }

    if (transparent) {
      *transparent = 0;
    }

    info = CGImageGetAlphaInfo(*pixmap);

    if (!ui_picture_modifier_is_normal(pic_mod)) {
      CGDataProviderRef provider = CGImageGetDataProvider(*pixmap);
      CFDataRef data = CGDataProviderCopyData(provider);
      image = (u_char *)CFDataGetBytePtr(data);

      adjust_cgimage(image, *width, *height, pic_mod);

      CFDataRef new_data = CFDataCreate(NULL, image, CFDataGetLength(data));
      CGDataProviderRef new_provider = CGDataProviderCreateWithCFData(new_data);
      CGImageRef new_image = CGImageCreate(
          *width, *height, CGImageGetBitsPerComponent(*pixmap), CGImageGetBitsPerPixel(*pixmap),
          CGImageGetBytesPerRow(*pixmap), CGImageGetColorSpace(*pixmap),
          CGImageGetBitmapInfo(*pixmap), new_provider, NULL, CGImageGetShouldInterpolate(*pixmap),
          CGImageGetRenderingIntent(*pixmap));
      CGImageRelease(*pixmap);
      CFRelease(new_provider);
      CFRelease(data);
      CFRelease(new_data);

      *pixmap = new_image;
    }
  }

  if (info == kCGImageAlphaPremultipliedLast || info == kCGImageAlphaPremultipliedFirst ||
      info == kCGImageAlphaLast || info == kCGImageAlphaFirst) {
    /* dummy (If cur_pic->mask is non-zero, need_clear = 1 in draw_picture() in ui_draw_str.c) */
    *mask = (PixmapMask)1L;
  }

  return 1;
}

static Pixmap resize(Pixmap pixmap, u_int new_width, u_int new_height) {
  CGColorSpaceRef cs = CGImageGetColorSpace(pixmap);
  CGContextRef ctx = CGBitmapContextCreate(NULL, new_width, new_height,
                                           CGImageGetBitsPerComponent(pixmap),
                                           0, cs, CGImageGetAlphaInfo(pixmap));
  CGContextDrawImage(ctx, CGRectMake(0, 0, new_width, new_height), pixmap);
  CGImageRelease(pixmap);
  Pixmap resized = CGBitmapContextCreateImage(ctx);
  CGContextRelease(ctx);

  return resized;
}

/* --- global functions --- */

void ui_imagelib_display_opened(Display *display) {}

void ui_imagelib_display_closed(Display *display) {}

Pixmap ui_imagelib_load_file_for_background(ui_window_t *win, char *path,
                                            ui_picture_modifier_t *pic_mod) {
  Pixmap pixmap;
  u_int width = 0;
  u_int height = 0;

  if (!load_file(path, &width, &height, pic_mod, &pixmap, NULL, NULL)) {
    return None;
  }

  if (ACTUAL_WIDTH(win) != width || ACTUAL_HEIGHT(win) != height) {
    pixmap = resize(pixmap, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win));
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
  u_int pix_width = 0;
  u_int pix_height = 0;

  if (cardinal) {
    return 0;
  }

  if (!load_file(path, &pix_width, &pix_height, NULL, pixmap, mask, transparent)) {
    return 0;
  }

  if ((*width > 0 && pix_width != *width) || (*height > 0 && pix_height != *height)) {
    *pixmap = resize(*pixmap, *width, *height);
  } else {
    *width = pix_width;
    *height = pix_height;
  }

  return 1;
}

void ui_destroy_image(Display *display, Pixmap pixmap) {
  CGImageRelease(pixmap);
}

void ui_destroy_mask(Display *display, PixmapMask mask /* can be NULL */) {}

#endif /* NO_IMAGE */
