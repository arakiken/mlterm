/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_window.h"

#include <string.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_unistd.h> /* bl_usleep */
#include <mef/ef_ucs4_map.h>
#ifdef USE_WAYLAND
#include <mef/ef_utf8_parser.h>
#endif
#include <mef/ef_utf16_parser.h>

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

/* XXX Check if win is input method window or not. */
#define IS_IM_WINDOW(win) ((win)->disp->num_roots >= 2 && (win) == (win)->disp->roots[1])

/* --- static variables --- */

static int click_interval = 250; /* millisecond, same as xterm. */

/* --- static functions --- */

static int scroll_region(ui_window_t *win, int src_x, int src_y, u_int width, u_int height,
                         int dst_x, int dst_y) {
  if (!win->is_mapped || !ui_window_is_scrollable(win)) {
    return 0;
  }

  ui_display_copy_lines(win->disp, src_x + win->x + win->hmargin, src_y + win->y + win->vmargin,
                        dst_x + win->x + win->hmargin, dst_y + win->y + win->vmargin,
                        width, height);

  return 1;
}

/*
 * copy_pixel() is called more than twice, so if it is implemented as a function
 * it may be uninlined in compiling.
 * dst should be aligned.
 */
#define copy_pixel(dst, pixel, bpp)    \
  switch (bpp) {                       \
    case 1:                            \
      *(dst) = pixel;                  \
      break;                           \
    case 2:                            \
      *((u_int16_t *)(dst)) = (pixel); \
      break;                           \
    /* case  4: */                     \
    default:                           \
      *((u_int32_t *)(dst)) = (pixel); \
  }

static inline u_int16_t *memset16(u_int16_t *dst, u_int16_t i, u_int len) {
  u_int count;

  for (count = 0; count < len; count++) {
    dst[count] = i;
  }

  return dst;
}

static inline u_int32_t *memset32(u_int32_t *dst, u_int32_t i, u_int len) {
  u_int count;

  for (count = 0; count < len; count++) {
    dst[count] = i;
  }

  return dst;
}

#ifdef USE_FREETYPE
#define BLEND(fg, bg, alpha) ((bg) + ((fg) - (bg)) * (alpha) / 255)
static int copy_blended_pixel(Display *display, u_char *dst, u_char **bitmap, u_long fg, u_long bg,
                              u_int bpp) {
  int a1 = *((*bitmap)++);
  int a2 = *((*bitmap)++);
  int a3 = *((*bitmap)++);

  if (a1 + a2 + a3 > 0) {
    int r1;
    int g1;
    int b1;
    int r2;
    int g2;
    int b2;
    u_long pixel;

    r1 = PIXEL_RED(fg, display->rgbinfo) & 0xff;
    g1 = PIXEL_GREEN(fg, display->rgbinfo) & 0xff;
    b1 = PIXEL_BLUE(fg, display->rgbinfo) & 0xff;
    r2 = PIXEL_RED(bg, display->rgbinfo) & 0xff;
    g2 = PIXEL_GREEN(bg, display->rgbinfo) & 0xff;
    b2 = PIXEL_BLUE(bg, display->rgbinfo) & 0xff;

    pixel =
#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
      /*
       * f: fg color
       * b: bg color
       * w: other windows or desktop wall picture below mlterm window.
       * 0.6, 0.4: a1, a2, a3
       * 1.0, 0.0: alpha of fg_color
       * 0.8, 0.2: alpha of bg color
       *
       * Correct: 0.6*(1.0*f + 0.0*w) + 0.4*(0.8*b + 0.2*w) = 0.6*f + 0.32*b + 0.08*w
       * Lazy   : (0.6*f + 0.4*b)*(1.0*0.6+0.8*0.4) + w*(1.0-(1.0*0.6+0.8*0.4))
       *                          ^^^^^^^^^^^^^^^^^     ^^^^^^^^^^^^^^^^^^^^^^
       *          = 0.552*f + 0.368*b + 0.08*w
       */
      BLEND((fg >> 24), (bg >> 24), (a1 + a2 + a3) / 3) << 24 |
#endif
      RGB_TO_PIXEL(BLEND(r1, r2, a1), BLEND(g1, g2, a2), BLEND(b1, b2, a3), display->rgbinfo);

    copy_pixel(dst, pixel, bpp);

    return 1;
  } else {
    return 0;
  }
}
#endif

/*
 * If the width and height of drawn characters is not over the window should be checked
 * before calling ui_window_draw_*string().
 */
static void draw_string_intern(ui_window_t *win, XFontStruct *xfont, u_int font_height,
                               u_int font_ascent, u_int font_width, int font_x_off,
                               int double_draw_gap, int is_proportional, int is_var_col_width,
                               u_int32_t fg_pixel, u_int32_t bg_pixel,
                               int x, /* win->x and win->hmargin are added. */
                               int y, /* win->y and win->vmargin are added. */
                               u_char **bitmaps, u_int len, u_char *picture,
                               size_t picture_line_len, int src_bg_is_set, int bpp) {
  int clip_y;
  u_int clip_bottom;
  size_t size;
  u_char *src;
  u_char *p;
  u_int count;
  int y_off;
  int orig_x;
  u_char *bitmap_line;
  u_long pixel;

  /*
   * Check if font->height or font->ascent excesses the display height,
   * because font->height doesn't necessarily equals to the height of the
   * US-ASCII font.
   *
   * XXX
   * On the other hand, font->width is always the same (or exactly double) for now.
   */

  clip_y = win->y + win->vmargin + win->clip_y;
  if (win->clip_height == 0) {
    clip_bottom = win->y + ACTUAL_HEIGHT(win);
  } else {
    clip_bottom = clip_y + win->clip_height;
  }

  if (y >= clip_bottom) {
    return;
  } else if (y + font_height > clip_bottom) {
    font_height = clip_bottom - y;
  }

  if (y + font_height < clip_y) {
    return;
  } else if (y < clip_y) {
    y_off = clip_y - y;

    if (picture) {
      picture += (y_off * picture_line_len);
    }
  } else {
    y_off = 0;
  }

  /*
   * (len + 1) is for decreasing the number of calling alloca() below.
   * (See if (retreat - filled > font_width) block.)
   */
#ifdef USE_FREETYPE
  if (xfont->is_aa && is_proportional) {
    u_int width = font_width; /* means 'len + 1' */

    for (count = 0; count < len; count++) {
      if (bitmaps[count] && bitmaps[count][0] /* Glyph advance */ > font_width) {
        width += bitmaps[count][0];
      } else {
        width += font_width;
      }
    }

    /*
     * The caller should check if the width of specified drawn strings <= the width
     * of the window.
     */
#if 0
    if (x + width > win->width + win->hmargin + win->x) {
      width = win->width + win->hmargin + win->x - x;
    }
#endif

    size = width * bpp;

    /* This is true only if USE_SDL2 and USE_BG_TEXTURE. (see ui_draw_str.c) */
    if (src_bg_is_set && picture) {
      picture = NULL;
      picture_line_len = 0;
      src_bg_is_set = 0;
    }

#if defined(DEBUG) && defined(USE_SDL2) && !defined(USE_BG_TEXTURE)
    {
      static int output_msg;
      if (!output_msg) {
        bl_debug_printf("mlterm is built without USE_BG_TEXTURE, "
                        "so screen is corrupt with -otl option.\n");
        output_msg = 1;
      }
    }
#endif
  } else
#endif
  {
    size = (len + 1) * font_width * bpp;
  }

#if defined(USE_SDL2) && defined(USE_BG_TEXTURE)
  if (bg_pixel == win->bg_color.pixel || !src_bg_is_set) {
    bg_pixel = 0x0;
    src_bg_is_set = 1;
  }
#endif

  if (!(src = alloca(size))) {
    return;
  }

#ifdef USE_FREETYPE
  if (!xfont->face || !xfont->is_aa)
#endif
  {
    u_int y_off_bytes;
    /*
     * If font_width is larger than xfont->width_full, memory access must be limited
     * to the latter one. (Don't excess xfont->glyph_width_bytes)
     */
    u_int max_glyph_width = xfont->width_full;

    if (max_glyph_width + font_x_off > font_width) {
      max_glyph_width = font_width - font_x_off;
    }

#if 1
    /* Optimization for most cases */
    if (src_bg_is_set && !double_draw_gap) {
      int x_off;
      u_int end_gap = font_width - font_x_off - max_glyph_width;
      u_int y_off_bytes = y_off * xfont->glyph_width_bytes;

      switch (bpp) {
      case 1:
        for (; y_off < font_height; y_off++, y_off_bytes += xfont->glyph_width_bytes) {
          p = (picture ? memcpy(src, (picture += picture_line_len), size)
                       : memset(src, bg_pixel, size));

          for (count = 0; count < len; count++) {
            if (!ui_get_bitmap_line(xfont, bitmaps[count], y_off_bytes, bitmap_line)) {
              p += font_width;
            } else {
              p += font_x_off;

              for (x_off = 0; x_off < max_glyph_width; x_off++, p++) {
                if (ui_get_bitmap_cell(bitmap_line, x_off)) {
                  *p = fg_pixel;
                }
              }

              p += end_gap;
            }
          }

          ui_display_put_image(win->disp, x, y + y_off, src, p - src, 0);
        }

        return;

      case 2:
        for (; y_off < font_height; y_off++, y_off_bytes += xfont->glyph_width_bytes) {
          p = (picture ? memcpy(src, (picture += picture_line_len), size)
                       : memset16(src, bg_pixel, size / 2));

          for (count = 0; count < len; count++) {
            if (!ui_get_bitmap_line(xfont, bitmaps[count], y_off_bytes, bitmap_line)) {
              p += (font_width * 2);
            } else {
              p += (font_x_off * 2);

              for (x_off = 0; x_off < max_glyph_width; x_off++, p += 2) {
                if (ui_get_bitmap_cell(bitmap_line, x_off)) {
                  *((u_int16_t *)p) = fg_pixel;
                }
              }

              p += (end_gap * 2);
            }
          }

          ui_display_put_image(win->disp, x, y + y_off, src, p - src, 0);
        }

        return;

      /* case  4: */
      default:
        for (; y_off < font_height; y_off++, y_off_bytes += xfont->glyph_width_bytes) {
          p = (picture ? memcpy(src, (picture += picture_line_len), size)
                       : memset32(src, bg_pixel, size / 4));

          for (count = 0; count < len; count++) {
            if (!ui_get_bitmap_line(xfont, bitmaps[count], y_off_bytes, bitmap_line)) {
              p += (font_width * 4);
            } else {
              p += (font_x_off * 4);

              for (x_off = 0; x_off < max_glyph_width; x_off++, p += 4) {
                if (ui_get_bitmap_cell(bitmap_line, x_off)) {
                  *((u_int32_t *)p) = fg_pixel;
                }
              }

              p += (end_gap * 4);
            }
          }

          ui_display_put_image(win->disp, x, y + y_off, src, p - src, 0);
        }

        return;
      }
    }
#endif

    orig_x = x;

    for (; y_off < font_height; y_off++) {
      if (src_bg_is_set) {
        if (picture) {
          memcpy(src, (picture += picture_line_len), size);
        } else {
          switch (bpp) {
          case 1:
            memset(src, bg_pixel, size);
            break;

          case 2:
            memset16(src, bg_pixel, size / 2);
            break;

          /* case  4: */
          default:
            memset32(src, bg_pixel, size / 4);
            break;
          }
        }
      }

      p = src;

      y_off_bytes = y_off * xfont->glyph_width_bytes;

      for (count = 0; count < len; count++, x += font_width) {
        int x_off;

        if (!ui_get_bitmap_line(xfont, bitmaps[count], y_off_bytes, bitmap_line)) {
          if (src_bg_is_set) {
            p += (font_width * bpp);
          } else {
            for (x_off = 0; x_off < font_width; x_off++, p += bpp) {
              pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
              copy_pixel(p, pixel, bpp);
            }
          }
        } else {
          int force_fg = 0;
          u_int glyph_end = font_x_off + max_glyph_width;

          for (x_off = 0; ; x_off++, p += bpp) {
            if (font_x_off <= x_off && x_off < glyph_end &&
                ui_get_bitmap_cell(bitmap_line, x_off - font_x_off)) {
              pixel = fg_pixel;
              force_fg = double_draw_gap;
            } else if (font_width <= x_off) {
              break;
            } else {
              if (force_fg) {
                pixel = fg_pixel;
                force_fg = 0;
              } else if (src_bg_is_set) {
                continue;
              } else {
                pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
              }
            }

            copy_pixel(p, pixel, bpp);
          }
        }
      }

      ui_display_put_image(win->disp, (x = orig_x), y + y_off, src, p - src, !src_bg_is_set);
    }
  }
#if defined(USE_FREETYPE)
  else {
    int orig_y_off = y_off;

    orig_x = x;

    for (; y_off < font_height; y_off++) {
      int prev_crowded_out = 0;

      if (src_bg_is_set) {
        if (picture) {
          memcpy(src, (picture += picture_line_len), size);
        } else {
          switch (bpp) {
          case 1:
            memset(src, bg_pixel, size);
            break;

          case 2:
            memset16(src, bg_pixel, size / 2);
            break;

          /* case  4: */
          default:
            memset32(src, bg_pixel, size / 4);
            break;
          }
        }
      }

      p = src;

      if (is_proportional) {
        /*
         * Bitmap format
         * 1 byte    1 byte    1 byte        1 byte    1 byte    1 byte
         * [advance] [retreat] [glyph width] [R alpha] [G alpha] [B alpha] ...
         */
        for (count = 0; count < len; count++) {
          /*
           * src_bg_is_set is always false if is_proportional is true.
           * This is because the edge of a glyph which is forced out from a cell
           * can be redrawn incorrectly even if is_var_col_width is false.
           * (see #ifdef USE_FRAMEBUFFER #ifdef USE_FREETYPE
           * #endif #endif in xcore_draw_str() in ui_draw_str.c)
           */
          int x_off;

          if (!bitmaps[count]) {
            if (src_bg_is_set /* && !picture */) {
              p += (font_width * bpp);
            } else {
              for (x_off = 0; x_off < font_width; x_off++, p += bpp) {
                pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                copy_pixel(p, pixel, bpp);
              }
            }

            if (prev_crowded_out > font_width) {
              prev_crowded_out -= font_width;
            } else {
              prev_crowded_out = 0;
            }

            x += font_width;
          } else {
            int retreat;
            int width;

            width = bitmaps[count][2];

            bitmap_line = bitmaps[count] + 3 /* header */ + y_off * width * 3;

            if ((retreat = bitmaps[count][1]) > 0) {
              u_int filled;

              if ((filled = (p - src) / bpp) < retreat) {
                while (x >= retreat) {
                  int x_off2;

                  x_off = -retreat;

                  if (y_off == orig_y_off) {
                    u_char *new_src;

                    if (retreat - filled > font_width) { /* See (len + 1) above */
                      size_t new_size = size + (retreat - filled - font_width) * bpp;

                      if (!(new_src = alloca(new_size))) {
                        break;
                      }

                      memcpy(new_src, src, p - src);
                      p = new_src + (p - src);
                      src = new_src;
                      size = new_size;
                    }

                    orig_x -= (retreat - filled);
                  } else {
                    x += (retreat - filled);
                  }

                  memmove(src + (retreat - filled) * bpp, src, p - src);

                  /*
                   * Don't use bg_pixel even if src_bg_is_set and !picture, because retreated
                   * pixels can be uncleared by overwriting glyphs afterwards.
                   */
                  for (x_off2 = 0, p = src; x_off2 < retreat - filled; x_off2++, p += bpp) {
                    pixel = ui_display_get_pixel(win->disp, x + x_off + x_off2, y + y_off);
                    copy_pixel(p, pixel, bpp);
                  }

                  p = src;
                  goto end_of_retreat;
                }

                /* Give up retreating before 'filled'. */
                bitmap_line += (retreat - filled);
                x_off = -filled;
                p = src;
              } else {
                x_off = -retreat;
                p -= (retreat * bpp);
              }
            } else {
              x_off = 0;
            }

          end_of_retreat:
            width -= retreat;

            if (is_var_col_width) {
              int advance = bitmaps[count][0];

              if (src_bg_is_set /* && !picture */) {
                for (; x_off < width; x_off++, p += bpp) {
                  if (copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg_pixel,
                                         bpp)) {
                    /* do nothing */
                  } else if (x_off >= prev_crowded_out) {
                    copy_pixel(p, bg_pixel, bpp);
                  }
                }

                for (; x_off < advance; x_off++, p += bpp) {
                  if (x_off >= prev_crowded_out) {
                    copy_pixel(p, bg_pixel, bpp);
                  }
                }
              } else {
                for (; x_off < width; x_off++, p += bpp) {
                  u_long bg = ui_display_get_pixel(win->disp, x + x_off, y + y_off);

                  if (copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg, bpp)) {
                    /* do nothing */
                  } else if (x_off >= prev_crowded_out) {
                    copy_pixel(p, bg, bpp);
                  }
                }

                for (; x_off < advance; x_off++, p += bpp) {
                  if (x_off >= prev_crowded_out) {
                    pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                    copy_pixel(p, pixel, bpp);
                  }
                }
              }

              if (count + 1 < len) {
                if (prev_crowded_out > advance) {
                  prev_crowded_out -= advance;
                } else {
                  prev_crowded_out = 0;
                }

                if (advance > 0 && x_off != advance) {
                  p += ((advance - x_off) * bpp);

                  if (x_off > advance && prev_crowded_out < x_off - advance) {
                    prev_crowded_out = x_off - advance;
                  }
                }

                x += advance;
              } else {
                prev_crowded_out = 0;
              }
            } else {
              /* is_var_col_width is false */
              int padding;

              width += font_x_off;

              /*
               * font_x_off is added to width above, so 'width + font_x_off'
               * means that font_x_off * 2 is added to width.
               */
#ifdef CENTER_PROPORTIONAL_GLYPHS
              if (width + font_x_off < font_width) {
                int diff = (font_width - width - font_x_off) / 2;

                width += diff;
                padding = font_x_off + diff;
              } else
#endif
              {
                if (width + font_x_off > font_width) {
                  int diff = (width + font_x_off - font_width) / 2;

                  if (diff > font_x_off) {
                    padding = 0;

                    /*
                     * If you use Inconsolata font with --otl option, === is shaped
                     * with garbage by this.
                     */
#if 0
                    bitmap_line += (diff - font_x_off) * 3;
#endif
                  } else {
                    padding = font_x_off - diff;
                  }
                } else {
                  padding = font_x_off;
                }

                if (width > (len - count) * font_width) {
                  width = (len - count) * font_width;
                }
              }

              if (padding > 0) {
                if ((!src_bg_is_set /* || picture */) && padding + x_off > 0) {
                  int copy_and_skip = padding;

                  if (x_off < 0) {
                    /* x_off < 0 has been already copied from ui_display_get_pixel(). */
                    copy_and_skip += (x_off);
                    p += (-x_off * bpp);
                    x_off = 0;
                  }

                  for (; copy_and_skip > 0; copy_and_skip--, x_off++, p += bpp) {
                    if (x_off >= prev_crowded_out) {
                      pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                      copy_pixel(p, pixel, bpp);
                    }
                  }
                } else {
                  /* x_off < 0 has been already copied from ui_display_get_pixel(). */
                  p += (padding * bpp);
                  x_off += padding;
                }
              }

              if (src_bg_is_set /* && !picture */) {
                for (; x_off < width; x_off++, p += bpp) {
                  if (copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg_pixel,
                                         bpp)) {
                    /* do nothing */
                  } else if (x_off >= prev_crowded_out) {
                    copy_pixel(p, bg_pixel, bpp);
                  }
                }

                for (; x_off < font_width; x_off++, p += bpp) {
                  copy_pixel(p, bg_pixel, bpp);
                }
              } else {
                for (; x_off < width; x_off++, p += bpp) {
                  u_long bg = ui_display_get_pixel(win->disp, x + x_off, y + y_off);

                  if (copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg, bpp)) {
                    /* do nothing */
                  } else if (x_off >= prev_crowded_out) {
                    copy_pixel(p, bg, bpp);
                  }
                }

                for (; x_off < font_width; x_off++, p += bpp) {
                  pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                  copy_pixel(p, pixel, bpp);
                }
              }

              /* If count == len - 1, width <= font_width is always true. */
              if (width <= font_width) {
                prev_crowded_out = 0;
              } else {
                prev_crowded_out = width - font_width;
                p -= (prev_crowded_out * bpp);
              }

              x += font_width;
            }
          }
        }
      } else {
        /* is_proportional is false */
        for (count = 0; count < len; count++, x += font_width) {
          int x_off;

          if (!bitmaps[count]) {
            if (src_bg_is_set) {
              p += (font_width * bpp);
            } else {
              for (x_off = 0; x_off < font_width; x_off++, p += bpp) {
                pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                copy_pixel(p, pixel, bpp);
              }
            }
          } else {
            u_int glyph_end = font_x_off + ((u_int16_t*)bitmaps[count])[1];
            int padding;

            /*
             * font_x_off is added to width above, so 'width + font_x_off'
             * means that font_x_off * 2 is added to width.
             */
#ifdef CENTER_PROPORTIONAL_GLYPHS
            if (glyph_end + font_x_off < font_width) {
              int diff = (font_width - glyph_end - font_x_off) / 2;

              glyph_end += diff;
              padding = font_x_off + diff;
            } else
#endif
            {
              padding = font_x_off;

              if (glyph_end > font_width) {
                glyph_end = font_width;
              }
            }

            bitmap_line = bitmaps[count] + 4 + y_off * ((u_int16_t*)bitmaps[count])[0];

            if (src_bg_is_set) {
              if (picture) {
                for (x_off = 0; ; x_off++, p += bpp) {
                  if (padding <= x_off && x_off < glyph_end) {
                    u_long bg = (bpp == 2) ? *((u_int16_t *)p) : *((u_int32_t *)p);
                    copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg, bpp);
                  } else if (font_width <= x_off) {
                    break;
                  }
                }
              } else {
                for (x_off = 0; ; x_off++, p += bpp) {
                  if (padding <= x_off && x_off < glyph_end) {
                    copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel,
                                       bg_pixel, bpp);
                  } else if (font_width <= x_off) {
                    break;
                  }
                }
              }
            } else {
              for (x_off = 0; ; x_off++, p += bpp) {
                if (padding <= x_off && x_off < glyph_end) {
                  u_long bg = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                  if (copy_blended_pixel(win->disp->display, p, &bitmap_line, fg_pixel, bg, bpp)) {
                    continue;
                  }
                } else if (font_width <= x_off) {
                  break;
                }

                pixel = ui_display_get_pixel(win->disp, x + x_off, y + y_off);
                copy_pixel(p, pixel, bpp);
              }
            }
          }
        }
      }

      ui_display_put_image(win->disp, (x = orig_x), y + y_off, src, p - src, !src_bg_is_set);
    }
  }
#endif
}

static void draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                        ui_color_t *bg_color,      /* must be NULL if wall_picture_bg is 1 */
                        int x, int y, u_char *str, /* 'len * ch_len' bytes */
                        u_int len, u_int ch_len, int wall_picture_bg) {
  u_int bpp;
  XFontStruct *xfont;
  u_char **bitmaps;
  u_int font_height;
  u_int font_ascent;
  u_char *picture;
  size_t picture_line_len;
  u_int count;
  int src_bg_is_set;
  int use_ot_layout;
  int get_space_glyph;
  int x_off;

  if (!win->is_mapped) {
    return;
  }

  if (!(bitmaps = alloca((len * sizeof(*bitmaps))))) {
    return;
  }

  bpp = win->disp->display->bytes_per_pixel;
  xfont = font->xfont;

#ifdef USE_OT_LAYOUT
  use_ot_layout = (font->use_ot_layout /* && font->ot_font */);
#else
  use_ot_layout = 0;
#endif

  /* Following check is done by the caller of this function. */
#if 0
  /*
   * Check if font->width * len excesses the display height
   * because full width fonts can be used for characters which console
   * applications regard as half width.
   */

  if (x + font->width * len > win->width) {
    len = (win->width - x) / font->width;
  }
#endif

  x += (win->hmargin + win->x);
  y = y + (win->vmargin + win->y) - font->ascent;

  if (wall_picture_bg) {
    /* bg_color is always NULL */
    Pixmap pic;
    int pic_x;
    int pic_y;

    if (win->wall_picture == ParentRelative) {
      pic = win->parent->wall_picture;
      pic_x = x;
      pic_y = y;
    } else {
      pic = win->wall_picture;
      pic_x = x - win->x;
      pic_y = y - win->y;
    }

    picture_line_len = pic->width * bpp;
    picture = pic->image + pic_y * picture_line_len +
              /* - picture_line_len is for picture += picture_line_len below. */
              pic_x * bpp - picture_line_len;

    src_bg_is_set = 1;
  } else {
    picture = NULL;

    if (bg_color) {
      src_bg_is_set = 1;
    } else {
      src_bg_is_set = 0;
    }
  }

  x_off = font->x_off;
  /* See ui_calculate_char_width() in ui_font.c */
  get_space_glyph = font->is_proportional && font->is_var_col_width;

  if (ch_len == 1) {
    for (count = 0; count < len; count++) {
      bitmaps[count] = ui_get_bitmap(xfont, str + count, 1, use_ot_layout,
                                     get_space_glyph, NULL);
    }
  } else /* if(ch_len == 2) */ {
    XFontStruct *compl_xfont;

#if defined(USE_FREETYPE)
    if (xfont->face && !IS_ISO10646_UCS4(font->id)) {
      u_char *str_p = str;
      ef_char_t non_ucs;
      ef_char_t ucs4;

      non_ucs.size = 2;
      non_ucs.cs = FONT_CS(font->id);
      non_ucs.property = 0;
      for (count = 0; count < len; count++, str_p += 2) {
        memcpy(non_ucs.ch, str_p, 2);
        if (!ef_map_to_ucs4(&ucs4, &non_ucs)) {
          continue;
        }

        if (ucs4.ch[0] == '\0' && ucs4.ch[1] == '\0') {
          memcpy(str_p, ucs4.ch + 2, 2);
        }
#if 0
        else {
          memcpy(str_p, ucs4.ch, 4);
          ch_len = 4;
        }
#endif
      }
    }
#endif

    for (count = 0; count < len;) {
      if (0xd8 <= str[0] && str[0] <= 0xdb) {
        if (use_ot_layout || count >= --len) {
          break;
        }

        if (0xdc <= str[2] && str[2] <= 0xdf) {
          /* surrogate pair */
          ef_int_to_bytes(str, 4, (str[0] - 0xd8) * 0x100 * 0x400 + str[1] * 0x400 +
                                       (str[2] - 0xdc) * 0x100 + str[3] + 0x10000);
          ch_len = 4;
        } else {
          /* illegal, ignored. */
          len--;
          count--;
          str += 4;
          continue;
        }
      }

      compl_xfont = NULL;
      bitmaps[count] = ui_get_bitmap(font->xfont, str, ch_len, use_ot_layout,
                                     get_space_glyph, &compl_xfont);
      if (compl_xfont && xfont != compl_xfont) {
        u_int w;

        if (count > 0) {
          font_ascent = font->ascent;
          font_height = font->height;
          if (ui_modify_bitmaps(xfont, bitmaps, count, &font_height, &font_ascent)) {
            draw_string_intern(win, xfont, font_height, font_ascent, font->width, x_off,
                               font->double_draw_gap, font->is_proportional, font->is_var_col_width,
                               fg_color->pixel, bg_color ? bg_color->pixel : 0, x, y,
                               bitmaps, count, picture, picture_line_len, src_bg_is_set, bpp);
          }
          x += (font->width * count);

          bitmaps[0] = bitmaps[count];
          len -= count;
          count = 0;
        } else {
          count++;
        }
        xfont = compl_xfont;

        /* see ui_font.c */
        if (font->id & FONT_FULLWIDTH) {
          w = xfont->width_full;
        } else {
          w = xfont->width;
        }

        if (w >= font->width) {
          x_off = 0;
        } else {
          x_off = (font->width - w) / 2;
        }
      } else {
        count++;
      }

      str += ch_len;
      ch_len = 2;
    }
  }

  font_ascent = font->ascent;
  font_height = font->height;
  if (ui_modify_bitmaps(xfont, bitmaps, len, &font_height, &font_ascent)) {
    draw_string_intern(win, xfont, font_height, font_ascent, font->width, x_off,
                       font->double_draw_gap, font->is_proportional, font->is_var_col_width,
                       fg_color->pixel, bg_color ? bg_color->pixel : 0, x, y,
                       bitmaps, len, picture, picture_line_len, src_bg_is_set, bpp);
  }
}

static int copy_area(ui_window_t *win, Pixmap src, PixmapMask mask, int src_x, /* can be minus */
                     int src_y,                                                /* can be minus */
                     u_int width, u_int height, int dst_x,                     /* can be minus */
                     int dst_y,                                                /* can be minus */
                     int accept_margin /* x/y can be minus and over width/height */
                     ) {
  int hmargin;
  int vmargin;
  int right_margin;
  int bottom_margin;
  int y_off;
  u_int bpp;
  u_char *picture;
  size_t src_width_size;

  if (!win->is_mapped) {
    return 0;
  }

  if (accept_margin) {
    hmargin = win->hmargin;
    vmargin = win->vmargin;
    right_margin = bottom_margin = 0;
  } else {
    hmargin = vmargin = 0;
    right_margin = RIGHT_MARGIN(win);
    bottom_margin = BOTTOM_MARGIN(win);
  }

  if (dst_x >= (int)win->width + hmargin || dst_y >= (int)win->height + vmargin) {
    return 0;
  }

  if (dst_x + width > win->width + hmargin - right_margin) {
    width = win->width + hmargin - right_margin - dst_x;
  }

  if (dst_y + height > win->height + vmargin - bottom_margin) {
    height = win->height + vmargin - bottom_margin - dst_y;
  }

  bpp = win->disp->display->bytes_per_pixel;
  src_width_size = src->width * bpp;
  picture = src->image + src_width_size * (vmargin + src_y) + bpp * (hmargin + src_x);

  if (mask) {
    mask += ((vmargin + src_y) * src->width + hmargin + src_x);

    for (y_off = 0; y_off < height; y_off++) {
      int x_off;
      u_int w;

      w = 0;
      for (x_off = 0; x_off < width; x_off++) {
        if (mask[x_off]) {
          w++;

          if (x_off + 1 == width) {
            /* for x_off - w */
            x_off++;
          } else {
            continue;
          }
        } else if (w == 0) {
          continue;
        }

        ui_display_put_image(win->disp, win->x + win->hmargin + dst_x + x_off - w,
                             win->y + win->vmargin + dst_y + y_off, picture + bpp * (x_off - w),
                             w * bpp, 0);
        w = 0;
      }

      mask += src->width;
      picture += src_width_size;
    }
  } else {
    size_t size;

    size = width * bpp;

    for (y_off = 0; y_off < height; y_off++) {
      ui_display_put_image(win->disp, win->x + win->hmargin + dst_x,
                           win->y + win->vmargin + dst_y + y_off,
                           picture, size, 0);
      picture += src_width_size;
    }
  }

  return 1;
}

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

#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  /* 
   * Switching input method engines invokes unfocus and focus. In this case,
   * it is necessary to search a window which was focused most recently.
   */
  if (win->inputtable > 0) {
    win->inputtable = -1;
  } else if (win->inputtable < 0) {
    if (win->inputtable > -10) {
      win->inputtable --;
    }
  }
#else
  if (win->inputtable) {
    win->inputtable = -1;
  }
#endif
  else {
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

#ifdef USE_WAYLAND
static void check_update_window(ui_window_t *win, int x /* parent */, int y /* parent */) {
  u_int count;

  x += win->x;
  y += win->y;

  if ((win->parent || win->num_children == 0) && /* XXX ui_layout_t is not updated. */
      (ui_display_get_pixel(win->disp, x + ACTUAL_WIDTH(win) / 2,
                            y + ACTUAL_HEIGHT(win) / 2) == 0 ||
       ui_display_get_pixel(win->disp, x + ACTUAL_WIDTH(win) - 1,
                            y + ACTUAL_HEIGHT(win) - 1) == 0)) {
    /* This window doesn't seem to have been drawn correctly yet after ui_display_resize(). */
    ui_window_update_all(win);
  }

  for (count = 0; count < win->num_children; count++) {
    check_update_window(win->children[count], x, y);
  }
}
#endif

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

static void convert_to_decsp_font_index(u_char *str, u_int len) {
  while (len != 0) {
    if (*str == 0x5f) {
      *str = 0x7f;
    } else if (0x5f < *str && *str < 0x7f) {
      (*str) -= 0x5f;
    }

    len--;
    str++;
  }
}

/* --- global functions --- */

int ui_window_init(ui_window_t *win, u_int width, u_int height, u_int min_width, u_int min_height,
                   u_int width_inc, u_int height_inc, u_int hmargin, u_int vmargin, int create_gc,
                   int inputtable) {
  memset(win, 0, sizeof(ui_window_t));

  /* If wall picture is set, scrollable will be 0. */
  win->is_scrollable = 1;

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  win->is_focused = 1;
#endif
  win->inputtable = inputtable;
  win->is_mapped = 1;

  win->create_gc = create_gc;

  win->width = width;
  win->height = height;
  win->min_width = min_width;
  win->min_height = min_height;
  win->width_inc = width_inc;
  win->height_inc = height_inc;
  win->sizehint_flag = SIZEHINT_WIDTH|SIZEHINT_HEIGHT;
  win->hmargin = hmargin;
  win->vmargin = vmargin;

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
#ifdef USE_WAYLAND
  ui_display_clear_clipboard(win->disp, win);
#endif

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
#if defined(USE_SDL2) && defined(USE_BG_TEXTURE)
  ui_display_set_wall_picture(win->disp, pic->image, pic->width, pic->height);

  return 0; /* to free pic memory */
#else
  u_int count;
#ifdef USE_GRF
  int ret;

  if ((ret = x68k_tvram_set_wall_picture(pic->image, pic->width, pic->height))) {
    win->wall_picture = DummyPixmap; /* dummy */

    /* Don't set is_scrollable = 0. */

    /* If ret == 2, text vram was initialized just now. */
    if (ret == 2) {
      clear_margin_area(win);

      if (win->window_exposed) {
        (*win->window_exposed)(win, 0, 0, win->width, win->height);
      }
    }

    return 0; /* to free pic memory. */
  }
#endif

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
#endif
}

int ui_window_unset_wall_picture(ui_window_t *win, int do_expose) {
  u_int count;

#ifdef USE_GRF
  x68k_tvram_set_wall_picture(NULL, 0, 0);
#endif

#if defined(USE_SDL2) && defined(USE_BG_TEXTURE)
  ui_display_set_wall_picture(win->disp, NULL, 0, 0);
#endif

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

#if defined(USE_SDL2) && defined(USE_BG_TEXTURE)
  ui_display_set_bg_color(win->disp, bg_color->pixel);
#endif

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

#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  /*
   * The default value of is_focused is 0 on wayland, while 1 on framebuffer.
   * (see ui_window_init())
   */
  if (!(child->is_mapped = map) && child->inputtable > 0) {
    child->inputtable = -1;
  }
#else
  if ((child->is_mapped = map) && win->is_focused && child->inputtable) {
    child->is_focused = 1;
  } else {
    child->is_focused = 0;
    if (child->inputtable > 0) {
      child->inputtable = -1;
    }
  }
#endif

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

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if (!win->parent && win->x == 0 && win->y == 0) {
    ui_window_resize_with_margin(win, win->disp->width, win->disp->height, NOTIFY_TO_MYSELF);
  }
#else
  /* win->window_realized() which was executed with is_mapped == 0 doesn't draw anything. */
  ui_window_update_all(win);
#endif

  return 1;
}

void ui_window_map(ui_window_t *win) {
  if (!win->is_mapped) {
    win->is_mapped = 1;

    clear_margin_area(win);
    (*win->window_exposed)(win, 0, 0, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win));
  }
}

void ui_window_unmap(ui_window_t *win) {
  win->is_mapped = 0;
}

int ui_window_resize(ui_window_t *win, u_int width, /* excluding margin */
                     u_int height,                  /* excluding margin */
                     ui_resize_flag_t flag          /* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
                     ) {
#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if ((flag & NOTIFY_TO_PARENT) &&
      /* XXX Check if win is input method window or not. */
      !IS_IM_WINDOW(win)) {
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
#endif

  if (win->width == width && win->height == height) {
    return 0;
  }

#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if (win->parent == NULL) {
    ui_display_resize(win->disp, width + win->hmargin * 2, height + win->vmargin * 2);
#ifdef USE_SDL2
    if (win->disp->display->resizing) {
      return 1;
    }
#endif
  } else if (flag & NOTIFY_TO_PARENT) {
    return ui_window_resize(win->parent, win->parent->width + width - win->width ,
                            win->parent->height + height - win->height,
                            NOTIFY_TO_MYSELF);
  }
#endif

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

#ifdef USE_WAYLAND
  /*
   * ui_display_resize() clears screen.
   * (win is always root here.)
   */
  check_update_window(win, 0, 0);
#endif

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

void ui_window_set_maximize_flag(ui_window_t *win, ui_maximize_flag_t flag) {
#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if (flag == MAXIMIZE_FULL) {
    ui_display_set_maximized(win->disp, 1);
  } else if (flag == MAXIMIZE_RESTORE) {
    ui_display_set_maximized(win->disp, 0);
  }
#endif
}

void ui_window_set_normal_hints(ui_window_t *win, u_int min_width, u_int min_height,
                                u_int width_inc, u_int height_inc) {
  win->min_width = min_width;
  win->min_height = min_height;
  win->width_inc = width_inc;
  win->height_inc = height_inc;
}

void ui_window_set_override_redirect(ui_window_t *win, int flag) {}

int ui_window_set_borderless_flag(ui_window_t *win, int flag) { return 0; }

int ui_window_move(ui_window_t *win, int x, int y) {
  if (win->parent) {
    x += win->parent->hmargin;
    y += win->parent->vmargin;
  }
#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  else {
    /*
     * Don't do "win->x = x; win->y = y;" here.
     * ui_window_xxx(..., x, y) functions add win->x and win->y to x and y arguments,
     * but it causes unexpected result because MANAGE_ROOT_WINDOWS_BY_MYSELF means that
     * win->x and win->y of root windows are always 0.
     */
    return ui_display_move(win->disp, x, y);
  }
#endif


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
#ifdef USE_GRF
  if (x68k_tvram_is_enabled()) {
    ui_window_fill_with(win, &black, x, y, width, height);
  } else
#endif
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

    copy_area(win, pic, None, src_x, src_y, width, height, x, y, 1);
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
  u_char *src;
  size_t size;
  int y_off;
  u_int bpp;
  u_long pixel;

  if (!win->is_mapped) {
    return;
  }

#if defined(USE_SDL2) && defined(USE_BG_TEXTURE)
  if (color->pixel == win->bg_color.pixel) {
    pixel = 0;
  } else
#endif
  {
    pixel = color->pixel;
  }

  x += (win->x + win->hmargin);
  y += (win->y + win->vmargin);

  if ((bpp = win->disp->display->bytes_per_pixel) == 1) {
    ui_display_fill_with(x, y, width, height, (u_int8_t)pixel);
  } else {
    if (!(src = alloca((size = width * bpp)))) {
      return;
    }

    for (y_off = 0; y_off < height; y_off++) {
      u_char *p = src;
      int x_off;

      if (bpp == 2) {
        for (x_off = 0; x_off < width; x_off++) {
          *((u_int16_t *)p) = pixel;
          p += 2;
        }
      } else /* if (bpp == 4) */
      {
        for (x_off = 0; x_off < width; x_off++) {
          *((u_int32_t *)p) = pixel;
          p += 4;
        }
      }

      ui_display_put_image(win->disp, x, y + y_off, src, size, 0);
    }
  }
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
#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  else if (event->type == FocusOut) {
    reset_input_focus(win);
  }
#endif

  return 1;
}

size_t ui_window_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                         KeySym *keysym, XKeyEvent *event) {
  u_int32_t ch;

  if (seq_len == 0) {
    return 0;
  }

  *parser = NULL;

#ifdef USE_WAYLAND
  *keysym = event->ksym;
  {
    u_char buf[7]; /* UTF_MAX_SIZE + 1 */
    size_t len = ui_display_get_utf8(buf, event->ksym);

    if (len >= 2) {
      if (seq_len >= len) {
        static ef_parser_t *utf8_parser;

        if (utf8_parser == NULL) {
          utf8_parser = ef_utf8_parser_new(); /* XXX leaked */
        }

        *parser = utf8_parser;
        memcpy(seq, buf, len);
      }

      return len;
    } else if (len == 0) {
      return 0;
    } else {
      ch = buf[0];
    }
  }
#else
#ifdef USE_SDL2
  if (event->str) {
    size_t len = strlen(event->str);
    if (len <= seq_len) {
      strncpy(seq, event->str, len);
      *parser = event->parser;
      *keysym = event->ksym;
    }

    return len;
  }
#endif

  ch = event->ksym;

#ifdef __ANDROID__
  if (ch == 0) {
    return ui_display_get_str(seq, seq_len);
  }
#endif

  if ((*keysym = event->ksym) >= 0x100) {
#ifndef USE_SDL2
#if defined(__linux__) || defined(__FreeBSD__)
    /*
     * Linux:
     * 0x00-0xff  : ASCII + ISO8859-1-R
     * 0x100-0x263: Function Keys (Max: KEY_CLEAR(0x163) + 0x100)
     * 0x1000-    : Unicode (Use after decrement 0x1000) (See kcode_to_ksym() in ui_display_linux.c)
     *
     * FreeBSD:
     * 0x00-0xff  : ASCII + ISO8859-1-R
     * 0x100-0x256: Function Keys (Max: KEY_KP0(0x156) + 0x100)
     * 0x1100-    : Unicode (Use after decrement 0x1000)
     *              (See receive_key_event() in ui_display_freebsd.c)
     */
    if (*keysym >= 0x1000) {
      ch -= 0x1000;

      goto ucs;
    } else
#endif
#endif
    {
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
    }
  } else if (*keysym == XK_Tab && (event->state & ShiftMask)) {
    *keysym = XK_ISO_Left_Tab;

    return 0;
  }
#endif

  /*
   * Control + '@'(0x40) ... '_'(0x5f) -> 0x00 ... 0x1f
   *
   * Not "<= '_'" but "<= 'z'" because Control + 'a' is not
   * distinguished from Control + 'A'.
   */
  if (ch < 0x80) {
    if ((event->state & ControlMask) && (ch == ' ' || ('@' <= ch && ch <= 'z'))) {
      seq[0] = (ch & 0x1f);
    } else {
      seq[0] = ch;
    }

    return 1;
  }

ucs:
  if (seq_len >= 2) {
    static ef_parser_t *utf16_parser;

    if (utf16_parser == NULL) {
      utf16_parser = ef_utf16_parser_new(); /* XXX leaked */
    }

    *parser = utf16_parser;

    seq[0] = (ch >> 8) & 0xff;
    seq[1] = ch & 0xff;

    /*
     * Disable mod_meta_mode for UTF-16.
     * e.g.) "mod_meta_mode = 8bit" modifies 0x20AC to 0xA0AC.
     *       (see key_pressed() in ui_screen.c
     */
    event->state &= ~ModMask;
  }

  return 2;
}

/*
 * Scroll functions.
 * The caller side should clear the scrolled area.
 */

int ui_window_scroll_upward(ui_window_t *win, u_int height) {
  return ui_window_scroll_upward_region(win, 0, win->height, height);
}

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
int ui_window_is_scrollable(ui_window_t *win) {
  /* XXX If input method module is activated, don't scroll window. */
  if (win->is_scrollable && !IM_WINDOW_IS_ACTIVATED(win->disp)) {
    return 1;
  } else {
    return 0;
  }
}
#endif

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
  return copy_area(win, src, mask, src_x, src_y, width, height, dst_x, dst_y, 0);
}

void ui_window_set_clip(ui_window_t *win, int x, int y, u_int width, u_int height) {
  win->clip_y = y;
  win->clip_height = height;
}

void ui_window_unset_clip(ui_window_t *win) { win->clip_y = win->clip_height = 0; }

void ui_window_draw_decsp_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                 int y, u_char *str, u_int len) {
  convert_to_decsp_font_index(str, len);

  ui_window_draw_string(win, font, fg_color, x, y, str, len);
}

void ui_window_draw_decsp_image_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                       ui_color_t *bg_color, /* NULL => use wall_picture for bg */
                                       int x, int y, u_char *str, u_int len) {
  convert_to_decsp_font_index(str, len);

  ui_window_draw_image_string(win, font, fg_color, bg_color, x, y, str, len);
}

void ui_window_draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x, int y,
                           u_char *str, u_int len) {
  draw_string(win, font, fg_color, NULL, x, y, str, len, 1, 0);
}

void ui_window_draw_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x, int y,
                             XChar2b *str, u_int len) {
  draw_string(win, font, fg_color, NULL, x, y, (u_char *)str, len, 2, 0);
}

void ui_window_draw_image_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                 ui_color_t *bg_color, /* NULL => use wall_picture for bg */
                                 int x, int y, u_char *str, u_int len) {
#ifdef USE_GRF
  if (bg_color == NULL && x68k_tvram_is_enabled()) {
    bg_color = &black;
  }
#endif

  draw_string(win, font, fg_color, bg_color, x, y, str, len, 1, bg_color == NULL);
}

void ui_window_draw_image_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                   ui_color_t *bg_color, /* NULL => use wall_picture for bg */
                                   int x, int y, XChar2b *str, u_int len) {
#ifdef USE_GRF
  if (bg_color == NULL && x68k_tvram_is_enabled()) {
    bg_color = &black;
  }
#endif

  draw_string(win, font, fg_color, bg_color, x, y, (u_char *)str, len, 2, bg_color == NULL);
}

void ui_window_draw_rect_frame(ui_window_t *win, int x1, int y1, int x2, int y2) {
  ui_window_fill_with(win, &win->fg_color, x1, y1, x2 - x1 + 1, 1);
  ui_window_fill_with(win, &win->fg_color, x1, y1, 1, y2 - y1 + 1);
  ui_window_fill_with(win, &win->fg_color, x1, y2, x2 - x1 + 1, 1);
  ui_window_fill_with(win, &win->fg_color, x2, y1, 1, y2 - y1 + 1);
}

int ui_window_set_selection_owner(ui_window_t *win, Time time, ui_selection_flag_t selection) {
#ifdef USE_SDL2
  /*
   * Set selection_owner NULL for ui_display_own_selection() not to
   * call ui_display_clear_selection().
   */
  win->disp->selection_owner = NULL;
#else
  if (ui_window_is_selection_owner(win, selection)) {
    /* Already owner */
  } else
#endif
#ifdef USE_WAYLAND
  if (selection == SEL_CLIPBOARD) {
    ui_display_own_clipboard(win->disp, win);
  } else
#endif
  {
    ui_display_own_selection(win->disp, win);
  }

#ifdef __ANDROID__
  if (win->utf_selection_requested) {
    (*win->utf_selection_requested)(win, NULL, 0);
  }
#endif

  return 1;
}

int ui_window_xct_selection_request(ui_window_t *win, Time time, ui_selection_flag_t selection) {
#if defined(__ANDROID__) || defined(USE_WAYLAND)
  return 0;
#else
  if (win->disp->selection_owner && win->disp->selection_owner->xct_selection_requested) {
    XSelectionRequestEvent ev;
    ev.type = 0;
    ev.target = win;
    (*win->disp->selection_owner->xct_selection_requested)(win->disp->selection_owner, &ev, 0);
  }

  return 1;
#endif
}

int ui_window_utf_selection_request(ui_window_t *win, Time time, ui_selection_flag_t selection) {
#if defined(__ANDROID__)
  ui_display_request_text_selection();
#elif defined(USE_WAYLAND)
  if (selection == SEL_CLIPBOARD) {
    ui_display_request_text_clipboard(win->disp);
  } else {
    ui_display_request_text_selection(win->disp);
  }
#elif defined(USE_SDL2)
  ui_display_request_text_selection(win->disp);
#else
  if (win->disp->selection_owner && win->disp->selection_owner->utf_selection_requested) {
    XSelectionRequestEvent ev;
    ev.type = 1;
    ev.target = win;
    (*win->disp->selection_owner->utf_selection_requested)(win->disp->selection_owner, &ev, 0);
  }
#endif

  return 1;
}

void ui_window_send_picture_selection(ui_window_t *win, Pixmap pixmap, u_int width, u_int height) {}

void ui_window_send_text_selection(ui_window_t *win, XSelectionRequestEvent *req_ev,
                                   u_char *sel_data, size_t sel_len, Atom sel_type) {
#if defined(__ANDROID__)
  ui_display_send_text_selection(sel_data, sel_len);
#elif defined(USE_WAYLAND) || defined(USE_SDL2)
  ui_display_send_text_selection(win->disp, req_ev, sel_data, sel_len);
#else
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
#endif
}

void ui_set_window_name(ui_window_t *win, u_char *name) {
#ifndef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if (name == NULL) {
    name = win->app_name;
  }

  ui_display_set_title(win->disp, name);
#endif
}

void ui_set_icon_name(ui_window_t *win, u_char *name) {}

void ui_window_set_icon(ui_window_t *win, ui_icon_picture_ptr_t icon) {}

void ui_window_remove_icon(ui_window_t *win) {}

void ui_window_reset_group(ui_window_t *win) {}

void ui_set_click_interval(int interval) {
  click_interval = interval;
}

int ui_get_click_interval(void) {
  return click_interval;
}

u_int ui_window_get_mod_ignore_mask(ui_window_t *win, KeySym *keysyms) {
  return ~0;
}

u_int ui_window_get_mod_meta_mask(ui_window_t *win, char *mod_key) { return ModMask; }

void ui_window_bell(ui_window_t *win, ui_bel_mode_t mode) {
  if (mode & BEL_VISUAL) {
    ui_window_blank(win);
    bl_usleep(100000); /* 100 mili sec */
    (*win->window_exposed)(win, 0, 0, win->width, win->height);
  }
}

void ui_window_translate_coordinates(ui_window_t *win, int x, int y, int *global_x, int *global_y) {
  *global_x = x + win->x;
  *global_y = y + win->y;

#if defined(ROTATABLE_DISPLAY) && !defined(MANAGE_ROOT_WINDOWS_BY_MYSELF)
  /* USE_WAYLAND|USE_SDL2 */
  ui_display_logical_to_physical_coordinates(win->disp, global_x, global_y);
#endif
}

void ui_window_set_input_focus(ui_window_t *win) {
  if (win->inputtable > 0 && win->is_focused) {
    return;
  }

  reset_input_focus(ui_get_root_window(win));
  win->inputtable = win->is_focused = 1;
  if (win->window_focused) {
    (*win->window_focused)(win);
  }
}

/* for ui_display.c */

void ui_window_clear_margin_area(ui_window_t *win) { clear_margin_area(win); }
