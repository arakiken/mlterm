/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_im_candidate_screen.h"

#ifdef USE_IM_PLUGIN

#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>
#include <vt_str.h>
#include <vt_parser.h>
#include "ui_draw_str.h"

#ifdef USE_CONSOLE
#define MARGIN 0
#define LINE_SPACE 0
#else
#define MARGIN 3
#define LINE_SPACE 2
#endif

#define VISIBLE_INDEX(n, p, i, t, l) \
  do {                               \
    (t) = ((i) / p) * p;             \
    (l) = (t) + p - 1;               \
    if ((l) > (n)-1) {               \
      (l) = (n)-1;                   \
    }                                \
  } while (0)

/*
 * calculate max number of digits
 */

#define NUM_OF_DIGITS(n, d) \
  do {                      \
    u_int d2 = (d);         \
    (n) = 1;                \
    while (((d2) /= 10)) {  \
      (n)++;                \
    }                       \
  } while (0)

#define INVALID_INDEX (cand_screen->num_candidates)

#ifdef USE_WAYLAND
#define DISPLAY(cand_screen) ((cand_screen)->window.disp->display->parent)
#else
#define DISPLAY(cand_screen) ((cand_screen)->window.disp)
#endif

/* --- static variables --- */

/* --- static functions --- */

static int free_candidates(ui_im_candidate_t *candidates, u_int num_candidates) {
  ui_im_candidate_t *c;
  int i;

  if (candidates == NULL || num_candidates == 0) {
    return 1;
  }

  for (i = 0, c = candidates; i < num_candidates; i++, c++) {
    vt_str_delete(c->chars, c->num_chars);

    c->filled_len = 0;
    c->num_chars = 0;
  }

  free(candidates);

  return 1;
}

static u_int candidate_width(ui_font_manager_t *font_man, ui_im_candidate_t *candidate) {
  u_int width;
  int i;

  if (candidate->chars == NULL || candidate->filled_len == 0) {
    return 0;
  }

  width = 0;
  ui_font_manager_set_attr(font_man, 0, 0);

  for (i = 0; i < candidate->filled_len; i++) {
    ui_font_t *xfont;

    xfont = ui_get_font(font_man, vt_char_font(&candidate->chars[i]));

    width += ui_calculate_char_width(xfont, vt_char_code(&candidate->chars[i]),
                                     vt_char_cs(&candidate->chars[i]), NULL);
  }

  return width;
}

static u_int max_candidate_width(ui_font_manager_t *font_man, ui_im_candidate_t *candidates,
                                 u_int num_candidates) {
  u_int max_width;
  int i;

  max_width = 0;

  for (i = 0; i < num_candidates; i++) {
    u_int width;

    width = candidate_width(font_man, &candidates[i]);

    if (width > max_width) {
      max_width = width;
    }
  }

  return max_width;
}

static u_int total_candidate_width(ui_font_manager_t *font_man, ui_im_candidate_t *candidates,
                                   u_int to, u_int from) {
  u_int total_width;
  int i;

  total_width = 0;

  for (i = to; i <= from; i++) {
    total_width += candidate_width(font_man, &candidates[i]);
  }

  return total_width;
}

static void adjust_window_position_by_size(ui_im_candidate_screen_t *cand_screen, int *x, int *y) {
#ifdef USE_WAYLAND
  if (ACTUAL_WIDTH(&cand_screen->window) > DISPLAY(cand_screen)->width) {
    /* do nothing */
  } else
#endif
  if (*x + ACTUAL_WIDTH(&cand_screen->window) > DISPLAY(cand_screen)->width) {
    if (cand_screen->is_vertical_term) {
      /* ui_im_candidate_screen doesn't know column width. */
      *x -= (ACTUAL_WIDTH(&cand_screen->window) + cand_screen->line_height);
    } else {
      *x = DISPLAY(cand_screen)->width - ACTUAL_WIDTH(&cand_screen->window);
    }
  }

#ifdef USE_WAYLAND
  if (ACTUAL_HEIGHT(&cand_screen->window) > DISPLAY(cand_screen)->height) {
    /* do nothing */
  } else
#endif
  if (*y + ACTUAL_HEIGHT(&cand_screen->window) > DISPLAY(cand_screen)->height) {
    *y -= ACTUAL_HEIGHT(&cand_screen->window);

    if (!cand_screen->is_vertical_term) {
      *y -= cand_screen->line_height;
    }
  }
}

static void resize(ui_im_candidate_screen_t *cand_screen, u_int width, u_int height) {
  if (ui_window_resize(&cand_screen->window, width, height, 0)) {
    int x;
    int y;

    x = cand_screen->x;
    y = cand_screen->y;
    adjust_window_position_by_size(cand_screen, &x, &y);

    if (x != cand_screen->window.x || y != cand_screen->window.y) {
      ui_window_move(&cand_screen->window, x, y);
    }

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
    /* resized but position is not changed. */
    ui_display_reset_input_method_window();
    ui_window_draw_rect_frame(&cand_screen->window, -MARGIN, -MARGIN,
                              cand_screen->window.width + MARGIN - 1,
                              cand_screen->window.height + MARGIN - 1);
#endif
  }
}

static void draw_str(ui_im_candidate_screen_t *cand_screen, vt_char_t *str, u_int len, int x,
                     int row, u_int font_height, u_int font_ascent, int to_eol) {
  (to_eol ? ui_draw_str_to_eol : ui_draw_str)(
      &cand_screen->window, cand_screen->font_man, cand_screen->color_man, str, len, x,
      (font_height + LINE_SPACE) * row, font_height + LINE_SPACE, font_ascent + LINE_SPACE / 2,
      LINE_SPACE / 2, 1 /* no need to draw underline */, 0);
}

#define MAX_NUM_OF_DIGITS 4 /* max is 9999. enough? */

static void draw_screen_vertical(ui_im_candidate_screen_t *cand_screen, u_int top, u_int last,
                                 u_int draw_index, int do_resize) {
  ui_font_t *xfont;
  u_int i;
  u_int num_digits;
  u_int win_width;
  u_int win_height;
  vt_char_t *p;

  if (cand_screen->num_candidates > cand_screen->num_per_window) {
    NUM_OF_DIGITS(num_digits, cand_screen->num_per_window);
  } else {
    NUM_OF_DIGITS(num_digits, last);
  }

  /*
   * resize window
   */

  /*
   * width : [digit] + [space] + [max_candidate_width]
   *         or width of "index/total"
   * height: ([ascii height] + [line space]) x (num_per_window + 1)
   *         or ([ascii height] + [line space)] x num_candidates
   *   +-------------------+
   *   |1  cand0           |\
   *   |2  cand1           | \
   *   |3  cand2           |  |
   *   |4  cand3           |  |
   *   |5  cand4           |  |-- num_per_window
   *   |6  cand5           |  |
   *   |7  cand6           |  |
   *   |8  cand7           |  |
   *   |9  widest candidate| /
   *   |10 cand9           |/
   *   |    index/total    |--> show if total > num_per_window
   *   +-------------------+
   */

  xfont = ui_get_usascii_font(cand_screen->font_man);

  if (do_resize) {
    u_int width;
    u_int num;

    /* width of window */
    win_width = xfont->width * (num_digits + 1);
    win_width +=
        max_candidate_width(cand_screen->font_man, &cand_screen->candidates[top], last - top + 1);

    NUM_OF_DIGITS(num, cand_screen->num_candidates);
    if ((width = (num * 2 + 1) * xfont->width) > win_width) {
      win_width = width;
    }

    /* height of window */
    if (cand_screen->num_candidates > cand_screen->num_per_window) {
      win_height = (xfont->height + LINE_SPACE) * (cand_screen->num_per_window + 1);
    } else {
      win_height = (xfont->height + LINE_SPACE) * cand_screen->num_candidates;
    }

    resize(cand_screen, win_width, win_height);
  } else {
    win_width = cand_screen->window.width;
    win_height = cand_screen->window.height;
  }

/*
 * digits and candidates
 */

#ifdef DEBUG
  if (num_digits > MAX_NUM_OF_DIGITS) {
    bl_warn_printf(BL_DEBUG_TAG " num_digits %d is too large.\n", num_digits);

    return;
  }
#endif

  if (draw_index != INVALID_INDEX) {
    i = draw_index;
    last = draw_index;
  } else {
    i = top;
  }

  for (; i <= last; i++) {
    u_char digit[MAX_NUM_OF_DIGITS + 1];
    vt_char_t digit_str[MAX_NUM_OF_DIGITS + 1];
    int j;

    /*
     * digits
     * +----------+
     * |1 cand0   |
     *  ^^
     */
    if (cand_screen->candidates[i].info) {
      char byte2;

      byte2 = (cand_screen->candidates[i].info >> 8) & 0xff;
      if (num_digits > 2) {
        num_digits = 2;
      }

      bl_snprintf(digit, MAX_NUM_OF_DIGITS + 1, "%c%c   ", cand_screen->candidates[i].info & 0xff,
                  byte2 ? byte2 : ' ');
    } else {
      bl_snprintf(digit, MAX_NUM_OF_DIGITS + 1, "%i    ", i - top + 1);
    }

    p = digit_str;
    for (j = 0; j < num_digits + 1; j++) {
      vt_char_init(p);
      vt_char_set(p++, digit[j], US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0, 0);
    }

    draw_str(cand_screen, digit_str, num_digits + 1, 0, i - top, xfont->height, xfont->ascent,
             0);

    vt_str_final(digit_str, num_digits + 1);

    /*
     * candidate
     * +----------+
     * |1 cand0   |
     *    ^^^^^
     */
    draw_str(cand_screen, cand_screen->candidates[i].chars, cand_screen->candidates[i].filled_len,
             xfont->width * (num_digits + 1), i - top, xfont->height, xfont->ascent, 1);
  }

  if (draw_index != INVALID_INDEX) {
    return;
  }

  /*
   * |7 cand6         |
   * |8 last candidate|
   * |                |\
   * |                | }-- clear this area
   * |                |/
   * +----------------+
   */
  if (cand_screen->num_candidates > cand_screen->num_per_window &&
      last - top < cand_screen->num_per_window) {
    u_int y;

    y = (xfont->height + LINE_SPACE) * (last - top + 1);

    ui_window_clear(&cand_screen->window, 0, y, win_width, win_height - y - 1);
  }

  /*
   * |8  cand7     |
   * |9  cand8     |
   * |10 cand9     |
   * | index/total | <-- draw this
   * +-------------+
   */
  if (cand_screen->num_candidates > cand_screen->num_per_window) {
    u_char navi[MAX_NUM_OF_DIGITS * 2 + 4];
    vt_char_t navi_str[MAX_NUM_OF_DIGITS * 2 + 4];
    u_int width;
    size_t len;
    int x;

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
    ui_window_clear(&cand_screen->window, 0,
                    (xfont->height + LINE_SPACE) * cand_screen->num_per_window + LINE_SPACE,
                    win_width, xfont->height);
#endif

    len = bl_snprintf(navi, MAX_NUM_OF_DIGITS * 2 + 2, "%d/%d", cand_screen->index + 1,
                      cand_screen->num_candidates);

    width = len * xfont->width;

    x = (win_width - width) / 2; /* centering */

    p = navi_str;
    for (i = 0; i < len; i++) {
      vt_char_init(p);
      vt_char_set(p++, navi[i], US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0, 0);
    }

    draw_str(cand_screen, navi_str, len, x, cand_screen->num_per_window, xfont->height,
             xfont->ascent, 0);

    vt_str_final(navi_str, len);
  }
}

static void draw_screen_horizontal(ui_im_candidate_screen_t *cand_screen, u_int top, u_int last,
                                   u_int draw_index, int do_resize) {
  ui_font_t *xfont;
  u_int win_width;
  u_int win_height;
  u_int i;
  int x = 0;
  u_int num_digits;
  u_char digit[MAX_NUM_OF_DIGITS + 1];
  vt_char_t digit_str[MAX_NUM_OF_DIGITS + 1];
  vt_char_t *p;

  /*
   * resize window
   */

  /*
   * +-------------------------------------+
   * |1:cand0 2:cand1 3:cand4 ... 10:cand9 |
   * +-------------------------------------+
   */

  xfont = ui_get_usascii_font(cand_screen->font_man);

  if (do_resize) {
    /* width of window */
    win_width = 0;
    for (i = top; i <= last; i++) {
      if (cand_screen->candidates[i].info) {
        if (((cand_screen->candidates[i].info >> 8) & 0xff) != 0) {
          num_digits = 2;
        } else {
          num_digits = 1;
        }
      } else {
        NUM_OF_DIGITS(num_digits, i);
      }

      win_width += xfont->width * (num_digits + 2);
    }
    win_width += total_candidate_width(cand_screen->font_man, cand_screen->candidates, top, last);
    /* height of window */
    win_height = xfont->height + LINE_SPACE;

    resize(cand_screen, win_width, win_height);
  } else {
    win_width = cand_screen->window.width;
    win_height = cand_screen->window.height;
  }

  for (i = top; i <= last; i++) {
    int j;

    /*
     * digits
     * +---------------
     * |1:cand0 2:cand1
     *  ^^
     */
    NUM_OF_DIGITS(num_digits, i);
    if (cand_screen->candidates[i].info) {
      char byte2;

      if ((byte2 = (cand_screen->candidates[i].info >> 8) & 0xff) != 0) {
        num_digits = 2;
        bl_snprintf(digit, MAX_NUM_OF_DIGITS + 1, "%c%c.", cand_screen->candidates[i].info, byte2);
      } else {
        num_digits = 1;
        bl_snprintf(digit, MAX_NUM_OF_DIGITS + 1, "%c.", cand_screen->candidates[i].info);
      }
    } else {
      bl_snprintf(digit, MAX_NUM_OF_DIGITS + 1, "%i.", i - top + 1);
    }

    p = digit_str;
    for (j = 0; j < num_digits + 1; j++) {
      vt_char_init(p);
      vt_char_set(p++, digit[j], US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0, 0);
    }

    if (draw_index != INVALID_INDEX) {
      if (i < draw_index) {
        x += (xfont->width * (num_digits + 2) +
              candidate_width(cand_screen->font_man, &cand_screen->candidates[i]));

        continue;
      } else /* if( i == draw_index) */
      {
        last = draw_index;
      }
    }

    draw_str(cand_screen, digit_str, num_digits + 1, x, 0, xfont->height, xfont->ascent, 0);
    x += xfont->width * (num_digits + 1);

    vt_str_final(digit_str, num_digits + 1);

    /*
     * candidate
     * +---------------
     * |1:cand0 2:cand2
     *    ^^^^^
     */
    draw_str(cand_screen, cand_screen->candidates[i].chars, cand_screen->candidates[i].filled_len,
             x, 0, xfont->height, xfont->ascent, 0);
    x += candidate_width(cand_screen->font_man, &cand_screen->candidates[i]);

    /*
     * +---------------
     * |1:cand0 2:cand2
     *         ^
     */
    ui_window_clear(&cand_screen->window, x, LINE_SPACE / 2, xfont->width, win_height - LINE_SPACE);
    x += xfont->width;
  }
}

static void draw_screen(ui_im_candidate_screen_t *cand_screen, u_int old_index, int do_resize) {
  u_int top;
  u_int last;

  VISIBLE_INDEX(cand_screen->num_candidates, cand_screen->num_per_window, cand_screen->index,
                top, last);

  if (old_index != cand_screen->index && old_index != INVALID_INDEX) {
    u_int old_top;
    u_int old_last;

    VISIBLE_INDEX(cand_screen->num_candidates, cand_screen->num_per_window, old_index, old_top,
                  old_last);

    if (old_top == top && old_last == last) {
      if (cand_screen->is_vertical_direction) {
        draw_screen_vertical(cand_screen, top, last, old_index, 0);
        draw_screen_vertical(cand_screen, top, last, cand_screen->index, 0);
      } else {
        draw_screen_horizontal(cand_screen, top, last, old_index, 0);
        draw_screen_horizontal(cand_screen, top, last, cand_screen->index, 0);
      }

      return;
    }
  }

  if (cand_screen->is_vertical_direction) {
    draw_screen_vertical(cand_screen, top, last, INVALID_INDEX, do_resize);
  } else {
    draw_screen_horizontal(cand_screen, top, last, INVALID_INDEX, do_resize);
  }
}

static void adjust_window_x_position(ui_im_candidate_screen_t *cand_screen, int *x) {
  u_int top;
  u_int last;
  u_int num_digits;

  if (cand_screen->is_vertical_term) {
    return;
  }

  VISIBLE_INDEX(cand_screen->num_candidates, cand_screen->num_per_window, cand_screen->index,
                top, last);

  if (cand_screen->is_vertical_direction) {
    if (cand_screen->num_candidates > cand_screen->num_per_window) {
      NUM_OF_DIGITS(num_digits, cand_screen->num_per_window);
    } else {
      NUM_OF_DIGITS(num_digits, last);
    }
  } else {
    num_digits = 1;
  }

  if (num_digits) {
    *x -= (ui_get_usascii_font(cand_screen->font_man)->width * (num_digits + 1) + MARGIN);
    if (*x < 0) {
      *x = 0;
    }
  }
}

/*
 * methods of ui_im_candidate_screen_t
 */

static void delete(ui_im_candidate_screen_t *cand_screen) {
  free_candidates(cand_screen->candidates, cand_screen->num_candidates);

  ui_display_remove_root(cand_screen->window.disp, &cand_screen->window);

#ifdef USE_REAL_VERTICAL_FONT
  if (cand_screen->is_vertical_term) {
    ui_font_manager_delete(cand_screen->font_man);
  }
#endif

  free(cand_screen);
}

static void show(ui_im_candidate_screen_t *cand_screen) {
  ui_window_map(&cand_screen->window);
}

static void hide(ui_im_candidate_screen_t *cand_screen) {
  ui_window_unmap(&cand_screen->window);
}

static int set_spot(ui_im_candidate_screen_t *cand_screen, int x, int y) {
  adjust_window_x_position(cand_screen, &x);
  cand_screen->x = x;
  cand_screen->y = y;

  adjust_window_position_by_size(cand_screen, &x, &y);

  if (cand_screen->window.x != x || cand_screen->window.y != y) {
    ui_window_move(&cand_screen->window, x, y);

    return 1;
  } else {
    return 0;
  }
}

static int init_candidates(ui_im_candidate_screen_t *cand_screen, u_int num_candidates,
                           u_int num_per_window) {
  if (cand_screen->candidates) {
    free_candidates(cand_screen->candidates, cand_screen->num_candidates);

    cand_screen->candidates = NULL;
  }

  cand_screen->num_candidates = num_candidates;
  cand_screen->num_per_window = num_per_window;

  /* allocate candidates(ui_im_candidate_t) array */
  if ((cand_screen->candidates =
           calloc(sizeof(ui_im_candidate_t), cand_screen->num_candidates)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " calloc failed.\n");
#endif
    cand_screen->num_candidates = 0;
    cand_screen->num_per_window = 0;
    return 0;
  }

  cand_screen->index = 0;
  cand_screen->need_redraw = 1;

  return 1;
}

static int set_candidate(ui_im_candidate_screen_t *cand_screen, ef_parser_t *parser, u_char *str,
                         u_int index /* 16bit: info, 16bit: index */
                         ) {
  int count = 0;
  ef_char_t ch;
  vt_char_t *p;

  u_short info;

  info = index >> 16;
  index &= 0xFF;

  if (index >= cand_screen->num_candidates) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG
                   " index of candidates is too large number "
                   "[num_candidates: %d, index: %d]\n",
                   cand_screen->num_candidates, index);
#endif
    return 0;
  }

  cand_screen->candidates[index].info = info;

  /*
   * count number of characters to allocate candidates[index].chars
   */

  (*parser->init)(parser);
  (*parser->set_str)(parser, str, strlen(str));

  while ((*parser->next_char)(parser, &ch)) {
    count++;
  }

  if (cand_screen->candidates[index].chars) {
    vt_str_delete(cand_screen->candidates[index].chars,
                  cand_screen->candidates[index].num_chars);
  }

  if (!(cand_screen->candidates[index].chars = vt_str_new(count))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_str_new() failed.\n");
#endif

    cand_screen->candidates[index].num_chars = 0;
    cand_screen->candidates[index].filled_len = 0;

    return 0;
  }

  cand_screen->candidates[index].num_chars = count;

  /*
   * im encoding -> term encoding
   */

  (*parser->init)(parser);
  (*parser->set_str)(parser, str, strlen(str));

  p = cand_screen->candidates[index].chars;

  vt_str_init(p, cand_screen->candidates[index].num_chars);

  while ((*parser->next_char)(parser, &ch)) {
    int is_fullwidth = 0;
    int is_comb = 0;

    if (vt_convert_to_internal_ch(cand_screen->vtparser, &ch) <= 0) {
      continue;
    }

    if (ch.property & EF_FULLWIDTH) {
      is_fullwidth = 1;
    } else if (ch.property & EF_AWIDTH) {
      /* TODO: check col_size_of_width_a */
      is_fullwidth = 1;
    }

    if (ch.property & EF_COMBINING) {
      is_comb = 1;

      if (vt_char_combine(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_FG_COLOR,
                          VT_BG_COLOR, 0, 0, 0, 0, 0, 0)) {
        continue;
      }

      /*
       * if combining failed , char is normally appended.
       */
    }

    vt_char_set(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_FG_COLOR, VT_BG_COLOR, 0,
                0, 0, 0, 0, 0);

    p++;
    cand_screen->candidates[index].filled_len++;
  }

  cand_screen->need_redraw = 1;

  return 1;
}

static int select_candidate(ui_im_candidate_screen_t *cand_screen, u_int index) {
  ui_im_candidate_t *cand;
  u_int i;
  u_int old_index;

  if (index >= cand_screen->num_candidates) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Selected index [%d] is larger than number of candidates [%d].\n",
                    index, cand_screen->num_candidates);
#endif

    return 0;
  }

  cand = &cand_screen->candidates[cand_screen->index];

  if (cand->chars) {
    for (i = 0; i < cand->filled_len; i++) {
      vt_char_set_fg_color(&cand->chars[i], VT_FG_COLOR);
      vt_char_set_bg_color(&cand->chars[i], VT_BG_COLOR);
    }
  }

  cand = &cand_screen->candidates[index];

  if (cand->chars) {
    for (i = 0; i < cand->filled_len; i++) {
      vt_char_set_fg_color(&cand->chars[i], VT_BG_COLOR);
      vt_char_set_bg_color(&cand->chars[i], VT_FG_COLOR);
    }
  }

  if (cand_screen->need_redraw) {
    old_index = INVALID_INDEX;
    cand_screen->need_redraw = 0;
  } else {
    old_index = cand_screen->index;
  }
  cand_screen->index = index;

  draw_screen(cand_screen, old_index, 1);

  return 1;
}

/*
 * callbacks of ui_window events
 */

static void window_realized(ui_window_t *win) {
  ui_im_candidate_screen_t *cand_screen;

  cand_screen = (ui_im_candidate_screen_t *)win;

  ui_window_set_type_engine(&cand_screen->window, ui_get_type_engine(cand_screen->font_man));

  ui_window_set_fg_color(win, ui_get_xcolor(cand_screen->color_man, VT_FG_COLOR));
  ui_window_set_bg_color(win, ui_get_xcolor(cand_screen->color_man, VT_BG_COLOR));

  ui_window_set_override_redirect(&cand_screen->window, 1);
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  ui_im_candidate_screen_t *cand_screen;

  cand_screen = (ui_im_candidate_screen_t *)win;

  if (cand_screen->num_candidates > 0) {
    draw_screen(cand_screen, INVALID_INDEX, 0);
  }

  cand_screen->need_redraw = 0;

  /* draw border (margin area has been already cleared in ui_window.c) */
  ui_window_draw_rect_frame(win, -MARGIN, -MARGIN, win->width + MARGIN - 1,
                            win->height + MARGIN - 1);
}

static void button_pressed(ui_window_t *win, XButtonEvent *event, int click_num) {
  ui_im_candidate_screen_t *cand_screen;
  u_int index;
  u_int top;
  u_int last;

  cand_screen = (ui_im_candidate_screen_t *)win;

  if (event->button != 1) {
    return;
  }

  if (!cand_screen->listener.selected) {
    return;
  }

  VISIBLE_INDEX(cand_screen->num_candidates, cand_screen->num_per_window, cand_screen->index,
                top, last);

  index = event->y / (ui_get_usascii_font(cand_screen->font_man)->height + LINE_SPACE);

  index += top;

  if (!select_candidate(cand_screen, index)) {
    return;
  }

  (*cand_screen->listener.selected)(cand_screen->listener.self, index);
}

/* --- global functions --- */

ui_im_candidate_screen_t *ui_im_candidate_screen_new(ui_display_t *disp,
                                                     ui_font_manager_t *font_man,
                                                     ui_color_manager_t *color_man,
                                                     void *vtparser, int is_vertical_term,
                                                     int is_vertical_direction,
                                                     u_int line_height_of_screen, int x, int y) {
  ui_im_candidate_screen_t *cand_screen;

  if ((cand_screen = calloc(1, sizeof(ui_im_candidate_screen_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

#ifdef USE_REAL_VERTICAL_FONT
  if (is_vertical_term) {
    cand_screen->font_man = ui_font_manager_new(disp->display,
                                                ui_get_type_engine(font_man),
                                                ui_get_font_present(font_man) & ~FONT_VERTICAL,
                                                ui_get_font_size(font_man),
                                                ui_get_current_usascii_font_cs(font_man),
                                                font_man->step_in_changing_font_size,
                                                ui_get_letter_space(font_man),
                                                ui_is_using_bold_font(font_man),
                                                ui_is_using_italic_font(font_man));
  } else
#endif
  {
    cand_screen->font_man = font_man;
  }

  cand_screen->color_man = color_man;
  cand_screen->vtparser = vtparser;

  cand_screen->x = x;
  cand_screen->y = y;
  cand_screen->line_height = line_height_of_screen;

  cand_screen->is_vertical_term = is_vertical_term;
  cand_screen->is_vertical_direction = is_vertical_direction;

  if (!ui_window_init(&cand_screen->window, MARGIN * 2, MARGIN * 2, MARGIN * 2, MARGIN * 2, 0, 0,
                      MARGIN, MARGIN, /* ceate_gc */ 1, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_window_init failed.\n");
#endif

    goto error;
  }

  ui_window_add_event_mask(&cand_screen->window, ButtonPressMask | ButtonReleaseMask);

  /*
   * +------------+
   * | ui_window.c | --- window events ---> +-------------------------+
   * +------------+                        | ui_im_candidate_screen.c |
   * +------------+ ----- methods  ------> +-------------------------+
   * | im plugin  | <---- callbacks ------
   * +------------+
   */

  /* callbacks for window events */
  cand_screen->window.window_realized = window_realized;
#if 0
  cand_screen->window.window_finalized = window_finalized;
#endif
  cand_screen->window.window_exposed = window_exposed;
#if 0
  cand_screen->window.button_released = button_released;
#endif
  cand_screen->window.button_pressed = button_pressed;
#if 0
  cand_screen->window.button_press_continued = button_press_continued;
  cand_screen->window.window_deleted = window_deleted;
  cand_screen->window.mapping_notify = mapping_notify;
#endif

  /* methods of ui_im_candidate_screen_t */
  cand_screen->delete = delete;
  cand_screen->show = show;
  cand_screen->hide = hide;
  cand_screen->set_spot = set_spot;
  cand_screen->init = init_candidates;
  cand_screen->set = set_candidate;
  cand_screen->select = select_candidate;

  if (!ui_display_show_root(disp, &cand_screen->window, x, y, XValue | YValue,
                            "mlterm-candidate-window", 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_display_show_root() failed.\n");
#endif

    goto error;
  }

  return cand_screen;

error:
  free(cand_screen);

  return NULL;
}

#else /* ! USE_IM_PLUGIN */

ui_im_candidate_screen_t *ui_im_candidate_screen_new(
    ui_display_t *disp, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
    void *vtparser, int is_vertical_term, int is_vertical_direction,
    u_int line_height_of_screen, int x, int y) {
  return NULL;
}

#endif
