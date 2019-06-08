/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_str.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>

#include "zmodem.h"

#ifndef LIBDIR
#define TRANSFERLIB_DIR "/usr/local/lib/mlterm/"
#else
#define TRANSFERLIB_DIR LIBDIR "/mlterm/"
#endif

/* --- static variables --- */

static int zmodem_mode;
static struct file_info zmodem_info[2];

static void (*dl_zmodem)(unsigned char *, const unsigned int, unsigned char *, unsigned int *,
                      const unsigned int);
static Q_BOOL (*dl_zmodem_start)(struct file_info *, const char *, const Q_BOOL,
                                 const ZMODEM_FLAVOR, int);
static Q_BOOL (*dl_zmodem_is_processing)(int *, int *);

static int is_tried;
#ifndef NO_DYNAMIC_LOAD_TRANSFER
static bl_dl_handle_t handle;
#endif

/* --- static functions --- */

static int load_library(void) {
  is_tried = 1;

#ifndef NO_DYNAMIC_LOAD_TRANSFER
  if (!(handle = bl_dl_open(TRANSFERLIB_DIR, "zmodem")) && !(handle = bl_dl_open("", "zmodem"))) {
    bl_error_printf("ZMODEM: Could not load.\n");

    return 0;
  }

  bl_dl_close_at_exit(handle);

  dl_zmodem = bl_dl_func_symbol(handle, "zmodem");
  dl_zmodem_start = bl_dl_func_symbol(handle, "zmodem_start");
  dl_zmodem_is_processing = bl_dl_func_symbol(handle, "zmodem_is_processing");
#else
  dl_zmodem = zmodem;
  dl_zmodem_start = zmodem_start;
  dl_zmodem_is_processing = zmodem_is_processing;
#endif

  return 1;
}

/* --- global functions --- */

int vt_transfer_start(/* vt_transfer_type_t type, */
                      char *send_file /* allocated by the caller */,
                      const char *save_dir, int is_crc32, int progress_len) {
  Q_BOOL ret;

  if (zmodem_mode || (is_tried ? (dl_zmodem_start == NULL) : !load_library())) {
    return 0;
  }

  if (send_file) {
    zmodem_info[0].name = send_file;
    stat(send_file, &zmodem_info[0].fstats);

    ret = (*dl_zmodem_start)(zmodem_info, save_dir, Q_TRUE, is_crc32 ? Z_CRC32 : Z_CRC16,
                             progress_len);
  } else {
    ret = (*dl_zmodem_start)(NULL, save_dir, Q_FALSE, is_crc32 ? Z_CRC32 : Z_CRC16, progress_len);
  }

  if (ret) {
    zmodem_mode = 1;

    return 1;
  } else {
    return 0;
  }
}

void vt_transfer_data(u_char *input, const u_int input_n, u_char *output, u_int *output_n,
                      const u_int output_max) {
#if 0
  if (input_n > 0) {
    FILE *fp = fopen("zmodem.log", "a");
    u_int i;

    fprintf(fp, "INPUT %d\n", input_n);
    for (i = 0; i < input_n; i++) {
      fprintf(fp, "%c", input[i]);
    }
    fprintf(fp, "\n");
    fclose(fp);
  }
#endif

  (*dl_zmodem)(input, input_n, output, output_n, output_max);

#if 0
  if (*output_n > 0) {
    FILE *fp = fopen("zmodem.log", "a");
    u_int i;

    fprintf(fp, "OUTPUT %d\n", *output_n);
    for (i = 0; i < *output_n; i++) {
      fprintf(fp, "%c", output[i]);
    }
    fprintf(fp, "\n");
    fclose(fp);
  }
#endif
}

int vt_transfer_get_state(int *progress_cur, int *progress_len) {
  static int progress_prev = -1;

  if ((*dl_zmodem_is_processing)(progress_cur, progress_len)) {
    if (progress_prev < *progress_cur) {
      progress_prev = *progress_cur;

      return 1;
    } else {
      return -1;
    }
  } else {
    zmodem_mode = 0;
    free(zmodem_info[0].name);
    zmodem_info[0].name = NULL;
    progress_prev = -1;

    return 0;
  }
}
