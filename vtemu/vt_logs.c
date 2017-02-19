/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_logs.h"

#include <string.h>      /* memmove/memset */
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h>

#if 0
#define __DEBUG
#endif

/* --- global functions --- */

int vt_log_init(vt_logs_t *logs, u_int num_of_rows) {
  logs->lines = NULL;
  logs->index = NULL;
  logs->num_of_rows = 0;

  if (num_of_rows == 0) {
    return 1;
  }

  if ((logs->lines = calloc(sizeof(vt_line_t), num_of_rows)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " calloc() failed.\n");
#endif
    return 0;
  }

  if ((logs->index = bl_cycle_index_new(num_of_rows)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " bl_cycle_index_new() failed.\n");
#endif

    free(logs->lines);
    logs->lines = NULL;

    return 0;
  }

  logs->num_of_rows = num_of_rows;

  return 1;
}

int vt_log_final(vt_logs_t *logs) {
  int count;

  if (logs->num_of_rows == 0) {
    return 1;
  }

  for (count = 0; count < logs->num_of_rows; count++) {
    vt_line_final(&logs->lines[count]);
  }

  bl_cycle_index_delete(logs->index);

  free(logs->lines);

  return 1;
}

int vt_change_log_size(vt_logs_t *logs, u_int new_num_of_rows) {
  u_int num_of_filled_rows;

  logs->unlimited = 0;

  num_of_filled_rows = vt_get_num_of_logged_lines(logs);

  if (new_num_of_rows == logs->num_of_rows) {
    return 1;
  } else if (new_num_of_rows == 0) {
    free(logs->lines);
    logs->lines = NULL;

    bl_cycle_index_delete(logs->index);
    logs->index = NULL;

    logs->num_of_rows = 0;

    return 1;
  } else if (new_num_of_rows > logs->num_of_rows) {
    vt_line_t *new_lines;

    if (sizeof(vt_line_t) * new_num_of_rows < sizeof(vt_line_t) * logs->num_of_rows) {
      /* integer overflow */
      return 0;
    }

    if ((new_lines = realloc(logs->lines, sizeof(vt_line_t) * new_num_of_rows)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " realloc() failed.\n");
#endif

      return 0;
    }

    memset(&new_lines[logs->num_of_rows], 0,
           sizeof(vt_line_t) * (new_num_of_rows - logs->num_of_rows));

    logs->lines = new_lines;
  } else if (new_num_of_rows < logs->num_of_rows) {
    vt_line_t *new_lines;
    vt_line_t *line;
    int count;
    int start;

    if ((new_lines = calloc(sizeof(vt_line_t), new_num_of_rows)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " calloc() failed.\n");
#endif

      return 0;
    }

    num_of_filled_rows = vt_get_num_of_logged_lines(logs);

    if (new_num_of_rows >= num_of_filled_rows) {
      start = 0;
    } else {
      start = num_of_filled_rows - new_num_of_rows;
    }

    /*
     * freeing excess lines.
     */
    for (count = 0; count < start; count++) {
      if ((line = vt_log_get(logs, count)) == NULL) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " this is impossible.\n");
#endif

        return 0;
      }

      vt_line_final(line);
    }

    /*
     * copying to new lines.
     */
    for (count = 0; count < new_num_of_rows; count++) {
      if ((line = vt_log_get(logs, count + start)) == NULL) {
        break;
      }

      vt_line_init(&new_lines[count], line->num_of_filled_chars);
      vt_line_share(&new_lines[count], line);
    }

    free(logs->lines);
    logs->lines = new_lines;
  }

  if (logs->index) {
    if (!bl_cycle_index_change_size(logs->index, new_num_of_rows)) {
      return 0;
    }
  } else {
    if ((logs->index = bl_cycle_index_new(new_num_of_rows)) == NULL) {
      return 0;
    }
  }

  logs->num_of_rows = new_num_of_rows;

  return 1;
}

int vt_log_add(vt_logs_t *logs, vt_line_t *line) {
  int at;

  if (logs->num_of_rows == 0) {
    return 1;
  }

  if (logs->unlimited &&
      bl_get_filled_cycle_index(logs->index) == bl_get_cycle_index_size(logs->index)) {
    if (logs->num_of_rows + 128 > logs->num_of_rows) {
      vt_change_log_size(logs, logs->num_of_rows + 128);
      logs->unlimited = 1;
    }
  }

  at = bl_next_cycle_index(logs->index);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %d len line logged to index %d.\n", line->num_of_filled_chars, at);
#endif

  vt_line_final(&logs->lines[at]);

  /* logs->lines[at] becomes completely the same one as line */
  vt_line_clone(&logs->lines[at], line, line->num_of_filled_chars);

  vt_line_set_updated(&logs->lines[at]);

  return 1;
}

vt_line_t *vt_log_get(vt_logs_t *logs, int at) {
  int _at;

  if (at < 0 || vt_get_num_of_logged_lines(logs) <= at) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " row %d is overflowed in logs.\n", at);
#endif

    return NULL;
  }

  if ((_at = bl_cycle_index_of(logs->index, at)) == -1) {
    return NULL;
  }

  return &logs->lines[_at];
}

u_int vt_get_num_of_logged_lines(vt_logs_t *logs) {
  if (logs->num_of_rows == 0) {
    return 0;
  } else {
    return bl_get_filled_cycle_index(logs->index);
  }
}

int vt_log_reverse_color(vt_logs_t *logs, int char_index, int row) {
  vt_line_t *line;

  if ((line = vt_log_get(logs, row)) == NULL) {
    return 0;
  }

  vt_char_reverse_color(vt_char_at(line, char_index));

  vt_line_set_modified(line, char_index, vt_line_end_char_index(line));

  return 1;
}

int vt_log_restore_color(vt_logs_t *logs, int char_index, int row) {
  vt_line_t *line;

  if ((line = vt_log_get(logs, row)) == NULL) {
    return 0;
  }

  vt_char_restore_color(vt_char_at(line, char_index));

  vt_line_set_modified(line, char_index, vt_line_end_char_index(line));

  return 1;
}
