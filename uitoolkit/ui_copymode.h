/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <pobl/bl_types.h>
#include <mef/ef_parser.h>
#include <vt_char.h>

#define MAX_COPYMODE_PATTERN_LEN 128

typedef struct ui_copymode {
  vt_char_t pattern[MAX_COPYMODE_PATTERN_LEN];
  u_int8_t pattern_len;
  int8_t pattern_editing;

  int16_t cursor_char_index; /* visual */
  int16_t cursor_row;        /* visual */

} ui_copymode_t;

ui_copymode_t *ui_copymode_new(int char_index, int row);

void ui_copymode_destroy(ui_copymode_t *copymode);

void ui_copymode_pattern_start_edit(ui_copymode_t *copymode);

void ui_copymode_pattern_cancel_edit(ui_copymode_t *copymode);

void ui_copymode_pattern_add_str(ui_copymode_t *copymode, u_char *str, size_t len,
                                 ef_parser_t *parser);

int ui_copymode_pattern_delete(ui_copymode_t *copymode);

u_char *ui_copymode_get_utf8_pattern(ui_copymode_t *copymode);
