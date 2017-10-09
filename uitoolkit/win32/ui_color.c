/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_color.h"

#include <string.h> /* memcpy,strcmp */
#include <stdio.h>  /* sscanf */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include <vt_color.h>

/* --- global functions --- */

int ui_load_named_xcolor(ui_display_t *disp, ui_color_t *xcolor, char *name) {
  vt_color_t color;
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  if (vt_color_parse_rgb_name(&red, &green, &blue, &alpha, name)) {
    return ui_load_rgb_xcolor(disp, xcolor, red, green, blue, alpha);
  }

  if ((color = vt_get_color(name)) != VT_UNKNOWN_COLOR && IS_VTSYS_BASE_COLOR(color)) {
    /*
     * 0 : 0x00, 0x00, 0x00
     * 1 : 0xff, 0x00, 0x00
     * 2 : 0x00, 0xff, 0x00
     * 3 : 0xff, 0xff, 0x00
     * 4 : 0x00, 0x00, 0xff
     * 5 : 0xff, 0x00, 0xff
     * 6 : 0x00, 0xff, 0xff
     * 7 : 0xe5, 0xe5, 0xe5
     */
    red = (color & 0x1) ? 0xff : 0;
    green = (color & 0x2) ? 0xff : 0;
    blue = (color & 0x4) ? 0xff : 0;
  } else {
    if (strcmp(name, "gray") == 0) {
      red = green = blue = 190;
    } else if (strcmp(name, "lightgray") == 0) {
      red = green = blue = 211;
    } else {
      return 0;
    }
  }

  return ui_load_rgb_xcolor(disp, xcolor, red, green, blue, 0xff);
}

int ui_load_rgb_xcolor(ui_display_t *disp, ui_color_t *xcolor, u_int8_t red, u_int8_t green,
                       u_int8_t blue, u_int8_t alpha) {
  xcolor->pixel = RGB(red, green, blue) | (alpha << 24);

  return 1;
}

void ui_unload_xcolor(ui_display_t *disp, ui_color_t *xcolor) {}

void ui_get_xcolor_rgba(u_int8_t *red, u_int8_t *green, u_int8_t *blue,
                        u_int8_t *alpha /* can be NULL */, ui_color_t *xcolor) {
  *red = GetRValue(xcolor->pixel);
  *green = GetGValue(xcolor->pixel);
  *blue = GetBValue(xcolor->pixel);
  if (alpha) {
    *alpha = (xcolor->pixel >> 24) & 0xff;
  }
}

int ui_xcolor_fade(ui_display_t *disp, ui_color_t *xcolor, u_int fade_ratio) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  ui_get_xcolor_rgba(&red, &green, &blue, &alpha, xcolor);

  red = (red * fade_ratio) / 100;
  green = (green * fade_ratio) / 100;
  blue = (blue * fade_ratio) / 100;

  ui_unload_xcolor(disp, xcolor);

  return ui_load_rgb_xcolor(disp, xcolor, red, green, blue, alpha);
}
