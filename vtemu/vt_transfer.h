/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_TRANSFER_H__
#define __VT_TRANSFER_H__

#include <pobl/bl_types.h>

int vt_transfer_start(char *send_file, const char *save_dir, int is_crc32, int progress_len);

void vt_transfer_cancel(void);

void vt_transfer_data(u_char *input, const u_int input_n, u_char *output, u_int *output_n,
                      const u_int output_max);

int vt_transfer_get_state(int *progress_cur, int *progress_len);

#endif
