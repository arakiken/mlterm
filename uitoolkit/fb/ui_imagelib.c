/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef NO_IMAGE

#include "../ui_imagelib.h"

#include <stdio.h>  /* sprintf */
#include <unistd.h> /* write , STDIN_FILENO */
#if !defined(USE_WIN32API) && !defined(__ANDROID__)
#include <sys/wait.h> /* waitpid */
#endif
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

#ifdef USE_GRF
#define BPP_PSEUDO 2
#else
#define BPP_PSEUDO 1
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
  u_char r, g, b;
  u_long pixel;

  if (!ui_picture_modifier_is_normal(pic_mod) && (value_table = alloca(256))) {
    value_table_refresh(value_table, pic_mod);
  } else if (display->bytes_per_pixel == 4 &&
             /* RRGGBB */
             display->rgbinfo.r_offset == 16 && display->rgbinfo.g_offset == 8 &&
             display->rgbinfo.b_offset == 0) {
    return;
  } else {
    value_table = NULL;
  }

  src = dst = pixmap->image;
  num_pixels = pixmap->width * pixmap->height;

  for (count = 0; count < num_pixels; count++) {
    pixel = *(src++);

    r = (pixel >> 16) & 0xff;
    g = (pixel >> 8) & 0xff;
    b = pixel & 0xff;

    if (value_table) {
      r = (value_table[r] * (255 - pic_mod->alpha) + pic_mod->blend_red * pic_mod->alpha) / 255;
      g = (value_table[g] * (255 - pic_mod->alpha) + pic_mod->blend_green * pic_mod->alpha) / 255;
      b = (value_table[b] * (255 - pic_mod->alpha) + pic_mod->blend_blue * pic_mod->alpha) / 255;
    }

    if (ui_cmap_get_closest_color(&pixel, r, g, b)) {
#ifdef USE_GRF
      *((u_int16_t *)dst) = pixel;
      dst += 2;
#else
      *(dst++) = pixel;
#endif
    } else {
      pixel = RGB_TO_PIXEL(r, g, b, display->rgbinfo) | 
              ALPHA_TO_PIXEL((pixel >> 24) & 0xff, display->rgbinfo, depth);

      if (display->bytes_per_pixel == 2) {
        *((u_int16_t *)dst) = pixel;
        dst += 2;
      } else /* if( display->bytes_per_pixel == 4) */
      {
        *((u_int32_t *)dst) = pixel;
        dst += 4;
      }
    }
  }

  if (display->bytes_per_pixel < 4) {
    void *p;

    if ((p = realloc(pixmap->image, pixmap->width * pixmap->height * display->bytes_per_pixel))) {
      pixmap->image = p;
    }
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


/* For old machines (not to use mlimgloader) */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && !defined(USE_GRF)

#define SIXEL_1BPP
#include "../../common/c_sixel.c"
#undef SIXEL_1BPP

/* depth should be checked by the caller. */
static int load_sixel_with_mask_from_data_1bpp(char *file_data, u_int width, u_int height,
                                               Pixmap *pixmap, PixmapMask *mask,
                                               int *transparent) {
  int x;
  int y;
  u_char *src;
#if 0
  u_char *dst;
#endif

  if (!(*pixmap = calloc(1, sizeof(**pixmap)))) {
    return 0;
  }

  if (!((*pixmap)->image =
        load_sixel_from_data_1bpp(file_data, &(*pixmap)->width, &(*pixmap)->height, transparent)) ||
      /* resize_sixel() frees pixmap->image in failure. */
      !resize_sixel(*pixmap, width, height, 1)) {
    free(*pixmap);

    return 0;
  }

  src = (*pixmap)->image;

#if 0
  if (mask && (dst = *mask = calloc(1, (*pixmap)->width * (*pixmap)->height))) {
    int has_tp;

    has_tp = 0;

    for (y = 0; y < (*pixmap)->height; y++) {
      for (x = 0; x < (*pixmap)->width; x++) {
        if (*src >= 0x80) {
          *dst = 1;
          /* clear opaque mark */
          *src &= 0x7f;
        } else {
          has_tp = 1;
        }

        src++;
        dst++;
      }
    }

    if (!has_tp) {
      free(*mask);
      *mask = None;
    }
  } else {
    for (y = 0; y < (*pixmap)->height; y++) {
      for (x = 0; x < (*pixmap)->width; x++) {
        /* clear opaque mark */
        *(src++) &= 0x7f;
      }
    }
  }
#else
  {
    u_char bg_color;
    u_char *p;

    if (mask) {
      *mask = None;
    }

    bg_color = 0;
    p = src;

    /* Guess the current screen background color. */
    for (y = 0; y < (*pixmap)->height; y++) {
      for (x = 0; x < (*pixmap)->width; x++) {
        if (*p >= 0x80) {
          bg_color = (((*p) & 0x7f) == 1) ? 0 : 1;
          break;
        }

        p++;
      }
    }

    for (y = 0; y < (*pixmap)->height; y++) {
      for (x = 0; x < (*pixmap)->width; x++) {
        if (*src >= 0x80) {
          /* clear opaque mark */
          *(src++) &= 0x7f;
        } else {
          /* replace transparent pixel by the background color */
          *(src++) = bg_color;
        }
      }
    }
  }
#endif

  return 1;
}
#endif
#endif /* BUILTIN_SIXEL */

#define SIXEL_SHAREPALETTE
#include "../../common/c_sixel.c"
#undef SIXEL_SHAREPALETTE

#if defined(USE_WIN32API)
#include <windows.h>

static int exec_mlimgloader(char *path, u_int width, u_int height, int keep_aspect,
                            Pixmap *pixmap) {
  HANDLE input_write;
  HANDLE input_read_tmp;
  HANDLE input_read;
  SECURITY_ATTRIBUTES sa;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  char *cmd_line;
  DWORD len;
  ssize_t size;
  u_int32_t tmp;

  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  if (!CreatePipe(&input_read_tmp, &input_write, &sa, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreatePipe() failed.\n");
#endif

    return 0;
  }

  if (!DuplicateHandle(GetCurrentProcess(), input_read_tmp, GetCurrentProcess(),
                       &input_read /* Address of new handle. */,
                       0, FALSE /* Make it uninheritable. */,
                       DUPLICATE_SAME_ACCESS)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " DuplicateHandle() failed.\n");
#endif

    CloseHandle(input_read_tmp);

    goto error1;
  }

  CloseHandle(input_read_tmp);

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
  si.hStdOutput = input_write;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  /*
   * Use this if you want to hide the child:
   *  si.wShowWindow = SW_HIDE;
   * Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
   * use the wShowWindow flags.
   */

  if ((cmd_line = alloca(23 + DIGIT_STR_LEN(u_int) * 2 + strlen(path) + 1)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca failed.\n");
#endif

    goto error1;
  }

  sprintf(cmd_line, "%s 0 %d %d %s stdout%s", "mlimgloader.exe", width, height, path,
          keep_aspect ? " -a" : "");

  if (!CreateProcess("mlimgloader.exe", cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
                     &si, &pi)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " CreateProcess() failed.\n");
#endif

    goto error1;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  CloseHandle(input_write);

  if (!(*pixmap = calloc(1, sizeof(**pixmap)))) {
    goto error2;
  }

  if (!ReadFile(input_read, &tmp, sizeof(u_int32_t), &len, NULL) || len != sizeof(u_int32_t)) {
    goto error2;
  }

  size = ((*pixmap)->width = tmp) * sizeof(u_int32_t);

  if (!ReadFile(input_read, &tmp, sizeof(u_int32_t), &len, NULL) || len != sizeof(u_int32_t)) {
    goto error2;
  }

  size *= ((*pixmap)->height = tmp);

  if (!((*pixmap)->image = malloc(size))) {
    goto error2;
  } else {
    u_char *p;

    p = (*pixmap)->image;
    do {
      if (!ReadFile(input_read, p, size, &len, NULL)) {
        free((*pixmap)->image);
        goto error2;
      }
      p += len;
    } while ((size -= len) > 0);
  }

  CloseHandle(input_read);

  return 1;

error1:
  CloseHandle(input_write);
  CloseHandle(input_read);

  return 0;

error2:
  free(*pixmap);
  CloseHandle(input_read);

  return 0;
}

#elif defined(__ANDROID__)

static int exec_mlimgloader(char *path, u_int width, u_int height, int keep_aspect,
                            Pixmap *pixmap) {
  if (!(*pixmap = calloc(1, sizeof(**pixmap)))) {
    return 0;
  }

  (*pixmap)->width = width;
  (*pixmap)->height = height;

  if (!((*pixmap)->image = ui_display_get_bitmap(path, &(*pixmap)->width, &(*pixmap)->height))) {
    free(*pixmap);

    return 0;
  }

  return 1;
}

#else

static int exec_mlimgloader(char *path, u_int width, u_int height, int keep_aspect,
                            Pixmap *pixmap) {
  pid_t pid;
  int fds[2];
  ssize_t size;
  u_int32_t tmp;

  if (pipe(fds) == -1) {
    return 0;
  }

  pid = fork();
  if (pid == -1) {
    close(fds[0]);
    close(fds[0]);

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

  /* bl_pty_fork() in vt_pty_unix_new() may block without this in startup. */
#if 1
  waitpid(pid, NULL, 0);
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

#endif

static int load_file(Display *display, char *path, u_int width, u_int height, int keep_aspect,
                     ui_picture_modifier_t *pic_mod, u_int depth, Pixmap *pixmap,
                     PixmapMask *mask, int *transparent) {
  if (!path || !*path) {
    return 0;
  }

#ifdef BUILTIN_SIXEL
  if (strcasecmp(path + strlen(path) - 4, ".six") == 0) {
    char *file_data;

    if (!(file_data = read_sixel_file(path))) {
      return 0;
    }

    /* For old machines */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && !defined(USE_GRF)
    if (depth == 1) {
      /* pic_mod is ignored. */
      if (load_sixel_with_mask_from_data_1bpp(file_data, width, height, pixmap, mask,
                                              transparent)) {
        free(file_data);
        return 1;
      }
    } else
#endif
    if (
/* For old machines and Android (not to use mlimgloader) */
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__ANDROID__) && \
    (!defined(__FreeBSD__) || !defined(PC98))
        width == 0 && height == 0 &&
#endif
        (*pixmap = calloc(1, sizeof(**pixmap)))) {
#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
      u_int32_t *sixel_cmap;

      if (ui_display_is_changeable_cmap()) {
        goto skip_sharepalette;
      }
#endif

      if (depth <= 8) {
        if (ui_picture_modifier_is_normal(pic_mod) /* see modify_pixmap() */) {
          if (((*pixmap)->image = load_sixel_from_data_sharepalette(file_data, &(*pixmap)->width,
                                                                    &(*pixmap)->height,
                                                                    transparent)) &&
              resize_sixel(*pixmap, width, height, BPP_PSEUDO)) {
            if (mask) {
              *mask = NULL;
            }
            free(file_data);

            goto loaded_nomodify;
          }
        }

        bl_msg_printf("Use closest colors for %s.\n", path);
      }

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
    skip_sharepalette:
      if (!(sixel_cmap = custom_palette) &&
          (sixel_cmap = alloca(sizeof(*sixel_cmap) * (SIXEL_PALETTE_SIZE + 1)))) {
        sixel_cmap[SIXEL_PALETTE_SIZE] = 0; /* No active palette */
        custom_palette = sixel_cmap;
      }
#endif

      if (((*pixmap)->image = load_sixel_from_data(file_data, &(*pixmap)->width,
                                                   &(*pixmap)->height, transparent)) &&
          /* resize_sixel() frees pixmap->image in failure. */
          resize_sixel(*pixmap, width, height, 4)) {
#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
        if (sixel_cmap) {
          /* see set_wall_picture() in ui_screen.c */
          ui_display_set_cmap(sixel_cmap, sixel_cmap[SIXEL_PALETTE_SIZE]);
        }
#endif
        free(file_data);

        goto loaded;
      } else {
        free(*pixmap);
      }
    }

    free(file_data);
  }
#endif /* BUILTIN_SIXEL */

  if (transparent) {
    *transparent = 0;
  }

  if (!exec_mlimgloader(path, width, height, keep_aspect, pixmap)) {
    return 0;
  }

loaded:
  if (mask) {
    u_char *dst;

    if ((dst = *mask = calloc(1, (*pixmap)->width * (*pixmap)->height))) {
      int x;
      int y;
      int has_tp;
      u_int32_t *src;

      has_tp = 0;
      src = (u_int32_t *)(*pixmap)->image;

      for (y = 0; y < (*pixmap)->height; y++) {
        for (x = 0; x < (*pixmap)->width; x++) {
          if (*(src++) >= 0x80000000) {
            *dst = 1;
          } else {
            has_tp = 1;
          }

          dst++;
        }
      }

      if (!has_tp) {
        free(*mask);
        *mask = None;
      }
    }
  }

  modify_pixmap(display, *pixmap, pic_mod, depth);

loaded_nomodify:
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s(w %d h %d) is loaded%s.\n", path, (*pixmap)->width,
                  (*pixmap)->height, (mask && *mask) ? " (has mask)" : "");
#endif

  return 1;
}

/* --- global functions --- */

void ui_imagelib_display_opened(Display *display) {}

void ui_imagelib_display_closed(Display *display) {}

Pixmap ui_imagelib_load_file_for_background(ui_window_t *win, char *path,
                                            ui_picture_modifier_t *pic_mod) {
  Pixmap pixmap;

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
  ui_display_enable_to_change_cmap(1);
#endif

  if (!load_file(win->disp->display, path, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win), 0, pic_mod,
                 win->disp->depth, &pixmap, NULL, NULL)) {
    pixmap = None;
  }

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
  ui_display_enable_to_change_cmap(0);
#endif

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

  if (!load_file(disp->display, path, *width, *height, keep_aspect, NULL, disp->depth,
                 pixmap, mask, transparent)) {
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
