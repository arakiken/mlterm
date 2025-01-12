/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <unistd.h> /* STDOUT_FILENO */
#include <stdlib.h> /* mbstowcs_s */
#include <wchar.h>  /* wcslen */

extern "C" {
#include <pobl/bl_debug.h>
#include <pobl/bl_conf_io.h> /* bl_get_user_rc_path */
#include <pobl/bl_mem.h>
#if defined(__CYGWIN__) || defined(__MSYS__)
#include <pobl/bl_path.h> /* bl_conv_to_win32_path */
#endif
}

#include <pobl/bl_types.h> /* u_int32_t/u_int16_t */
#include <pobl/bl_def.h>   /* SSIZE_MAX, USE_WIN32API */

#ifdef USE_WIN32API
#include <fcntl.h> /* O_BINARY */
#endif

#define _WIN32_WINNT 0x0502 /* for SetDllDirectory */
#include <windows.h>
#include <gdiplus.h>

using namespace Gdiplus;

#if 0
#define __DEBUG
#endif

#ifndef IID_PPV_ARGS
/* IStream IID (objidl.h) */
static const IID __uuid_inst = {
    0x0000000c, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
#define IID_PPV_ARGS(iface) __uuid_inst, reinterpret_cast<void**>(iface)
#endif

#ifndef URL_MK_UNIFORM
#define URL_MK_UNIFORM 1
#endif

/* --- static functions --- */

#define BUILTIN_IMAGELIB /* Necessary to use create_cardinals_from_sixel() */
#include "../../common/c_imagelib.c"

static void help(void) {
  /* Don't output to stdout where mlterm waits for image data. */
  fprintf(stderr, "mlimgloader 0 [width] [height] [file path] stdout (-a)\n");
  fprintf(stderr, "  -a: keep aspect ratio.\n");
}

/* path: cygwin style path on cygwin/msys. win32 style path on win32. */
static u_int32_t *create_cardinals_from_file(const char *path, u_int width, u_int height,
                                             int keep_aspect) {
  u_int32_t *cardinal = NULL;

  /* strings.h is included in c_imagelib.c */
  if (strcasecmp(path + strlen(path) - 4, ".six") == 0 &&
      (cardinal = create_cardinals_from_sixel(path)) &&
      (width == 0 || height == 0)) {
    return cardinal;
  }

  Gdiplus::GdiplusStartupInput startup;
  ULONG_PTR token;

  if (Gdiplus::GdiplusStartup(&token, &startup, NULL) != Gdiplus::Ok) {
    return NULL;
  }

  /* MAX_PATH which is 260 (3+255+1+1) is defined in win32 alone. */
  wchar_t wpath[MAX_PATH];
#if defined(__CYGWIN__) || defined(__MSYS__)
  char winpath[MAX_PATH];
#endif
  HMODULE module;
  IBindCtx *ctx;
  IMoniker *moniker;
  IStream *stream = NULL;
  Gdiplus::Bitmap *bitmap;

  if (cardinal) {
    /* Resize sixel graphics */
    Gdiplus::Bitmap src(cardinal[0], cardinal[1], cardinal[0] * 4,
                        PixelFormat32bppARGB, (BYTE*)(cardinal + 2));
    if (!(bitmap = new Bitmap(width, height, PixelFormat32bppARGB))) {
      goto end1;
    }

    Graphics *graphics;
    if (!(graphics = Graphics::FromImage(bitmap))) {
      delete bitmap;

      goto end1;
    }

    Gdiplus::Rect rect(0, 0, width, height);
    graphics->DrawImage(&src, rect, 0, 0, cardinal[0], cardinal[1], UnitPixel);

    delete graphics;
    free(cardinal);
  } else {
    /* Image except sixel graphics */
    int hash = hash_path(path);

    if (strstr(path, "://")) {
      typedef HRESULT (*func)(IMoniker*, LPCWSTR, IMoniker**, DWORD);
      func create_url_moniker;

      SetDllDirectory("");
      if ((module = LoadLibrary("urlmon")) &&
          (create_url_moniker = (func)GetProcAddress(module, "CreateURLMonikerEx"))) {
        MultiByteToWideChar(CP_UTF8, 0, path, strlen(path) + 1, wpath, MAX_PATH);

        if ((*create_url_moniker)(NULL, wpath, &moniker, URL_MK_UNIFORM) == S_OK) {
          if (CreateBindCtx(0, &ctx) == S_OK) {
            if (moniker->BindToStorage(ctx, NULL, IID_PPV_ARGS(&stream)) != S_OK) {
              ctx->Release();
              moniker->Release();
            }
          } else {
            moniker->Release();
          }
        }

        if (!stream) {
          FreeLibrary(module);
        }
      }
    }
#if defined(__CYGWIN__) || defined(__MSYS__)
    /* In case win32 style path is specified on cygwin/msys. */
    else if (strchr(path, '/')) {
      /* cygwin style path => win32 style path. */
      if (bl_conv_to_win32_path(path, winpath, sizeof(winpath)) == 0) {
        path = winpath;
      }
    }
#endif

    char *dir;
    if (strcmp(path + strlen(path) - 4, ".gif") == 0 && /* Animation GIF */
#if defined(__CYGWIN__) || defined(__MSYS__)
      /* converted to win32 by bl_conv_to_win32_path */
        (!strstr(path, "mlterm\\anim") && (dir = bl_get_user_rc_path("mlterm/")))
#else
        (!strstr(path, "mlterm/anim") && (dir = bl_get_user_rc_path("mlterm\\")))
#endif
        ) {
      char new_path[strlen(dir) + 8 + 5 + DIGIT_STR_LEN(int) + 1];

      sprintf(new_path, "%sanim%d.gif", dir, hash);

      FILE *fp;
      if (stream && (fp = fopen(new_path, "wb"))) {
        BYTE buf[10240];
        ULONG rd_len;
        HRESULT res;

        do {
          res = stream->Read(buf, sizeof(buf), &rd_len);
          fwrite(buf, 1, rd_len, fp);
        } while (res == Gdiplus::Ok);

        fclose(fp);

        stream->Release();
        ctx->Release();
        moniker->Release();
        FreeLibrary(module);
        stream = NULL;

        path = new_path;
      }

      split_animation_gif(path, dir, hash);
      free(dir);

      /* Replace path by the split file. */
#if defined(__CYGWIN__) || defined(__MSYS__)
      if (bl_conv_to_win32_path(new_path, winpath, sizeof(winpath)) == 0) {
        path = winpath;
      } else
#endif
      {
        path = new_path;
      }

      if (!stream) {
        MultiByteToWideChar(CP_UTF8, 0, path, strlen(path) + 1, wpath, MAX_PATH);
      }
    } else {
      if (!stream) {
        MultiByteToWideChar(CP_UTF8, 0, path, strlen(path) + 1, wpath, MAX_PATH);
      }
    }

    cardinal = NULL;

    if (width == 0 && height == 0) {
      if (stream ? !(bitmap = Gdiplus::Bitmap::FromStream(stream))
          : !(bitmap = Gdiplus::Bitmap::FromFile(wpath))) {
        goto end2;
      }
    } else {
      Image *image;

      if ((stream ? !(image = Image::FromStream(stream)) : !(image = Image::FromFile(wpath))) ||
          image->GetLastStatus() != Gdiplus::Ok) {
        goto end2;
      }

      u_int img_width = image->GetWidth();
      u_int img_height = image->GetHeight();

      if (width == 0) {
        width = img_width;;
      } else if (height == 0) {
        height = img_height;
      }

      if (keep_aspect) {
        u_int w = height * img_width / img_height;

        if (w < width) {
          width = w;
        } else {
          u_int h = width * img_height / img_width;

          if (h < height) {
            height = h;
          }
        }
      }

      if (!(bitmap = new Bitmap(width, height, PixelFormat32bppARGB))) {
        delete image;

        goto end2;
      }

      Graphics *graphics;
      if (!(graphics = Graphics::FromImage(bitmap))) {
        delete image;

        goto end3;
      }

      Gdiplus::Rect rect(0, 0, width, height);

      graphics->DrawImage(image, rect, 0, 0, img_width, img_height, UnitPixel);

      delete image;
      delete graphics;
    }
  }

  if (bitmap->GetLastStatus() != Gdiplus::Ok) {
    goto end3;
  }

  width = bitmap->GetWidth();
  height = bitmap->GetHeight();

  u_int32_t *p;
  if (width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height || /* integer overflow */
      !(p = cardinal = (u_int32_t*)malloc((width * height + 2) * sizeof(*cardinal)))) {
    goto end3;
  }

  *(p++) = width;
  *(p++) = height;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Gdiplus::Color pixel;

      bitmap->GetPixel(x, y, &pixel);

      *(p++) = (pixel.GetA() << 24) | (pixel.GetR() << 16) | (pixel.GetG() << 8) | pixel.GetB();
    }
  }

end3:
  delete bitmap;

end2:
  if (stream) {
    stream->Release();
    ctx->Release();
    moniker->Release();
    FreeLibrary(module);
  }

end1:
  Gdiplus::GdiplusShutdown(token);

  return cardinal;
}

/* --- global functions --- */

int PASCAL WinMain(HINSTANCE hinst, HINSTANCE hprev, char *cmdline, int cmdshow) {
  WCHAR **w_argv;
  int argc;

#if 0
  bl_set_msg_log_file_name("mlterm/msg.log");
#endif

  w_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argc == 0) {
    GlobalFree(w_argv);

    return -1;
  }

  char *argv[argc];

  for (int count = 0; count < argc; count++) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w_argv[count], wcslen(w_argv[count]) + 1,
                                  NULL, 0, NULL, NULL);
    if ((argv[count] = (char*)malloc(len))) {
      WideCharToMultiByte(CP_UTF8, 0, w_argv[count], wcslen(w_argv[count]) + 1, argv[count], len,
                          NULL, NULL);
    } else {
      argc = count;
      break;
    }
  }

  GlobalFree(w_argv);

  if ((argc != 6 && argc != 7) || strcmp(argv[5], "stdout") != 0) {
    help();

    return -1;
  }

  char new_path[strlen(argv[4]) + 1];

  if (strstr(argv[4], ".rgs")) {
    strcpy(new_path, argv[4]);
    if (convert_regis_to_bmp(new_path)) {
      argv[4] = new_path;
    }
  }

  u_int width = atoi(argv[2]);
  u_int height = atoi(argv[3]);

  /*
   * attr.width / attr.height aren't trustworthy because this program can be
   * called before window is actually resized.
   */

  u_char *cardinal;
  ssize_t size; /* should declare before 'goto' */
  if (!(cardinal = (u_char*)create_cardinals_from_file(argv[4], width, height,
                                                       (argc == 7 &&
                                                        strcmp(argv[6], "-a") == 0)))) {
    goto error;
  }

  width = ((u_int32_t*)cardinal)[0];
  height = ((u_int32_t*)cardinal)[1];
  size = sizeof(u_int32_t) * (width * height + 2);

#ifdef USE_WIN32API
  setmode(STDOUT_FILENO, O_BINARY);
#endif

  while (size > 0) {
    ssize_t n_wr;

    if ((n_wr = write(STDOUT_FILENO, cardinal, size)) < 0) {
      goto error;
    }

    cardinal += n_wr;
    size -= n_wr;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Exit image loader\n");
#endif

  /* XXX should free(argv[n]) */

  return 0;

error:
  /* XXX should free(argv[n]) */
  bl_error_printf("Couldn't load %s\n", argv[4]);

  return -1;
}
