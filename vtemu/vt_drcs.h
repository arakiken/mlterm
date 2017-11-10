/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_DRCS_H__
#define __VT_DRCS_H__

#include <pobl/bl_types.h>
#include <mef/ef_char.h>

typedef struct vt_drcs_font {
  ef_charset_t cs;
  char *glyphs[0x5f];

} vt_drcs_font_t;

typedef struct vt_drcs {
  vt_drcs_font_t *fonts;
  u_int num_fonts;

} vt_drcs_t;

#define vt_drcs_new() calloc(1, sizeof(vt_drcs_t))

#define vt_drcs_delete(drcs) \
  vt_drcs_select(drcs);      \
  vt_drcs_final_full();      \
  free(drcs);

void vt_drcs_select(vt_drcs_t *drcs);

vt_drcs_font_t *vt_drcs_get_font(ef_charset_t cs, int create);

char *vt_drcs_get_glyph(ef_charset_t cs, u_char idx);

void vt_drcs_final(ef_charset_t cs);

void vt_drcs_final_full(void);

int vt_drcs_add(vt_drcs_font_t *font, int idx, const char *seq, u_int width, u_int height);

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch);

int vt_convert_unicode_pua_to_drcs(ef_char_t *ch);

#endif
