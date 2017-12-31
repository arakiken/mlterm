/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_drcs.h"

#include <string.h> /* memset */
#include <limits.h> /* UINT_MAX */
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>

#ifndef UINT16_MAX
#define UINT16_MAX ((1 << 16) - 1)
#endif

/* --- static variables --- */

static vt_drcs_t *cur_drcs;

/* --- global functions --- */

void vt_drcs_select(vt_drcs_t *drcs) {
  cur_drcs = drcs;
}

char *vt_drcs_get_glyph(ef_charset_t cs, u_char idx) {
  vt_drcs_font_t *font;

  /* msb can be set in vt_parser.c (e.g. ESC(I (JISX0201 kana)) */
  if ((font = vt_drcs_get_font(cur_drcs, cs, 0)) && 0x20 <= (idx & 0x7f)) {
#if 0
    /*
     * See https://vt100.net/docs/vt510-rm/DECDLD.html
     *
     * Pcss: Defines the character set as a 94- or 96- character graphic set.
     *       0 = 94-character set. (default)
     *       1 = 96-character set.
     * The value of Pcss changes the meaning of the Pcn (starting character) parameter above.
     * If Pcss = 0 (94-character set)
     * The terminal ignores any attempt to load characters into the 2/0 or 7/15 table positions.
     *     Pcn        Specifies
     *     1          column 2/row 1
     *     ...
     *     94         column 7/row 14
     *
     *     If Pcss = 1 (96-character set)
     *     Pcn        Specifies
     *     0          column 2/row 0
     *     ...
     *     95         column 7/row 15
     */
    if (IS_CS94SB(cs) && (idx == 0x20 | idx == 0x7f)) {
      return NULL;
    }
#endif

    return font->glyphs[(idx & 0x7f) - 0x20];
  } else {
    return NULL;
  }
}

vt_drcs_font_t *vt_drcs_get_font(vt_drcs_t *drcs, ef_charset_t cs, int create) {
  if (!drcs) {
    return NULL;
  }

  /* CS94SB(0x30)-CS94SB(0x7e) (=0x00-0x4e), CS96SB(0x30)-CS96SB(0x7e) (0x50-0x9e) */
  if (cs > CS96SB_ID(0x7e)) {
    return NULL;
  }

  if (!drcs->fonts[cs]) {
    if (!create || !(drcs->fonts[cs] = calloc(1, sizeof(vt_drcs_font_t)))) {
      return NULL;
    }
  }

  return drcs->fonts[cs];
}

void vt_drcs_final(vt_drcs_t *drcs, ef_charset_t cs) {
  if (drcs) {
    if (drcs->fonts[cs]) {
      int idx;

      for (idx = 0; idx <= 0x5f; idx++) {
        free(drcs->fonts[cs]->glyphs[idx]);
      }

      free(drcs->fonts[cs]);
      drcs->fonts[cs] = NULL;
    }
  }
}

void vt_drcs_final_full(vt_drcs_t *drcs) {
  ef_charset_t cs;

  for (cs = CS94SB_ID(0x30); cs <= CS96SB_ID(0x7e); cs++) {
    vt_drcs_final(drcs, cs);
  }
}

void vt_drcs_add_glyph(vt_drcs_font_t *font, int idx, const char *seq, u_int width, u_int height) {
  if (font) {
    free(font->glyphs[idx]);

    if ((font->glyphs[idx] = malloc(2 + strlen(seq) + 1))) {
      font->glyphs[idx][0] = width;
      font->glyphs[idx][1] = height;
      strcpy(font->glyphs[idx] + 2, seq);
    }
  }
}

void vt_drcs_add_picture(vt_drcs_font_t *font, int id, int offset, int beg_idx,
                         u_int num_cols, u_int num_rows,
                         u_int num_cols_small, u_int num_rows_small) {
  if (num_cols > UINT16_MAX || num_rows > UINT16_MAX) {
    return;
  }

  if (font) {
    font->pic_id = id;
    font->pic_offset = offset;
    font->pic_beg_idx = beg_idx;
    font->pic_num_rows = num_rows;
    font->pic_num_cols = num_cols;
    font->pic_num_cols_small = num_cols_small;
    font->pic_num_rows_small = num_rows_small;
  }
}

int vt_drcs_get_picture(vt_drcs_font_t *font, int *id, int *pos, u_int ch) {
  if (font->pic_num_rows > 0) {
    ch &= 0x7f;
    if (ch >= 0x20 && (ch -= 0x20) >= font->pic_beg_idx) {
      ch += font->pic_offset;
      /* See MAKE_INLINEPIC_POS() in ui_picture.h */
      *pos = (ch % font->pic_num_cols_small) * font->pic_num_rows +
             (ch / font->pic_num_cols_small);
      *id = font->pic_id;

      return 1;
    }
  }

  return 0;
}

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch) {
  if (vt_drcs_get_glyph(ch->cs, ch->ch[0])) {
    if (IS_CS94SB(ch->cs)) {
      ch->ch[2] = CS94SB_FT(ch->cs);
      ch->ch[3] = ch->ch[0] & 0x7f;
    } else {
      ch->ch[2] = CS96SB_FT(ch->cs);
      ch->ch[3] = ch->ch[0] | 0x80;
    }
    ch->ch[1] = 0x10;
    ch->ch[0] = 0x00;
    ch->cs = ISO10646_UCS4_1;
    ch->size = 4;
    ch->property = 0;

    return 1;
  } else {
    return 0;
  }
}

int vt_convert_unicode_pua_to_drcs(ef_char_t *ch) {
  u_char *c;

  c = ch->ch;

  if (c[1] == 0x10 && 0x30 <= c[2] && c[2] <= 0x7e && c[0] == 0x00) {
    if (0x20 <= c[3] && c[3] <= 0x7f) {
      ch->cs = CS94SB_ID(c[2]);
    } else if (0xa0 <= c[3] && c[3] <= 0xff) {
      ch->cs = CS96SB_ID(c[2]);
    } else {
      return 0;
    }

    c[0] = c[3];
    ch->size = 1;
    ch->property = 0;

    return 1;
  }

  return 0;
}
