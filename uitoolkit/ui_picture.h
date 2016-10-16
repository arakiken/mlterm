/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_PICTURE_H__
#define __UI_PICTURE_H__

#include <pobl/bl_types.h> /* u_int16_t */
#include <vt_term.h>

#include "ui.h" /* XA_PIXMAP */
#include "ui_window.h"

typedef struct ui_picture_modifier {
  u_int16_t brightness; /* 0 - 65535 */
  u_int16_t contrast;   /* 0 - 65535 */
  u_int16_t gamma;      /* 0 - 65535 */

  u_int8_t alpha; /* 0 - 255 */
  u_int8_t blend_red;
  u_int8_t blend_green;
  u_int8_t blend_blue;

} ui_picture_modifier_t;

typedef struct ui_picture {
  Display* display;
  ui_picture_modifier_t* mod;
  char* file_path;
  u_int width;
  u_int height;

  Pixmap pixmap;

  u_int ref_count;

} ui_picture_t;

typedef struct ui_icon_picture {
  ui_display_t* disp;
  char* file_path;

  Pixmap pixmap;
  PixmapMask mask;
  u_int32_t* cardinal;

  u_int ref_count;

} ui_icon_picture_t;

typedef struct ui_inline_picture {
  Pixmap pixmap;
  PixmapMask mask;
  char* file_path;
  u_int width;
  u_int height;
  ui_display_t* disp;
  vt_term_t* term;
  u_int8_t col_width;
  u_int8_t line_height;

  int16_t next_frame;
  u_int16_t weighting;

} ui_inline_picture_t;

#define MAX_INLINE_PICTURES (1 << PICTURE_ID_BITS)
#define MAKE_INLINEPIC_POS(col, row, num_of_rows) ((col) * (num_of_rows) + (row))
#define INLINEPIC_AVAIL_ROW -(MAX_INLINE_PICTURES * 2)

#ifdef NO_IMAGE

#define ui_picture_display_opened(display) (0)
#define ui_picture_display_closed(display) (0)
#define ui_picture_modifiers_equal(a, b) (0)
#define ui_acquire_bg_picture(win, mod, file_path) (NULL)
#define ui_release_picture(pic) (0)
#define ui_acquire_icon_picture(disp, file_path) (NULL)
#define ui_release_icon_picture(pic) (0)
#define ui_load_inline_picture(disp, file_path, width, height, col_width, line_height, term) (-1)
#define ui_get_inline_picture(idx) (NULL)

#else

/* defined in c_sixel.c */
u_int32_t* ui_set_custom_sixel_palette(u_int32_t* palette);

int ui_picture_display_opened(Display* display);

int ui_picture_display_closed(Display* display);

int ui_picture_modifiers_equal(ui_picture_modifier_t* a, ui_picture_modifier_t* b);

ui_picture_t* ui_acquire_bg_picture(ui_window_t* win, ui_picture_modifier_t* mod, char* file_path);

int ui_release_picture(ui_picture_t* pic);

ui_icon_picture_t* ui_acquire_icon_picture(ui_display_t* disp, char* file_path);

int ui_release_icon_picture(ui_icon_picture_t* pic);

int ui_load_inline_picture(ui_display_t* disp, char* file_path, u_int* width, u_int* height,
                           u_int col_width, u_int line_height, vt_term_t* term);

ui_inline_picture_t* ui_get_inline_picture(int idx);

int ui_add_frame_to_animation(int prev_idx, int next_idx);

int ui_animate_inline_pictures(vt_term_t* term);

int ui_load_tmp_picture(ui_display_t* disp, char* file_path, Pixmap* pixmap, PixmapMask* mask,
                        u_int* width, u_int* height);

void ui_delete_tmp_picture(ui_display_t* disp, Pixmap pixmap, PixmapMask mask);

#endif

#define ui_picture_modifier_is_normal(pic_mod) (ui_picture_modifiers_equal((pic_mod), NULL))

#endif
