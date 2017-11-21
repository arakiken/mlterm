/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_window.h"

#include <string.h>
#include <stdio.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_unistd.h> /* bl_usleep */
#include <mef/ef_codepoint_parser.h>
#include <mef/ef_utf16_parser.h>

#include <vt_char.h>
#include <vt_char_encoding.h>

#include "ui_display.h"
#include "ui_font.h"

#define MAX_CLICK 3 /* max is triple click */

/* win->width is not multiples of (win)->width_inc in framebuffer. */
#define RIGHT_MARGIN(win) \
  ((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define BOTTOM_MARGIN(win) \
  ((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)

#ifdef USE_GRF
static ui_color_t black = {TP_COLOR, 0, 0, 0, 0};
#endif

#define ParentRelative (1L)
#define DummyPixmap (2L)

#define COL_WIDTH (win->disp->display->col_width)
#define LINE_HEIGHT (win->disp->display->line_height)

/* XXX Check if win is input method window or not. */
#define IS_IM_WINDOW(win) ((win)->disp->num_roots >= 2 && (win) == (win)->disp->roots[1])

/* --- static variables --- */

static int click_interval = 250; /* millisecond, same as xterm. */

static ef_parser_t *cp_parser;
static ef_parser_t *utf16_parser;

/* --- static functions --- */

static int scroll_region(ui_window_t *win, int src_x, int src_y, u_int width, u_int height,
                         int dst_x, int dst_y) {
  int top;
  int top_c;
  int bottom;
  int bottom_c;
  int left;
  int left_c;
  int right;
  int right_c;

  if (!win->is_mapped || !ui_window_is_scrollable(win)) {
    return 0;
  }

  if (src_y < dst_y) {
    top = src_y;
    bottom = dst_y + height;
  } else {
    top = dst_y;
    bottom = src_y + height;
  }

  top_c = (top + win->y) / LINE_HEIGHT;
  bottom_c = (bottom + win->y) / LINE_HEIGHT;

  if (src_x < dst_x) {
    left = src_x;
    right = dst_x + width;
  } else {
    left = dst_x;
    right = src_x + width;
  }

  left_c = (left + win->x) / COL_WIDTH;
  right_c = (right + win->x) / COL_WIDTH;

  fprintf(win->disp->display->fp, "\x1b[%d;%dr", top_c + 1, bottom_c);
  fprintf(win->disp->display->fp, "\x1b[?69h\x1b[%d;%ds", left_c + 1, right_c);

/* XXX for mlterm-3.7.1 or before */
#if 1
  fprintf(win->disp->display->fp, "\x1b[%d;%dH", top_c + 1, left_c + 1);
#endif

  if (src_y < dst_y) {
    fprintf(win->disp->display->fp, "\x1b[%dT", (dst_y - src_y) / LINE_HEIGHT);
  } else if (src_y > dst_y) {
    fprintf(win->disp->display->fp, "\x1b[%dS", (src_y - dst_y) / LINE_HEIGHT);
  }

  if (src_x < dst_x) {
    int len = (dst_x - src_x) / COL_WIDTH;
    for (; top_c < bottom_c; top_c++) {
      fprintf(win->disp->display->fp, "\x1b[%d;%dH\x1b[%d@", top_c + 1, left_c + 1, len);
    }
  } else if (src_x > dst_x) {
    int len = (src_x - dst_x) / COL_WIDTH;
    for (; top_c < bottom_c; top_c++) {
      fprintf(win->disp->display->fp, "\x1b[%d;%dH\x1b[%dP", top_c + 1, left_c + 1, len);
    }
  }

  fprintf(win->disp->display->fp, "\x1b[r\x1b[?69l");

  return 1;
}

static void set_attr(FILE *fp, vt_font_t font, u_int fg_pixel, u_int bg_pixel,
                     int underline_style, int size_attr) {
  static int size_attr_set;

  if (fg_pixel < 0x8) {
    fprintf(fp, "\x1b[%dm", fg_pixel + 30);
  } else if (fg_pixel < 0x10) {
    fprintf(fp, "\x1b[%dm", (fg_pixel & ~VT_BOLD_COLOR_MASK) + 90);
  } else {
    fprintf(fp, "\x1b[38;5;%dm", fg_pixel);
  }

  if (bg_pixel < 0x8) {
    fprintf(fp, "\x1b[%dm", bg_pixel + 40);
  } else if (bg_pixel < 0x10) {
    fprintf(fp, "\x1b[%dm", (bg_pixel & ~VT_BOLD_COLOR_MASK) + 100);
  } else {
    fprintf(fp, "\x1b[48;5;%dm", bg_pixel);
  }

  switch (underline_style) {
    case UNDERLINE_NORMAL:
      fwrite("\x1b[4m", 1, 4, fp);
      break;
    case UNDERLINE_DOUBLE:
      fwrite("\x1b[21m", 1, 5, fp);
      break;
  }

  if (font & FONT_BOLD) {
    fwrite("\x1b[1m", 1, 4, fp);
  }

  if (font & FONT_ITALIC) {
    fwrite("\x1b[3m", 1, 4, fp);
  }

  if (size_attr == 0) {
    if (size_attr_set) {
      fwrite("\x1b#5", 1, 3, fp);
    }
  } else {
    size_attr_set = 1;

    if (size_attr == DOUBLE_WIDTH) {
      fwrite("\x1b#6", 1, 3, fp);
    } else if (size_attr == DOUBLE_HEIGHT_TOP) {
      fwrite("\x1b#3", 1, 3, fp);
    } else /* if (size_attr == DOUBLE_HEIGHT_BOTTOM) */ {
      fwrite("\x1b#4", 1, 3, fp);
    }
  }
}

static void draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                        ui_color_t *bg_color,      /* must be NULL if wall_picture_bg is 1 */
                        int x, int y, u_char *str, /* 'len * ch_len' bytes */
                        u_int len, u_int ch_len, int underline_style) {
  u_char *str2;

  if (!win->is_mapped) {
    return;
  }

  if ((ch_len != 1 || str[0] >= 0x80) && (str2 = alloca(len * UTF_MAX_SIZE))) {
    ef_parser_t *parser;
    ef_charset_t cs;

    if ((cs = FONT_CS(font->id)) == ISO10646_UCS4_1) {
      if (!utf16_parser) {
        utf16_parser = ef_utf16_parser_new();
      }

      parser = utf16_parser;
      (*parser->init)(parser);
      (*parser->set_str)(parser, str, len * ch_len);
    } else {
      if (!cp_parser) {
        cp_parser = ef_codepoint_parser_new();
      }

      parser = cp_parser;
      (*parser->init)(parser);
      /* 3rd argument of parser->set_str is len(16bit) + cs(16bit) */
      (*parser->set_str)(parser, str, (len * ch_len) | (cs << 16));
    }

    (*win->disp->display->conv->init)(win->disp->display->conv);
    if ((len = (*win->disp->display->conv->convert)(win->disp->display->conv, str2,
                                                    len *UTF_MAX_SIZE, parser)) > 0) {
      str = str2;
    }
  } else {
    /*
     * XXX
     * Padding white space by font->x_off is processed only if ch_len == 1 for now.
     * font->x_off is calculated by the right-justified in ui_font.c.
     */
    if (font->x_off >= font->width / 2) {
      if ((str2 = alloca(len * 2))) {
        u_int count;

        for (count = 0; count < len; count++) {
          str2[count * 2] = str[count];
          str2[count * 2 + 1] = ' ';
        }

        len *= 2;
        str = str2;
      }
    }

    len *= ch_len;
  }

  fprintf(win->disp->display->fp, "\x1b[%d;%dH",
          (win->y + win->vmargin + y) / LINE_HEIGHT + 1 +
            (font->size_attr == DOUBLE_HEIGHT_BOTTOM ? 1 : 0),
          (win->x + win->hmargin + x) / COL_WIDTH + 1);
  set_attr(win->disp->display->fp, font->id, fg_color->pixel, bg_color->pixel, underline_style,
           font->size_attr);
  fwrite(str, 1, len, win->disp->display->fp);
  fwrite("\x1b[m", 1, 3, win->disp->display->fp);
  fflush(win->disp->display->fp);
}

#ifdef USE_LIBSIXEL
static int copy_area(ui_window_t *win, Pixmap src, int src_x, /* can be minus */
                     int src_y,                               /* can be minus */
                     u_int width, u_int height, int dst_x,    /* can be minus */
                     int dst_y,                               /* can be minus */
                     int accept_margin /* x/y can be minus and over width/height */
                     ) {
  int hmargin;
  int vmargin;
  int right_margin;
  int bottom_margin;
  u_char *picture;

  if (!win->is_mapped) {
    return 0;
  }

#if 0
  if (accept_margin)
#endif
  {
    hmargin = win->hmargin;
    vmargin = win->vmargin;
    right_margin = bottom_margin = 0;
  }
#if 0
  else {
    hmargin = vmargin = 0;
    right_margin = RIGHT_MARGIN(win);
    bottom_margin = BOTTOM_MARGIN(win);
  }
#endif

  if (dst_x >= (int)win->width + hmargin || dst_y >= (int)win->height + vmargin) {
    return 0;
  }

  if (dst_x + width > win->width + hmargin - right_margin) {
    width = win->width + hmargin - right_margin - dst_x;
  }

  if (dst_y + height > win->height + vmargin - bottom_margin) {
    height = win->height + vmargin - bottom_margin - dst_y;
  }

  picture = src->image + src->width * 4 * (vmargin + src_y) + 4 * (hmargin + src_x);

  if (width < src->width) {
    u_char *clip;
    u_char *p;
    size_t line_len;

    line_len = width * 4;

    if ((p = clip = calloc(line_len, height))) {
      u_int count;

      for (count = 0; count < height; count++) {
        memcpy(p, picture, line_len);
        p += line_len;
        picture += (src->width * 4);
      }

      picture = clip;
    }
  }

  fprintf(win->disp->display->fp, "\x1b[%d;%dH", (win->y + win->vmargin + dst_y) / LINE_HEIGHT + 1,
          (win->x + win->hmargin + dst_x) / COL_WIDTH + 1);
  ui_display_output_picture(win->disp, picture, width, height);
  fflush(win->disp->display->fp);

  if (width < src->width) {
    free(picture);
  }

  return 1;
}
#else
#define copy_area(win, src, src_x, src_y, width, height, dst_x, dst_y, accept_margin) (0)
#endif

static void clear_margin_area(ui_window_t *win) {
  u_int right_margin;
  u_int bottom_margin;

  right_margin = RIGHT_MARGIN(win);
  bottom_margin = BOTTOM_MARGIN(win);

  if (win->hmargin | win->vmargin | right_margin | bottom_margin) {
    ui_window_clear(win, -(win->hmargin), -(win->vmargin), win->hmargin, ACTUAL_HEIGHT(win));
    ui_window_clear(win, 0, -(win->vmargin), win->width, win->vmargin);
    ui_window_clear(win, win->width - right_margin, -(win->vmargin), win->hmargin + right_margin,
                    ACTUAL_HEIGHT(win));
    ui_window_clear(win, 0, win->height - bottom_margin, win->width, win->vmargin + bottom_margin);
  }

  /* XXX */
  if (win->num_children == 2 &&
      ACTUAL_HEIGHT(win->children[0]) == ACTUAL_HEIGHT(win->children[1])) {
    if (win->children[0]->x + ACTUAL_WIDTH(win->children[0]) <= win->children[1]->x) {
      ui_window_clear(win, win->children[0]->x + ACTUAL_WIDTH(win->children[0]), 0,
                      win->children[1]->x - win->children[0]->x - ACTUAL_WIDTH(win->children[0]),
                      win->height);
    } else if (win->children[0]->x >= win->children[1]->x + ACTUAL_WIDTH(win->children[1])) {
      ui_window_clear(win, win->children[1]->x + ACTUAL_WIDTH(win->children[1]), 0,
                      win->children[0]->x - win->children[1]->x - ACTUAL_WIDTH(win->children[1]),
                      win->height);
    }
  }
}

static int fix_rl_boundary(ui_window_t *win, int boundary_start, int *boundary_end) {
  int margin;

  margin = RIGHT_MARGIN(win);

  if (boundary_start > win->width - margin) {
    return 0;
  }

  if (*boundary_end > win->width - margin) {
    *boundary_end = win->width - margin;
  }

  return 1;
}

static void reset_input_focus(ui_window_t *win) {
  u_int count;

  if (win->inputtable) {
    win->inputtable = -1;
  } else {
    win->inputtable = 0;
  }

  if (win->is_focused) {
    win->is_focused = 0;

    if (win->window_unfocused) {
      (*win->window_unfocused)(win);
    }
  }

  for (count = 0; count < win->num_children; count++) {
    reset_input_focus(win->children[count]);
  }
}

#if 0
static int check_child_window_area(ui_window_t *win) {
  if (win->num_children > 0) {
    u_int count;
    u_int sum;

    for (sum = 0, count = 1; count < win->num_children; count++) {
      sum += (ACTUAL_WIDTH(win->children[count]) * ACTUAL_HEIGHT(win->children[count]));
    }

    if (sum < win->disp->width * win->disp->height * 0.9) {
      return 0;
    }
  }

  return 1;
}
#endif

/* --- global functions --- */

int ui_window_init(ui_window_t *win, u_int width, u_int height, u_int min_width, u_int min_height,
                   u_int width_inc, u_int height_inc, u_int hmargin, u_int vmargin, int create_gc,
                   int inputtable) {
  memset(win, 0, sizeof(ui_window_t));

  /* If wall picture is set, scrollable will be 0. */
  win->is_scrollable = 1;

  win->is_focused = 1;
  win->inputtable = inputtable;
  win->is_mapped = 1;

  win->create_gc = create_gc;

  win->width = width;
  win->height = height;
  win->min_width = min_width;
  win->min_height = min_height;
  win->width_inc = width_inc;
  win->height_inc = height_inc;
  win->hmargin = 0 /* hmargin */;
  win->vmargin = 0 /* vmargin */;

  win->prev_clicked_button = -1;

  win->app_name = "mlterm"; /* Can be changed in ui_display_show_root(). */

  return 1;
}

void ui_window_final(ui_window_t *win) {
  u_int count;

  for (count = 0; count < win->num_children; count++) {
    ui_window_final(win->children[count]);
  }

  free(win->children);

  ui_display_clear_selection(win->disp, win);

  if (win->window_finalized) {
    (*win->window_finalized)(win);
  }
}

void ui_window_set_type_engine(ui_window_t *win, ui_type_engine_t type_engine) {}

void ui_window_add_event_mask(ui_window_t *win, long event_mask) {
  win->event_mask |= event_mask;
}

void ui_window_remove_event_mask(ui_window_t *win, long event_mask) {
  win->event_mask &= ~event_mask;
}

void ui_window_ungrab_pointer(ui_window_t *win) {}

int ui_window_set_wall_picture(ui_window_t *win, Pixmap pic, int do_expose) {
  u_int count;

  win->wall_picture = pic;
  win->is_scrollable = 0;

  if (do_expose) {
    clear_margin_area(win);

    if (win->window_exposed) {
      (*win->window_exposed)(win, 0, 0, win->width, win->height);
    }
#if 0
    else {
      ui_window_clear_all(win);
    }
#endif
  }

  for (count = 0; count < win->num_children; count++) {
    ui_window_set_wall_picture(win->children[count], ParentRelative, do_expose);
  }

  return 1;
}

int ui_window_unset_wall_picture(ui_window_t *win, int do_expose) {
  u_int count;

  win->wall_picture = None;
  win->is_scrollable = 1;

  if (do_expose) {
    clear_margin_area(win);

    if (win->window_exposed) {
      (*win->window_exposed)(win, 0, 0, win->width, win->height);
    }
#if 0
    else {
      ui_window_clear_all(win);
    }
#endif
  }

  for (count = 0; count < win->num_children; count++) {
    ui_window_unset_wall_picture(win->children[count], do_expose);
  }

  return 1;
}

int ui_window_set_transparent(
    ui_window_t *win, /* Transparency is applied to all children recursively */
    ui_picture_modifier_ptr_t pic_mod) {
  return 0;
}

int ui_window_unset_transparent(ui_window_t *win) { return 0; }

void ui_window_set_cursor(ui_window_t *win, u_int cursor_shape) {
  win->cursor_shape = cursor_shape;
}

int ui_window_set_fg_color(ui_window_t *win, ui_color_t *fg_color) {
  if (win->fg_color.pixel == fg_color->pixel) {
    return 0;
  }

  win->fg_color = *fg_color;

  return 1;
}

int ui_window_set_bg_color(ui_window_t *win, ui_color_t *bg_color) {
  if (win->bg_color.pixel == bg_color->pixel) {
    return 0;
  }

  win->bg_color = *bg_color;

  clear_margin_area(win);

  return 1;
}

int ui_window_add_child(ui_window_t *win, ui_window_t *child, int x, int y, int map) {
  void *p;

  if (win->parent) {
    /* Can't add a grand child window. */
    return 0;
  }

  if ((p = realloc(win->children, sizeof(*win->children) * (win->num_children + 1))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " realloc failed.\n");
#endif

    return 0;
  }

  win->children = p;

  child->parent = win;
  child->x = x + win->hmargin;
  child->y = y + win->vmargin;

  if ((child->is_mapped = map) && win->is_focused && child->inputtable) {
    child->is_focused = 1;
  } else {
    child->is_focused = 0;
    if (child->inputtable > 0) {
      child->inputtable = -1;
    }
  }

  win->children[win->num_children++] = child;

  return 1;
}

int ui_window_remove_child(ui_window_t *win, ui_window_t *child) {
  u_int count;

  for (count = 0; count < win->num_children; count++) {
    if (win->children[count] == child) {
      child->parent = NULL;

      win->children[count] = win->children[--win->num_children];

      return 1;
    }
  }

  return 0;
}

ui_window_t *ui_get_root_window(ui_window_t *win) {
  while (win->parent != NULL) {
    win = win->parent;
  }

  return win;
}

GC ui_window_get_fg_gc(ui_window_t *win) { return None; }

GC ui_window_get_bg_gc(ui_window_t *win) { return None; }

int ui_window_show(ui_window_t *win,
                   int hint /* If win->parent(_window) is None,
                               specify XValue|YValue to localte window at win->x/win->y. */
                   ) {
  u_int count;

  if (win->my_window) {
    /* already shown */

    return 0;
  }

  if (win->parent) {
    win->disp = win->parent->disp;
    win->parent_window = win->parent->my_window;
    win->gc = win->parent->gc;
  }

  win->my_window = win; /* Note that ui_connect_dialog.c uses this. */

  if (win->parent && !win->parent->is_transparent && win->parent->wall_picture) {
    ui_window_set_wall_picture(win, ParentRelative, 0);
  }

  /*
   * This should be called after Window Manager settings, because
   * ui_set_{window|icon}_name() can be called in win->window_realized().
   */
  if (win->window_realized) {
    int is_mapped;

    /*
     * Don't show anything until ui_window_resize_with_margin() is called
     * at the end of this function.
     */
    is_mapped = win->is_mapped;
    win->is_mapped = 0; /* XXX ui_window_set_wall_picture() depends on this. */
    (*win->window_realized)(win);
    win->is_mapped = is_mapped;
  }

  /*
   * showing child windows.
   */

  for (count = 0; count < win->num_children; count++) {
    ui_window_show(win->children[count], 0);
  }

  if (!win->parent && win->x == 0 && win->y == 0) {
    ui_window_resize_with_margin(win, win->disp->width, win->disp->height, NOTIFY_TO_MYSELF);
  }

  return 1;
}

void ui_window_map(ui_window_t *win) {
  if (!win->is_mapped) {
    win->is_mapped = 1;

    (*win->window_exposed)(win, 0, 0, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win));
    clear_margin_area(win);
  }
}

void ui_window_unmap(ui_window_t *win) {
  win->is_mapped = 0;
}

int ui_window_resize(ui_window_t *win, u_int width, /* excluding margin */
                     u_int height,                  /* excluding margin */
                     ui_resize_flag_t flag          /* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
                     ) {
  if ((flag & NOTIFY_TO_PARENT) && !IS_IM_WINDOW(win)) {
    if (win->parent) {
      win = win->parent;
    }

    /*
     * XXX
     * If Font size, screen_{width|height}_ratio or vertical_mode is changed
     * and ui_window_resize( NOTIFY_TO_PARENT) is called, ignore this call and
     * resize windows with display size.
     */
    win->width = 0;
    return ui_window_resize_with_margin(win, win->disp->width, win->disp->height, NOTIFY_TO_MYSELF);
  }

  if (width + win->hmargin * 2 > win->disp->width) {
    width = win->disp->width - win->hmargin * 2;
  }

  if (height + win->vmargin * 2 > win->disp->height) {
    height = win->disp->height - win->vmargin * 2;
  }

  if (win->width == width && win->height == height) {
    return 0;
  }

  win->width = width;
  win->height = height;

  if (flag & NOTIFY_TO_MYSELF) {
    if (win->window_resized) {
      (*win->window_resized)(win);
    }

    /*
     * clear_margin_area() must be called after win->window_resized
     * because wall_picture can be resized to fit to the new window
     * size in win->window_resized.
     *
     * Don't clear_margin_area() if flag == 0 because ui_window_resize()
     * is called before ui_window_move() in ui_im_*_screen.c and could
     * cause segfault.
     */
    clear_margin_area(win);
  }

  return 1;
}

/*
 * !! Notice !!
 * This function is not recommended.
 * Use ui_window_resize if at all possible.
 */
int ui_window_resize_with_margin(ui_window_t *win, u_int width, u_int height,
                                 ui_resize_flag_t flag /* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
                                 ) {
  return ui_window_resize(win, width - win->hmargin * 2, height - win->vmargin * 2, flag);
}

void ui_window_set_maximize_flag(ui_window_t *win, ui_maximize_flag_t flag) {}

void ui_window_set_normal_hints(ui_window_t *win, u_int min_width, u_int min_height,
                                u_int width_inc, u_int height_inc) {}

void ui_window_set_override_redirect(ui_window_t *win, int flag) {}

int ui_window_set_borderless_flag(ui_window_t *win, int flag) { return 0; }

int ui_window_move(ui_window_t *win, int x, int y) {
  if (win->parent) {
    x += win->parent->hmargin;
    y += win->parent->vmargin;
  }

  if (win->x == x && win->y == y) {
    return 0;
  }

  win->x = x;
  win->y = y;

  if (/* ! check_child_window_area( ui_get_root_window( win)) || */
      win->x + ACTUAL_WIDTH(win) > win->disp->width ||
      win->y + ACTUAL_HEIGHT(win) > win->disp->height) {
    /*
     * XXX Hack
     * (Expect the caller to call ui_window_resize() immediately after this.)
     */
    return 1;
  }

  /*
   * XXX
   * Check if win is input method window or not, because ui_window_move()
   * can fall into the following infinite loop on framebuffer.
   * 1) ui_im_stat_screen::draw_screen() ->
   *    ui_window_move() ->
   *    ui_im_stat_screen::window_exposed() ->
   *    ui_im_stat_screen::draw_screen()
   * 2) ui_im_candidate_screen::draw_screen() ->
   *    ui_im_candidate_screen::resize() ->
   *    ui_window_move() ->
   *    ui_im_candidate_screen::window_exposed() ->
   *    ui_im_candidate_screen::draw_screen()
   */
  if (!IS_IM_WINDOW(win)) {
    clear_margin_area(win);

    if (win->window_exposed) {
      (*win->window_exposed)(win, 0, 0, win->width, win->height);
    }
#if 0
    else {
      ui_window_clear_all(win);
    }
#endif

    /* XXX */
    if (win->parent) {
      clear_margin_area(win->parent);
    }
  }

  return 1;
}

void ui_window_clear(ui_window_t *win, int x, int y, u_int width, u_int height) {
  if (!win->wall_picture) {
    ui_window_fill_with(win, &win->bg_color, x, y, width, height);
  } else {
    Pixmap pic;
    int src_x;
    int src_y;

    if (win->wall_picture == ParentRelative) {
      src_x = x + win->x;
      src_y = y + win->y;
      pic = win->parent->wall_picture;
    } else {
      pic = win->wall_picture;
      src_x = x;
      src_y = y;
    }

    copy_area(win, pic, src_x, src_y, width, height, x, y, 1);
  }
}

void ui_window_clear_all(ui_window_t *win) {
  ui_window_clear(win, 0, 0, win->width, win->height);
}

void ui_window_fill(ui_window_t *win, int x, int y, u_int width, u_int height) {
  ui_window_fill_with(win, &win->fg_color, x, y, width, height);
}

void ui_window_fill_with(ui_window_t *win, ui_color_t *color, int x, int y, u_int width,
                         u_int height) {
  u_int h;
  int fill_to_end;

  if (!win->is_mapped) {
    return;
  }

  if (height < LINE_HEIGHT || width < COL_WIDTH) {
    return;
  }

  if (!IS_IM_WINDOW(win) && (!win->parent || win->x + ACTUAL_WIDTH(win) >= win->parent->width) &&
      x + width >= win->width) {
    fill_to_end = 1;
  } else {
    fill_to_end = 0;
  }

  x += (win->x + win->hmargin);
  y += (win->y + win->vmargin);

  if (color->pixel < 0x8) {
    fprintf(win->disp->display->fp, "\x1b[%dm", color->pixel + 40);
  } else if (color->pixel < 0x10) {
    fprintf(win->disp->display->fp, "\x1b[%dm", (color->pixel & ~VT_BOLD_COLOR_MASK) + 100);
  } else {
    fprintf(win->disp->display->fp, "\x1b[48;5;%dm", color->pixel);
  }

#if 1
  for (h = 0; h < height; h += LINE_HEIGHT) {
    fprintf(win->disp->display->fp, "\x1b[%d;%dH", (y + h) / LINE_HEIGHT + 1, x / COL_WIDTH + 1);

    if (fill_to_end) {
      fwrite("\x1b[K", 1, 3, win->disp->display->fp);
    } else {
      u_int w;

      for (w = 0; w < width; w += COL_WIDTH) {
        fwrite(" ", 1, 1, win->disp->display->fp);
      }
    }
  }
#else
  fprintf(win->disp->display->fp, "\x1b[%d;%d;%d;%d$z", y / LINE_HEIGHT + 1, x / COL_WIDTH + 1,
          (y + height) / LINE_HEIGHT, (x + width) / COL_WIDTH);
#endif

  fwrite("\x1b[m", 1, 3, win->disp->display->fp);
  fflush(win->disp->display->fp);
}

void ui_window_blank(ui_window_t *win) {
  ui_window_fill_with(win, &win->fg_color, 0, 0, win->width - RIGHT_MARGIN(win),
                      win->height - BOTTOM_MARGIN(win));
}

void ui_window_update(ui_window_t *win, int flag) {
  if (!win->is_mapped) {
    return;
  }

  if (win->update_window) {
    (*win->update_window)(win, flag);
  }
}

void ui_window_update_all(ui_window_t *win) {
  u_int count;

  if (!win->is_mapped) {
    return;
  }

  if (!win->parent) {
    ui_display_reset_cmap();
  }

  clear_margin_area(win);

  if (win->window_exposed) {
    (*win->window_exposed)(win, 0, 0, win->width, win->height);
  }

  for (count = 0; count < win->num_children; count++) {
    ui_window_update_all(win->children[count]);
  }
}

void ui_window_idling(ui_window_t *win) {
  u_int count;

  for (count = 0; count < win->num_children; count++) {
    ui_window_idling(win->children[count]);
  }

#ifdef __DEBUG
  if (win->button_is_pressing) {
    bl_debug_printf(BL_DEBUG_TAG " button is pressing...\n");
  }
#endif

  if (win->button_is_pressing && win->button_press_continued) {
    (*win->button_press_continued)(win, &win->prev_button_press_event);
  } else if (win->idling) {
    (*win->idling)(win);
  }
}

/*
 * Return value: 0 => different window.
 *               1 => finished processing.
 */
int ui_window_receive_event(ui_window_t *win, XEvent *event) {
#if 0
  u_int count;

  for (count = 0; count < win->num_children; count++) {
    if (ui_window_receive_event(win->children[count], event)) {
      return 1;
    }
  }
#endif

  if (event->type == KeyPress) {
    if (win->key_pressed) {
      (*win->key_pressed)(win, &event->xkey);
    }
  } else if (event->type == MotionNotify) {
    if (win->button_is_pressing) {
      if (win->button_motion) {
        event->xmotion.x -= win->hmargin;
        event->xmotion.y -= win->vmargin;

        (*win->button_motion)(win, &event->xmotion);
      }

      /* following button motion ... */

      win->prev_button_press_event.x = event->xmotion.x;
      win->prev_button_press_event.y = event->xmotion.y;
      win->prev_button_press_event.time = event->xmotion.time;
    } else if ((win->event_mask & PointerMotionMask) && win->pointer_motion) {
      event->xmotion.x -= win->hmargin;
      event->xmotion.y -= win->vmargin;

      (*win->pointer_motion)(win, &event->xmotion);
    }
  } else if (event->type == ButtonRelease) {
    if (win->button_released) {
      event->xbutton.x -= win->hmargin;
      event->xbutton.y -= win->vmargin;

      (*win->button_released)(win, &event->xbutton);
    }

    win->button_is_pressing = 0;
  } else if (event->type == ButtonPress) {
    if (win->button_pressed) {
      event->xbutton.x -= win->hmargin;
      event->xbutton.y -= win->vmargin;

      /* XXX If button is released outside screen, ButtonRelease event might not happen. */
      if (win->button_is_pressing) {
        if (win->button_released) {
          XButtonEvent ev = event->xbutton;
          ev.type = ButtonRelease;
          (*win->button_released)(win, &ev);
        }
        win->button_is_pressing = 0;
      }

      if (win->click_num == MAX_CLICK) {
        win->click_num = 0;
      }

      if (win->prev_clicked_time + click_interval >= event->xbutton.time &&
          event->xbutton.button == win->prev_clicked_button) {
        win->click_num++;
        win->prev_clicked_time = event->xbutton.time;
      } else {
        win->click_num = 1;
        win->prev_clicked_time = event->xbutton.time;
        win->prev_clicked_button = event->xbutton.button;
      }

      (*win->button_pressed)(win, &event->xbutton, win->click_num);
    }

    if (event->xbutton.button <= Button3) {
      /* button_is_pressing flag is on except wheel mouse (Button4/Button5). */
      win->button_is_pressing = 1;
      win->prev_button_press_event = event->xbutton;
    }

    if (!win->is_focused && win->inputtable && event->xbutton.button == Button1 &&
        !event->xbutton.state) {
      ui_window_set_input_focus(win);
    }
  }

  return 1;
}

size_t ui_window_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                         KeySym *keysym, XKeyEvent *event) {
  u_char ch;

  if (seq_len == 0) {
    return 0;
  }

  *parser = NULL;

  ch = event->ksym;

  if ((*keysym = event->ksym) >= 0x100) {
    switch (*keysym) {
      case XK_KP_Multiply:
        ch = '*';
        break;
      case XK_KP_Add:
        ch = '+';
        break;
      case XK_KP_Separator:
        ch = ',';
        break;
      case XK_KP_Subtract:
        ch = '-';
        break;
      case XK_KP_Divide:
        ch = '/';
        break;

      default:
        if (win->disp->display->lock_state & NLKED) {
          switch (*keysym) {
            case XK_KP_Insert:
              ch = '0';
              break;
            case XK_KP_End:
              ch = '1';
              break;
            case XK_KP_Down:
              ch = '2';
              break;
            case XK_KP_Next:
              ch = '3';
              break;
            case XK_KP_Left:
              ch = '4';
              break;
            case XK_KP_Begin:
              ch = '5';
              break;
            case XK_KP_Right:
              ch = '6';
              break;
            case XK_KP_Home:
              ch = '7';
              break;
            case XK_KP_Up:
              ch = '8';
              break;
            case XK_KP_Prior:
              ch = '9';
              break;
            case XK_KP_Delete:
              ch = '.';
              break;
            default:
              return 0;
          }

          *keysym = ch;
        } else {
          return 0;
        }
    }
  } else if (*keysym == XK_Tab && (event->state & ShiftMask)) {
    *keysym = XK_ISO_Left_Tab;

    return 0;
  }

  /*
   * Control + '@'(0x40) ... '_'(0x5f) -> 0x00 ... 0x1f
   *
   * Not "<= '_'" but "<= 'z'" because Control + 'a' is not
   * distinguished from Control + 'A'.
   */
  if ((event->state & ControlMask) && (ch == ' ' || ('@' <= ch && ch <= 'z'))) {
    seq[0] = (ch & 0x1f);
    if (seq[0] == XK_BackSpace || seq[0] == XK_Tab || seq[0] == XK_Return) {
      *keysym = seq[0];
      event->state &= ~ControlMask;
    }
  } else {
    seq[0] = ch;
  }

  return 1;
}

/*
 * Scroll functions.
 * The caller side should clear the scrolled area.
 */

int ui_window_scroll_upward(ui_window_t *win, u_int height) {
  return ui_window_scroll_upward_region(win, 0, win->height, height);
}

int ui_window_is_scrollable(ui_window_t *win) {
  /* XXX If input method module is activated, don't scroll window. */
  if (win->is_scrollable &&
      (win->disp->display->support_hmargin || win->disp->width == ACTUAL_WIDTH(win)) &&
      !IM_WINDOW_IS_ACTIVATED(win->disp)) {
    return 1;
  } else {
    return 0;
  }
}

int ui_window_scroll_upward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                   u_int height) {
  if (boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " boundary start %d end %d height %d in window((h) %d (w) %d)\n",
                   boundary_start, boundary_end, height, win->height, win->width);
#endif

    return 0;
  }

  return scroll_region(win, 0, boundary_start + height,                    /* src */
                       win->width, boundary_end - boundary_start - height, /* size */
                       0, boundary_start);                                 /* dst */
}

int ui_window_scroll_downward(ui_window_t *win, u_int height) {
  return ui_window_scroll_downward_region(win, 0, win->height, height);
}

int ui_window_scroll_downward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                     u_int height) {
  if (boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " boundary start %d end %d height %d\n", boundary_start,
                   boundary_end, height);
#endif

    return 0;
  }

  return scroll_region(win, 0, boundary_start, win->width, boundary_end - boundary_start - height,
                       0, boundary_start + height);
}

int ui_window_scroll_leftward(ui_window_t *win, u_int width) {
  return ui_window_scroll_leftward_region(win, 0, win->width, width);
}

int ui_window_scroll_leftward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                     u_int width) {
  if (boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width ||
      !fix_rl_boundary(win, boundary_start, &boundary_end)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " boundary start %d end %d width %d in window((h) %d (w) %d)\n",
                   boundary_start, boundary_end, width, win->height, win->width);
#endif

    return 0;
  }

  scroll_region(win, boundary_start + width, 0,                     /* src */
                boundary_end - boundary_start - width, win->height, /* size */
                boundary_start, 0);                                 /* dst */

  return 1;
}

int ui_window_scroll_rightward(ui_window_t *win, u_int width) {
  return ui_window_scroll_rightward_region(win, 0, win->width, width);
}

int ui_window_scroll_rightward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                      u_int width) {
  if (boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width ||
      !fix_rl_boundary(win, boundary_start, &boundary_end)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " boundary start %d end %d width %d\n", boundary_start,
                   boundary_end, width);
#endif

    return 0;
  }

  scroll_region(win, boundary_start, 0, boundary_end - boundary_start - width, win->height,
                boundary_start + width, 0);

  return 1;
}

int ui_window_copy_area(ui_window_t *win, Pixmap src, PixmapMask mask, int src_x, /* >= 0 */
                        int src_y,                                                /* >= 0 */
                        u_int width, u_int height, int dst_x,                     /* >= 0 */
                        int dst_y                                                 /* >= 0 */
                        ) {
  return copy_area(win, src, src_x, src_y, width, height, dst_x, dst_y, 0);
}

void ui_window_set_clip(ui_window_t *win, int x, int y, u_int width, u_int height) {}

void ui_window_unset_clip(ui_window_t *win) {}

void ui_window_console_draw_decsp_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                         ui_color_t *bg_color, int x, int y, u_char *str, u_int len,
                                         int underline_style) {
  if (!win->is_mapped) {
    return;
  }

  fprintf(win->disp->display->fp, "\x1b[%d;%dH",
          (win->y + win->vmargin + y) / LINE_HEIGHT + 1 +
            (font->size_attr == DOUBLE_HEIGHT_BOTTOM ? 1 : 0),
          (win->x + win->hmargin + x) / COL_WIDTH + 1);
  set_attr(win->disp->display->fp, font->id, fg_color->pixel, bg_color->pixel, underline_style,
           font->size_attr);
  fwrite("\x1b(0", 1, 3, win->disp->display->fp);
  fwrite(str, 1, len, win->disp->display->fp);
  fwrite("\x1b(B\x1b[m", 1, 6, win->disp->display->fp);
  fflush(win->disp->display->fp);
}

void ui_window_console_draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                   ui_color_t *bg_color, int x, int y, u_char *str, u_int len,
                                   int underline_style) {
  draw_string(win, font, fg_color, bg_color, x, y, str, len, 1, underline_style);
}

void ui_window_console_draw_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                     ui_color_t *bg_color, int x, int y, XChar2b *str, u_int len,
                                     int underline_style) {
  draw_string(win, font, fg_color, bg_color, x, y, str, len, 2, underline_style);
}

void ui_window_draw_rect_frame(ui_window_t *win, int x1, int y1, int x2, int y2) {
  ui_window_fill_with(win, &win->fg_color, x1, y1, x2 - x1 + 1, 1);
  ui_window_fill_with(win, &win->fg_color, x1, y1, 1, y2 - y1 + 1);
  ui_window_fill_with(win, &win->fg_color, x1, y2, x2 - x1 + 1, 1);
  ui_window_fill_with(win, &win->fg_color, x2, y1, 1, y2 - y1 + 1);
}

void ui_set_use_clipboard_selection(int use_it) {}

int ui_is_using_clipboard_selection(void) { return 0; }

int ui_window_set_selection_owner(ui_window_t *win, Time time) {
  if (ui_window_is_selection_owner(win)) {
    /* Already owner */

    return 1;
  }

  return ui_display_own_selection(win->disp, win);
}

int ui_window_xct_selection_request(ui_window_t *win, Time time) {
  u_int count;
  u_int num_displays;
  ui_display_t **displays = ui_get_opened_displays(&num_displays);

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->selection_owner) {
      ui_window_t *owner = displays[count]->selection_owner;
      if (owner->xct_selection_requested) {
        XSelectionRequestEvent ev;
        ev.type = 0;
        ev.target = win;
        (*owner->xct_selection_requested)(owner, &ev, 0);
      }

      break;
    }
  }

  return 1;
}

int ui_window_utf_selection_request(ui_window_t *win, Time time) {
  u_int count;
  u_int num_displays;
  ui_display_t **displays = ui_get_opened_displays(&num_displays);

  for (count = 0; count < num_displays; count++) {
    if (displays[count]->selection_owner) {
      ui_window_t *owner = displays[count]->selection_owner;
      if (owner->utf_selection_requested) {
        XSelectionRequestEvent ev;
        ev.type = 1;
        ev.target = win;
        (*owner->utf_selection_requested)(owner, &ev, 0);
      }

      break;
    }
  }

  return 1;
}

void ui_window_send_picture_selection(ui_window_t *win, Pixmap pixmap, u_int width, u_int height) {}

void ui_window_send_text_selection(ui_window_t *win, XSelectionRequestEvent *req_ev,
                                   u_char *sel_data, size_t sel_len, Atom sel_type) {
  if (req_ev) {
    if (req_ev->type == 1) {
      if (req_ev->target->utf_selection_notified) {
        (*req_ev->target->utf_selection_notified)(req_ev->target, sel_data, sel_len);
      }
    } else {
      if (req_ev->target->xct_selection_notified) {
        (*req_ev->target->xct_selection_notified)(req_ev->target, sel_data, sel_len);
      }
    }
  }
}

void ui_set_window_name(ui_window_t *win, u_char *name) {
  vt_char_encoding_t encoding;
  ef_parser_t *parser;

  if (name == NULL) {
    name = win->app_name;
  }

  /* See parse_title() in vt_parser.c */
  if ((encoding = vt_get_char_encoding("auto")) == VT_UTF8) {
    fwrite("\x1b[>2t\x1b]2;", 1, 9, win->disp->display->fp);
    fwrite(name, 1, strlen(name), win->disp->display->fp);
    fwrite("\x07\x1b[>2T", 1, 6, win->disp->display->fp);
  } else if ((parser = vt_char_encoding_parser_new(encoding))) {
    u_char buf[64];
    size_t len;

    (*win->disp->display->conv->init)(win->disp->display->conv);
    (*parser->init)(parser);
    (*parser->set_str)(parser, name, strlen(name));

    fwrite("\x1b]2;", 1, 4, win->disp->display->fp);
    while ((len = (*win->disp->display->conv->convert)(win->disp->display->conv, buf,
                                                       sizeof(buf), parser)) > 0) {
      fwrite(buf, 1, len, win->disp->display->fp);
    }
    fwrite("\x07", 1, 1, win->disp->display->fp);

    (*parser->delete)(parser);
  }
}

void ui_set_icon_name(ui_window_t *win, u_char *name) {
  vt_char_encoding_t encoding;
  ef_parser_t *parser;

  if (name == NULL) {
    name = win->app_name;
  }

  if ((encoding = vt_get_char_encoding("auto")) == VT_UTF8) {
    fwrite("\x1b[>2t\x1b]1;", 1, 9, win->disp->display->fp);
    fwrite(name, 1, strlen(name), win->disp->display->fp);
    fwrite("\x07\x1b[>2T", 1, 6, win->disp->display->fp);
  } else if ((parser = vt_char_encoding_parser_new(encoding))) {
    u_char buf[64];
    size_t len;

    (*win->disp->display->conv->init)(win->disp->display->conv);
    (*parser->init)(parser);
    (*parser->set_str)(parser, name, strlen(name));

    fwrite("\x1b]1;", 1, 4, win->disp->display->fp);
    while ((len = (*win->disp->display->conv->convert)(win->disp->display->conv, buf,
                                                       sizeof(buf), parser)) > 0) {
      fwrite(buf, 1, len, win->disp->display->fp);
    }
    fwrite("\x07", 1, 1, win->disp->display->fp);

    (*parser->delete)(parser);
  }
}

void ui_window_set_icon(ui_window_t *win, ui_icon_picture_ptr_t icon) {}

void ui_window_remove_icon(ui_window_t *win) {}

void ui_window_reset_group(ui_window_t *win) {}

void ui_set_click_interval(int interval) {
  click_interval = interval;
}

int ui_get_click_interval(void) {
  return click_interval;
}

u_int ui_window_get_mod_ignore_mask(ui_window_t *win, KeySym *keysyms) { return ~0; }

u_int ui_window_get_mod_meta_mask(ui_window_t *win, char *mod_key) { return ModMask; }

void ui_window_bell(ui_window_t *win, ui_bel_mode_t mode) {
  if (mode & BEL_VISUAL) {
    ui_window_blank(win);
    bl_usleep(100000); /* 100 mili sec */
    (*win->window_exposed)(win, 0, 0, win->width, win->height);
  }

  if (mode & BEL_SOUND) {
    fwrite("\x07", 1, 1, win->disp->display->fp);
  }
}

void ui_window_translate_coordinates(ui_window_t *win, int x, int y, int *global_x, int *global_y,
                                    Window *child) {
  *global_x = x + win->x;
  *global_y = y + win->y;
}

void ui_window_set_input_focus(ui_window_t *win) {
  reset_input_focus(ui_get_root_window(win));
  win->inputtable = win->is_focused = 1;
  if (win->window_focused) {
    (*win->window_focused)(win);
  }
}

/* for ui_display.c */

void ui_window_clear_margin_area(ui_window_t *win) { clear_margin_area(win); }
