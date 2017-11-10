/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_drcs.h"

#include <string.h> /* memset */
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>

/* --- static variables --- */

static vt_drcs_t *cur_drcs;
static ef_charset_t cached_font_cs = UNKNOWN_CS;
static vt_drcs_font_t *cached_font;

/* --- static functions --- */

static void drcs_final(vt_drcs_font_t *font) {
  int idx;

  for (idx = 0; idx < 0x5f; idx++) {
    free(font->glyphs[idx]);
  }

  memset(font->glyphs, 0, sizeof(font->glyphs));

  if (cached_font_cs == font->cs) {
    /* Clear cache in vt_drcs_get(). */
    cached_font_cs = UNKNOWN_CS;
  }
}

/* --- global functions --- */

void vt_drcs_select(vt_drcs_t *drcs) {
  cur_drcs = drcs;
  /* Clear cache in vt_drcs_get(). */
  cached_font_cs = UNKNOWN_CS;
}

vt_drcs_font_t *vt_drcs_get_font(ef_charset_t cs, int create) {
  u_int count;
  void *p;

  if (cs == cached_font_cs) {
    if (cached_font || !create) {
      return cached_font;
    }
  } else if (!cur_drcs) {
    return NULL;
  } else {
    for (count = 0; count < cur_drcs->num_fonts; count++) {
      if (cur_drcs->fonts[count].cs == cs) {
        return &cur_drcs->fonts[count];
      }
    }
  }

  if (!create ||
      /* XXX leaks */
      !(p = realloc(cur_drcs->fonts, sizeof(vt_drcs_font_t) * (cur_drcs->num_fonts + 1)))) {
    return NULL;
  }

  cur_drcs->fonts = p;
  memset(cur_drcs->fonts + cur_drcs->num_fonts, 0, sizeof(vt_drcs_font_t));
  cached_font_cs = cur_drcs->fonts[cur_drcs->num_fonts].cs = cs;

  return (cached_font = &cur_drcs->fonts[cur_drcs->num_fonts++]);
}

char *vt_drcs_get_glyph(ef_charset_t cs, u_char idx) {
  vt_drcs_font_t *font;

  /* msb can be set in vt_parser.c (e.g. ESC(I (JISX0201 kana)) */
  if ((font = vt_drcs_get_font(cs, 0)) && 0x20 <= (idx & 0x7f)) {
    return font->glyphs[(idx & 0x7f) - 0x20];
  } else {
    return NULL;
  }
}

void vt_drcs_final(ef_charset_t cs) {
  u_int count;

  for (count = 0; count < cur_drcs->num_fonts; count++) {
    if (cur_drcs->fonts[count].cs == cs) {
      drcs_final(cur_drcs->fonts + count);
      cur_drcs->fonts[count] = cur_drcs->fonts[--cur_drcs->num_fonts];

      return;
    }
  }
}

void vt_drcs_final_full(void) {
  u_int count;

  for (count = 0; count < cur_drcs->num_fonts; count++) {
    drcs_final(cur_drcs->fonts + count);
  }

  free(cur_drcs->fonts);
  cur_drcs->fonts = NULL;
  cur_drcs->num_fonts = 0;
  cur_drcs = NULL;
}

int vt_drcs_add(vt_drcs_font_t *font, int idx, const char *seq, u_int width, u_int height) {
  free(font->glyphs[idx]);

  if ((font->glyphs[idx] = malloc(2 + strlen(seq) + 1))) {
    font->glyphs[idx][0] = width;
    font->glyphs[idx][1] = height;
    strcpy(font->glyphs[idx] + 2, seq);
  }

  return 1;
}

int vt_convert_drcs_to_unicode_pua(ef_char_t *ch) {
  if (vt_drcs_get_glyph(ch->cs, ch->ch[0])) {
    ch->ch[3] = ch->ch[0];
    ch->ch[2] = ch->cs + 0x30; /* see CS94SB_ID() in ef_charset.h */
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

  if (c[1] == 0x10 && 0x40 <= c[2] && c[2] <= 0x7e && 0x20 <= c[3] && c[3] <= 0x7f &&
      c[0] == 0x00) {
    c[0] = c[3];
    ch->cs = CS94SB_ID(c[2]);
    ch->size = 1;
    ch->property = 0;

    return 1;
  } else {
    return 0;
  }
}
