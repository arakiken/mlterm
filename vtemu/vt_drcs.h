/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_DRCS_H__
#define __VT_DRCS_H__

#include <pobl/bl_types.h>
#include <mef/ef_char.h>

/*
 * XXX
 * 0x100 == CS_REVISION_N(cs, 1) in encdoefilter, but it is assumed that
 * single byte charsets (0x0-0x9e) doesn't have revisions.
 *
 * DRCS: encodefilter <=> vtemu
 * cs: cs | (' '-0x1f << 9) | ((I-0x1f) << 13) <=> cs | 0x100
 * ch: 0x20..0x7f <=> DRCSMMv2 0x20..0x7f or 0xa0..0xff
 *                    DRCSMMv3 ((I-0x1f) << 8) | 0x21..0x7e
 */
#define CS_TO_DRCS(cs) ((cs) | 0x100)
#define DRCS_TO_CS(cs) ((cs) & 0xff)
#define IS_DRCS(cs) (((cs) & 0x100) && IS_CSSB(cs))

#define DRCS_CH_TO_SLOT(ch) (((ch) >> 8) & 0xff)

typedef struct vt_drcs_picture {
  u_int16_t id; /* MAX_INLINE_PICTURES in ui_picture.h */
  u_int16_t beg_idx; /* 0x0-0x59 */
  u_int16_t num_cols;
  u_int16_t num_rows;
  u_int16_t num_cols_small;
  u_int16_t num_rows_small;
  u_int32_t offset; /* offset from the begining of the picture */

} vt_drcs_picture_t;

typedef struct vt_drcs_font {
  char *glyphs[0x60]; /* 0x20-0x7f */

  /*
   * pictures[0]: No intermediate character except " ".
   * pictures[1]-pictures[16]: 0x20-0x2f after " ".
   */
  vt_drcs_picture_t pictures[17];

} vt_drcs_font_t;

typedef struct vt_drcs {
  /*
   * DRCSMMv2: 0x0(CS94SB_ID(0x30)) - 0x4e(CS94SB_ID(0x7e)),
   *           0x50(CS96SB_ID(0x30) - 0x9e(CS96SB_ID(0x7e))
   * DRCSMMv3: 0x10(CS94SB_ID(0x40)) - 0x4e(CS94SB_ID(0x7e))
   */
  vt_drcs_font_t *fonts[CS96SB_ID(0x7e)+1];

} vt_drcs_t;

#define vt_drcs_new() calloc(1, sizeof(vt_drcs_t))

#define vt_drcs_destroy(drcs) \
  vt_drcs_final_full(drcs);  \
  free(drcs);

void vt_drcs_set_version(int version);

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

#define vt_drcs_has_picture(font, slot) ((font)->pictures[(slot)].num_rows > 0)

#define vt_drcs_is_picture(font, slot, ch) \
  (vt_drcs_has_picture(font, slot) && \
   (font)->pictures[slot].beg_idx + (slot ? 0x21 : 0x20) <= ((ch) & 0x7f))

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch);

int vt_convert_unicode_pua_to_drcs(ef_char_t *ch);

#endif
