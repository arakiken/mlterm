/*
 *	$Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h> /* strstr */
#include <stdlib.h> /* atoi */

#include <pobl/bl_debug.h>
#include <pobl/bl_types.h> /* u_int32_t/u_int16_t */
#include <pobl/bl_def.h>   /* SSIZE_MAX, USE_WIN32API */
#include <pobl/bl_str.h>   /* bl_str_alloca_dup */
#if defined(__CYGWIN__) || defined(__MSYS__)
#include <pobl/bl_path.h> /* bl_conv_to_win32_path */
#endif

#ifdef USE_WIN32API
#include <fcntl.h> /* O_BINARY */
#endif

#if 0
#define __DEBUG
#endif

/* --- static functions --- */

#define BUILTIN_IMAGELIB /* Necessary to use create_cardinals_from_sixel() */
#include "../../common/c_imagelib.c"

static void help(void) {
  /* Don't output to stdout where mlterm waits for image data. */
  fprintf(stderr, "mlimgloader [window id] [width] [height] [file path] (-c)\n");
  fprintf(stderr, " window id: ignored.\n");
  fprintf(stderr, " -c       : output XA_CARDINAL format data to stdout.\n");
}

/* --- global functions --- */

int main(int argc, char** argv) {
  u_char* cardinal;
  ssize_t size;
  u_int width;
  u_int height;

#if 0
  bl_set_msg_log_file_name("mlterm/msg.log");
#endif

  if (argc != 6 || strcmp(argv[5], "-c") != 0) {
    help();

    return -1;
  }

  if (strstr(argv[4], ".rgs")) {
    char* new_path;

    new_path = bl_str_alloca_dup(argv[4]);
    if (convert_regis_to_bmp(new_path)) {
      argv[4] = new_path;
    }
  }

  if (!(cardinal = (u_char*)create_cardinals_from_sixel(argv[4]))) {
#if defined(__CYGWIN__) || defined(__MSYS__)
#define MAX_PATH 260 /* 3+255+1+1 */
    char winpath[MAX_PATH];
    if (bl_conv_to_win32_path(argv[4], winpath, sizeof(winpath)) < 0 ||
        !(cardinal = (u_char*)create_cardinals_from_sixel(winpath)))
#endif
    {
      goto error;
    }
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

  return 0;

error:
  bl_error_printf("Couldn't load %s\n", argv[4]);

  return -1;
}
