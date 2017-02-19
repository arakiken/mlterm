/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_LOGS_H__
#define __VT_LOGS_H__

#include <pobl/bl_cycle_index.h>

#include "vt_char.h"
#include "vt_edit.h"

typedef struct vt_logs {
  vt_line_t *lines;
  bl_cycle_index_t *index;
  u_int num_of_rows;
  int unlimited;

} vt_logs_t;

int vt_log_init(vt_logs_t *logs, u_int num_of_rows);

int vt_log_final(vt_logs_t *logs);

int vt_change_log_size(vt_logs_t *logs, u_int num_of_rows);

#define vt_unlimit_log_size(logs) ((logs)->unlimited = 1)

#define vt_log_size_is_unlimited(logs) ((logs)->unlimited)

int vt_log_add(vt_logs_t *logs, vt_line_t *line);

vt_line_t *vt_log_get(vt_logs_t *logs, int at);

u_int vt_get_num_of_logged_lines(vt_logs_t *logs);

#define vt_get_log_size(logs) ((logs)->num_of_rows)

int vt_log_reverse_color(vt_logs_t *logs, int char_index, int row);

int vt_log_restore_color(vt_logs_t *logs, int char_index, int row);

#endif
