/*
 *	$Id$
 */

#include "../ui_color.h"

#include <string.h> /* memcpy,strcmp */
#include <stdio.h>  /* sscanf */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h>
#include <vt_color.h>

/* --- static functions --- */

static void native_color_to_xcolor(ui_color_t* xcolor, XColor* ncolor) {
  xcolor->pixel = ncolor->pixel;
  xcolor->red = (ncolor->red >> 8) & 0xff;
  xcolor->green = (ncolor->green >> 8) & 0xff;
  xcolor->blue = (ncolor->blue >> 8) & 0xff;
  xcolor->alpha = 0xff;
}

static int alloc_closest_xcolor_pseudo(ui_display_t* disp, int red, /* 0 to 0xffff */
                                       int green,                   /* 0 to 0xffff */
                                       int blue,                    /* 0 to 0xffff */
                                       ui_color_t* ret_xcolor) {
  XColor* all_colors; /* colors exist in the shared color map */
  XColor closest_color;

  int closest_index = -1;
  u_long min_diff = 0xffffffff;
  u_long diff;
  int diff_r, diff_g, diff_b;
  int ncells = disp->visual->map_entries;
  int i;

  /* FIXME: When visual class is StaticColor, should not be return? */
  if (!disp->visual->class == PseudoColor && !disp->visual->class == GrayScale) {
    return 0;
  }

  if ((all_colors = malloc(ncells * sizeof(XColor))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

    return 0;
  }

  /* get all colors from default colormap */
  for (i = 0; i < ncells; i++) {
    all_colors[i].pixel = i;
  }
  XQueryColors(disp->display, disp->colormap, all_colors, ncells);

  red >>= 8;
  green >>= 8;
  blue >>= 8;

  /* find the closest color */
  for (i = 0; i < ncells; i++) {
    diff_r = red - (all_colors[i].red >> 8);
    diff_g = green - (all_colors[i].green >> 8);
    diff_b = blue - (all_colors[i].blue >> 8);
    diff = COLOR_DISTANCE(diff_r, diff_g, diff_b);

    if (diff < min_diff) {
      min_diff = diff;
      closest_index = i;

      /* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
      if (diff < COLOR_DISTANCE_THRESHOLD) {
        break;
      }
    }
  }

  if (closest_index == -1) /* unable to find closest color */
  {
    closest_color.red = 0;
    closest_color.green = 0;
    closest_color.blue = 0;
  } else {
    closest_color.red = all_colors[closest_index].red;
    closest_color.green = all_colors[closest_index].green;
    closest_color.blue = all_colors[closest_index].blue;
  }

  closest_color.flags = DoRed | DoGreen | DoBlue;
  free(all_colors);

  if (!XAllocColor(disp->display, disp->colormap, &closest_color)) {
    return 0;
  }

  ret_xcolor->pixel = closest_color.pixel;
  ret_xcolor->red = (closest_color.red >> 8) & 0xff;
  ret_xcolor->green = (closest_color.green >> 8) & 0xff;
  ret_xcolor->blue = (closest_color.blue >> 8) & 0xff;
  ret_xcolor->alpha = 0xff;

  return 1;
}

/* --- global functions --- */

int ui_load_named_xcolor(ui_display_t* disp, ui_color_t* xcolor, char* name) {
  XColor near;
  XColor exact;
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  if (vt_color_parse_rgb_name(&red, &green, &blue, &alpha, name)) {
    return ui_load_rgb_xcolor(disp, xcolor, red, green, blue, alpha);
  }

  if (!XAllocNamedColor(disp->display, disp->colormap, name, &near, &exact)) {
    /* try to find closest color */
    if (XParseColor(disp->display, disp->colormap, name, &exact)) {
      return alloc_closest_xcolor_pseudo(disp, exact.red, exact.green, exact.blue, xcolor);
    } else {
      return 0;
    }
  }

  native_color_to_xcolor(xcolor, &near);

  return 1;
}

int ui_load_rgb_xcolor(ui_display_t* disp, ui_color_t* xcolor, u_int8_t red, u_int8_t green,
                       u_int8_t blue, u_int8_t alpha) {
  if (disp->depth == 32 && alpha < 0xff) {
    xcolor->red = red;
    xcolor->green = green;
    xcolor->blue = blue;
    xcolor->alpha = alpha;
    /* XXX */
    xcolor->pixel = (alpha << 24) | (((u_int)red * (u_int)alpha / 256) << 16) |
                    (((u_int)green * (u_int)alpha / 256) << 8) |
                    (((u_int)blue * (u_int)alpha / 256));
  } else {
    XColor ncolor;

    ncolor.red = (red << 8) + red;
    ncolor.green = (green << 8) + green;
    ncolor.blue = (blue << 8) + blue;
    ncolor.flags = 0;

    if (!XAllocColor(disp->display, disp->colormap, &ncolor)) {
      /* try to find closest color */
      return alloc_closest_xcolor_pseudo(disp, ncolor.red, ncolor.green, ncolor.blue, xcolor);
    }

    native_color_to_xcolor(xcolor, &ncolor);
  }

  return 1;
}

int ui_unload_xcolor(ui_display_t* disp, ui_color_t* xcolor) {
#if 0
  u_long pixel[1];

  pixel[0] = xcolor->pixel;

  XFreeColors(disp->display, disp->colormap, pixel, 1, 0);
#endif

  return 1;
}

int ui_get_xcolor_rgba(u_int8_t* red, u_int8_t* green, u_int8_t* blue,
                       u_int8_t* alpha, /* can be NULL */
                       ui_color_t* xcolor) {
  *red = xcolor->red;
  *green = xcolor->green;
  *blue = xcolor->blue;

  if (alpha) {
    *alpha = xcolor->alpha;
  }

  return 1;
}

int ui_xcolor_fade(ui_display_t* disp, ui_color_t* xcolor, u_int fade_ratio) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  ui_get_xcolor_rgba(&red, &green, &blue, &alpha, xcolor);

#if 0
  bl_msg_printf("Fading R%d G%d B%d => ", red, green, blue);
#endif

  red = (red * fade_ratio) / 100;
  green = (green * fade_ratio) / 100;
  blue = (blue * fade_ratio) / 100;

  ui_unload_xcolor(disp, xcolor);

#if 0
  bl_msg_printf("R%d G%d B%d\n", red, green, blue);
#endif

  return ui_load_rgb_xcolor(disp, xcolor, red, green, blue, alpha);
}
