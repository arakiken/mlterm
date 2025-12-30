/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_drcs.h"

#include <string.h> /* memset */
#include <limits.h> /* UINT_MAX */
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

#ifndef UINT16_MAX
#define UINT16_MAX ((1 << 16) - 1)
#endif

/* --- static variables --- */

static vt_drcs_t *cur_drcs;
static int version = 2; /* 2 or 3 */

/* --- global functions --- */

void vt_drcs_set_version(int v) {
  if (v == 2 || v == 3) {
    version = v;
  }
}

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
  if (!drcs || !IS_DRCS(cs)) {
    return NULL;
  }

  cs = DRCS_TO_CS(cs);

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

void vt_drcs_add_picture(vt_drcs_font_t *font, int id, u_int offset,
                         int beg_idx /* DRCSMMv2: ch - 0x20, DRCSMMv3: ch - 0x21 */,
                         u_int num_cols, u_int num_rows,
                         u_int num_cols_small, u_int num_rows_small) {
  u_int slot = DRCS_CH_TO_SLOT(beg_idx);

  /* 'offset > UINT32_MAX' is not necessary to check. */
  if (num_cols > UINT16_MAX || num_rows > UINT16_MAX ||
      slot >= BL_ARRAY_SIZE(font->pictures)) {
    return;
  }

  font->pictures[slot].id = id;
  font->pictures[slot].offset = offset;
  if (slot > 0) {
    font->pictures[slot].beg_idx = (beg_idx & 0x7f);
  } else {
    font->pictures[slot].beg_idx = beg_idx;
  }
  font->pictures[slot].num_rows = num_rows;
  font->pictures[slot].num_cols = num_cols;
  font->pictures[slot].num_cols_small = num_cols_small;
  font->pictures[slot].num_rows_small = num_rows_small;
}

int vt_drcs_get_picture(vt_drcs_font_t *font, int *id, int *pos, u_int ch) {
  u_int slot;

  if (ch >= 0x121) {
    slot = DRCS_CH_TO_SLOT(ch);
    ch &= 0x7f;
    if (ch >= 0x21) {
      ch -= 0x21;
    } else {
      return 0;
    }
  } else if (ch >= 0x20) {
    slot = 0;
    ch = (ch & 0x7f) - 0x20;
  } else {
    return 0;
  }

  if (vt_drcs_has_picture(font, slot) && ch >= font->pictures[slot].beg_idx) {
    ch += font->pictures[slot].offset;

    /* See MAKE_INLINEPIC_POS() in ui_picture.h */
    *pos = (ch % font->pictures[slot].num_cols_small) * font->pictures[slot].num_rows +
           (ch / font->pictures[slot].num_cols_small);
    *id = font->pictures[slot].id;

    return 1;
  } else {
    return 0;
  }
}

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch) {
  if (IS_DRCS(ch->cs)) {
    u_char *bytes = ch->ch;

    if (version == 2) {
      if (IS_CS94SB(ch->cs)) {
        bytes[2] = CS94SB_FT(ch->cs);
        bytes[3] = bytes[0] & 0x7f;
      } else {
        bytes[2] = CS96SB_FT(ch->cs);
        bytes[3] = bytes[0] | 0x80;
      }
      bytes[1] = 0x10;
      bytes[0] = 0x00;
    } else if (/* version == 3 && */ ch->size == 2) {
      if (0x1 <= bytes[0] && bytes[0] <= 0x10 && 0x21 <= bytes[1] && bytes[1] <= 0x7e &&
          IS_CS94SB(ch->cs)) {
        ef_int_to_bytes(bytes, 4,
                        0x100000 + ((bytes[0] - 1) * 63 + (CS94SB_FT(ch->cs) - 0x40)) * 94 +
                        (bytes[1] - 0x21));
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  } else {
    return 0;
  }

  ch->cs = ISO10646_UCS4_1;
  ch->size = 4;
  ch->property = 0;

  return 1;
}

int vt_convert_unicode_pua_to_drcs(ef_char_t *ch) {
  u_char *bytes = ch->ch;

  if (version == 2) {
    if (bytes[1] == 0x10 && 0x30 <= bytes[2] && bytes[2] <= 0x7e && bytes[0] == 0x00) {
      if (0x20 <= bytes[3] && bytes[3] <= 0x7f) {
        ch->cs = CS_TO_DRCS(CS94SB_ID(bytes[2]));
      } else if (0xa0 <= bytes[3] && bytes[3] <= 0xff) {
        ch->cs = CS_TO_DRCS(CS96SB_ID(bytes[2]));
      } else {
        return 0;
      }

      bytes[0] = bytes[3];
      ch->size = 1;
      ch->property = 0; /* Ignore EF_AWIDTH */

      return 1;
    }
  } else /* if (version == 3) */ {
    if (bytes[0] == 0x00 && bytes[1] == 0x10) {
      u_int ucs = ef_char_to_int(ch);
      u_int intermed = ((ucs - 0x100000) / 94) / 63 + 0x20;
      u_int ft = ((ucs - 0x100000) / 94) % 63 + 0x40;
      u_int c = (ucs - 0x100000) % 94 + 0x21;

      ef_int_to_bytes(bytes, 2, ((intermed - 0x1f) << 8) + c);
      ch->cs = CS_TO_DRCS(CS94SB_ID(ft));
      ch->size = 2;
      ch->property = 0;

      return 1;
    }
  }

  return 0;
}
