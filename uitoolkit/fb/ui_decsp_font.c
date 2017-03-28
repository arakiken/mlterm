/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_decsp_font.h"

#include <stdio.h>
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_util.h>

/* --- static functions --- */

static void draw_check(u_char *fb, u_int width_bytes, u_int height) {
  int y;
  u_char check1 = 0xaa;
  u_char check2 = 0x55;

  for (y = 0; y < height; y++) {
    if ((y & 1) == 0) {
      memset(fb, check1, width_bytes);
    } else {
      memset(fb, check2, width_bytes);
    }
    fb += width_bytes;
  }
}

static void draw_vline(u_char *fb, u_int width_bytes, int x, int beg_y, int end_y) {
  int y;
  int mask;

  fb += (width_bytes * beg_y);
  fb += (x / 8);
  mask = (1 << (8 - (x&7) - 1));
  for (y = beg_y; y <= end_y; y++) {
    *fb |= mask;
    fb += width_bytes;
  }
}

static void draw_hline(u_char *fb, u_int width_bytes, int beg_x, int end_x, int y) {
  int x;

  fb += (width_bytes * y);
  for (x = beg_x; x <= end_x; x++) {
    fb[x/8] |= (1 << (8 - (x&7) - 1));
  }
}

static void draw_diamond(u_char *fb, u_int width_bytes, u_int width, u_int height) {
  int x;
  int y;
  u_int mod = height / 2;

  for (y = 0; y < mod; y++) {
    x = (width * (mod - y - 1) / mod + 1) / 2;
    draw_hline(fb, width_bytes, x, width - x - 1, y);
  }

  for (; y < height; y++) {
    x = (width * (y - mod) / mod + 1) / 2;
    draw_hline(fb, width_bytes, x, width - x - 1, y);
  }
}

/* --- global functions --- */

int ui_load_decsp_xfont(XFontStruct *xfont, const char *decsp_id) {
  u_int width;
  u_int height;
  int count;
  u_int glyph_size;
  u_char *p;

  if (sscanf(decsp_id, "decsp-%dx%d", &width, &height) != 2) {
    return 0;
  }

  xfont->file = strdup(decsp_id);

  xfont->glyph_width_bytes = (width + 7) / 8;
  xfont->width = xfont->width_full = width;
  xfont->height = height;
  xfont->ascent = height - height / 6;
  xfont->min_char_or_byte2 = 0x1;
  xfont->max_char_or_byte2 = 0x1e;
  xfont->num_of_glyphs = 30;
  glyph_size = xfont->glyph_width_bytes * height;
  xfont->glyphs = calloc(30, glyph_size);
  xfont->glyph_offsets = calloc(30, sizeof(xfont->glyph_offsets[0]));
  xfont->glyph_indeces = calloc(30, sizeof(xfont->glyph_indeces[0]));
  xfont->ref_count = 1;

  for (count = 0; count < 30; count++) {
    xfont->glyph_offsets[count] = count * glyph_size;
    xfont->glyph_indeces[count] = count;
  }

  /*
   * Glyph map
   *
   *        Used , Used , None , None , None , None , None ,
   * None , None , None , Used , Used , Used , Used , Used ,
   * Used , Used , Used , Used , Used , Used , Used , Used ,
   * Used , Used , None , None , None , None , Used
   */
  p = xfont->glyphs;
  draw_diamond(p, xfont->glyph_width_bytes, width, height);

  p += glyph_size;
  draw_check(p, xfont->glyph_width_bytes, height);

  p += (glyph_size * (0x0a - 0x01));
  draw_hline(p, xfont->glyph_width_bytes, 0, width / 2 - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height / 2 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width / 2 - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, height / 2 - 1, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, width / 2 - 1, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, height / 2 - 1, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, width / 2 - 1, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height / 2 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, 0);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height / 4 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height / 2 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height * 3 / 4 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, width / 2 - 1, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width / 2 - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height / 2 - 1);

  p += glyph_size;
  draw_hline(p, xfont->glyph_width_bytes, 0, width - 1, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, height / 2 - 1, height - 1);

  p += glyph_size;
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, 0, height - 1);

  p += (glyph_size * (0x1d - 0x18));
  draw_hline(p, xfont->glyph_width_bytes, width / 2 - 2, width / 2, height / 2 - 1);
  draw_vline(p, xfont->glyph_width_bytes, width / 2 - 1, height / 2 - 2, height / 2);

  return 1;
}

/* unload_pcf() in ui_font.c completely frees memory. */
