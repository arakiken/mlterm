/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_DRCS_H__
#define __VT_DRCS_H__

#include <pobl/bl_types.h>
#include <mef/ef_char.h>

typedef struct vt_drcs_font {
  /*ef_charset_t cs;*/ /* 0x40-0x7e */
  char *glyphs[0x60];

  u_int16_t pic_id; /* MAX_INLINE_PICTURES in ui_picture.h */
  u_int16_t pic_beg_idx; /* 0x0-0x59 */
  u_int16_t pic_num_cols;
  u_int16_t pic_num_rows;
  u_int16_t pic_num_cols_small;
  u_int16_t pic_num_rows_small;
  u_int32_t pic_offset; /* offset from the begining of the picture */

} vt_drcs_font_t;

typedef struct vt_drcs {
  vt_drcs_font_t *fonts[CS96SB_ID(0x7e)+1];

} vt_drcs_t;

#define vt_drcs_new() calloc(1, sizeof(vt_drcs_t))

#define vt_drcs_delete(drcs) \
  vt_drcs_final_full(drcs);  \
  free(drcs);

void vt_drcs_select(vt_drcs_t *drcs);

char *vt_drcs_get_glyph(ef_charset_t cs, u_char idx);

vt_drcs_font_t *vt_drcs_get_font(vt_drcs_t *drcs, ef_charset_t cs, int create);

void vt_drcs_final(vt_drcs_t *drcs, ef_charset_t cs);

void vt_drcs_final_full(vt_drcs_t *drcs);

void vt_drcs_add_glyph(vt_drcs_font_t *font, int idx, const char *seq, u_int width, u_int height);

void vt_drcs_add_picture(vt_drcs_font_t *font, int id, u_int offset, int beg_inx,
                         u_int num_cols, u_int num_rows, u_int num_cols_small,
                         u_int num_rows_small);

int vt_drcs_get_picture(vt_drcs_font_t *font, int *id, int *pos, u_int ch);

#define vt_drcs_has_picture(font) ((font)->pic_num_rows > 0)

#define vt_drcs_is_picture(font, ch) \
  (vt_drcs_has_picture(font) && (font)->pic_beg_idx + 0x20 <= ((ch) & 0x7f))

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch);

int vt_convert_unicode_pua_to_drcs(ef_char_t *ch);

#endif
