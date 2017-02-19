/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_COLOR_H__
#define __VT_COLOR_H__

#include <pobl/bl_types.h>

#define MAX_VTSYS_COLORS 16
#define MAX_BASIC_VTSYS_COLORS 8
#define MAX_256_COLORS 240
#define MAX_EXT_COLORS 240
#define MAX_256EXT_COLORS (MAX_256_COLORS + MAX_EXT_COLORS)

/* same as 0 <= color <= 0x7 */
#define IS_VTSYS_BASE_COLOR(color) ((unsigned int)(color) <= 0x7)
/* same as 0 <= color <= 0xf */
#define IS_VTSYS_COLOR(color) ((unsigned int)(color) <= 0xf)
#define IS_256_COLOR(color) (0x10 <= (color) && (color) <= 0xff)
#define IS_VTSYS256_COLOR(color) ((unsigned int)(color) <= 0xff)
#define IS_EXT_COLOR(color) (0x100 <= (color) && (color) <= 0x1ef)
#define IS_256EXT_COLOR(color) (0x10 <= (color) && (color) <= 0x1ef)
#define IS_VALID_COLOR_EXCEPT_SPECIAL_COLORS(color) ((unsigned int)(color) <= 0x1ef)
#define IS_FG_BG_COLOR(color) (0x1f0 <= (color) && (color) <= 0x1f1)
#define IS_ALT_COLOR(color) (0x1f2 <= (color))
#define EXT_COLOR_TO_INDEX(color) ((color)-MAX_VTSYS_COLORS - MAX_256_COLORS)
#define INDEX_TO_EXT_COLOR(color) ((color) + MAX_VTSYS_COLORS + MAX_256_COLORS)
#define COLOR_DISTANCE(diff_r, diff_g, diff_b) \
  ((diff_r) * (diff_r)*9 + (diff_g) * (diff_g)*30 + (diff_b) * (diff_b))
/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
#define COLOR_DISTANCE_THRESHOLD 640

/* XXX If these members are changed, modify init() in MLTerm.java. */
typedef enum vt_color {
  VT_UNKNOWN_COLOR = -1,

  /*
   * Don't change this order, which vt_parser.c(change_char_attr etc) and
   * x_color_cache.c etc depend on.
   */
  VT_BLACK = 0x0,
  VT_RED = 0x1,
  VT_GREEN = 0x2,
  VT_YELLOW = 0x3,
  VT_BLUE = 0x4,
  VT_MAGENTA = 0x5,
  VT_CYAN = 0x6,
  VT_WHITE = 0x7,

  VT_BOLD_COLOR_MASK = 0x8,

  /*
   * 0x8 - 0xf: bold vt colors.
   */

  /*
   * 0x10 - 0xff: 240 colors.
   */

  /*
   * 0x100 - 0x1ef: 241-480 colors.
   */

  VT_FG_COLOR = 0x1f0,
  VT_BG_COLOR = 0x1f1,

  VT_BOLD_COLOR = 0x1f2,
  VT_ITALIC_COLOR = 0x1f3,
  VT_UNDERLINE_COLOR = 0x1f4,
  VT_BLINKING_COLOR = 0x1f5,
  VT_CROSSED_OUT_COLOR = 0x1f6,

} vt_color_t;

void vt_set_color_mode(const char *mode);

char *vt_get_color_mode(void);

int vt_color_config_init(void);

int vt_color_config_final(void);

int vt_customize_color_file(char *color, char *rgb, int save);

char *vt_get_color_name(vt_color_t color);

vt_color_t vt_get_color(const char *name);

int vt_get_color_rgba(vt_color_t color, u_int8_t *red, u_int8_t *green, u_int8_t *blue,
                      u_int8_t *alpha);

int vt_color_parse_rgb_name(u_int8_t *red, u_int8_t *green, u_int8_t *blue, u_int8_t *alpha,
                            const char *name);

u_int vt_get_closest_256_color(vt_color_t *closest, u_int *min_diff, u_int8_t red, u_int8_t green,
                               u_int8_t blue, int threshold);

vt_color_t vt_get_closest_color(u_int8_t red, u_int8_t green, u_int8_t blue);

int vt_ext_color_is_changed(vt_color_t color);

#endif
