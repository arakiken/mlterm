/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_font.h"

#include <stdio.h>
#include <fcntl.h>    /* open */
#include <unistd.h>   /* close */
#include <sys/mman.h> /* mmap */
#include <string.h>   /* memcmp */
#include <sys/stat.h> /* fstat */
#include <utime.h>    /* utime */

#include <pobl/bl_def.h> /* WORDS_BIGENDIAN */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* strdup */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_conf_io.h>/* bl_get_user_rc_path */
#include <pobl/bl_util.h>   /* TOINT32 */
#include <mef/ef_char.h>
#ifdef __ANDROID__
#include <dirent.h>
#endif

#include "ui_display.h"

#define DIVIDE_ROUNDINGUP(a, b) (((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)))

#if 0
#define __DEBUG
#endif

/* --- global functions --- */

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold, int letter_space /* ignored */) {
  ui_font_t *font;
  u_int cols;

  if (type_engine != TYPE_XCORE || !(font = calloc(1, sizeof(ui_font_t)))) {
    return NULL;
  }

  if (!(font->xfont = calloc(1, sizeof(XFontStruct)))) {
    free(font);

    return NULL;
  }

  font->display = display;
  font->xfont = NULL;

  font->id = id;

  if (font->id & FONT_FULLWIDTH) {
    cols = 2;
  } else {
    cols = 1;
  }

  if (font_present & FONT_VAR_WIDTH) {
    /*
     * If you use fixed-width fonts whose width is differnet from
     * each other.
     */
    font->is_var_col_width = 1;
  }

  if (font_present & FONT_VERTICAL) {
    font->is_vertical = 1;
  }

  if (use_medium_for_bold) {
    font->double_draw_gap = 1;
  }

  font->width = font->display->col_width * cols;
  font->height = font->display->line_height;
  font->ascent = 0;

  /* x_off is *not* divided by 2 => see draw_string() in ui_window.c */

  if (col_width == 0) {
    /* standard(usascii) font */

    if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      font->x_off = font->width /* / 2 */;
      font->width *= 2;
    }

    /* letter_space and line_space are ignored. */
#if 0
    if (letter_space > 0 || font->width > -letter_space) {
      font->width += letter_space;

      if (letter_space > 0 || font->x_off * 2 > -letter_space) {
        font->x_off += (letter_space / 2);
      }
    }
#endif
  } else {
    /* not a standard(usascii) font */

    /*
     * XXX hack
     * forcibly conforming non standard font width to standard font width.
     */

    if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      if (font->width != col_width) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, col_width);

/* is_var_col_width is always false if is_vertical is true. */
#if 0
        if (!font->is_var_col_width)
#endif
        {
          if (font->width < col_width) {
            font->x_off = (col_width - font->width) /* / 2 */;
          }

          font->width = col_width;
        }
      }
    } else {
      if (font->width != col_width * cols) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, col_width * cols);

        if (!font->is_var_col_width) {
          if (font->width < col_width * cols) {
            font->x_off = (col_width * cols - font->width) /* / 2 */;
          }

          font->width = col_width * cols;
        }
      }
    }
  }

  font->size_attr = size_attr;

  /*
   * checking if font width/height/ascent member is sane.
   */

  if (font->width == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " font width is 0.\n");
#endif

    /* XXX this may be inaccurate. */
    font->width = DIVIDE_ROUNDINGUP(fontsize * cols, 2);
  }

  if (font->height == 0) {
    /* XXX this may be inaccurate. */
    font->height = fontsize;
  }

#ifdef DEBUG
  ui_font_dump(font);
#endif

  return font;
}

ui_font_t *ui_font_new_for_decsp(Display *display, vt_font_t id, u_int width, u_int height,
                                 u_int ascent) {
  return NULL;
}

void ui_font_destroy(ui_font_t *font) {
  free(font);
}

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int is_awidth,
                              int *draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

  return font->width;
}

/* Return written size */
size_t ui_convert_ucs4_to_utf16(u_char *dst, /* 4 bytes. Big endian. */
                                u_int32_t src) {
  if (src < 0x10000) {
    dst[0] = (src >> 8) & 0xff;
    dst[1] = src & 0xff;

    return 2;
  } else if (src < 0x110000) {
    /* surrogate pair */

    u_char c;

    src -= 0x10000;
    c = (u_char)(src / (0x100 * 0x400));
    src -= (c * 0x100 * 0x400);
    dst[0] = c + 0xd8;

    c = (u_char)(src / 0x400);
    src -= (c * 0x400);
    dst[1] = c;

    c = (u_char)(src / 0x100);
    src -= (c * 0x100);
    dst[2] = c + 0xdc;
    dst[3] = (u_char)src;

    return 4;
  }

  return 0;
}

#ifdef DEBUG

void ui_font_dump(ui_font_t *font) {
  bl_msg_printf("Font id %x: XFont %p (width %d, height %d, ascent %d, x_off %d)", font->id,
                font->xfont, font->width, font->height, font->ascent, font->x_off);

  if (font->is_proportional) {
    bl_msg_printf(" (proportional)");
  }

  if (font->is_var_col_width) {
    bl_msg_printf(" (var col width)");
  }

  if (font->is_vertical) {
    bl_msg_printf(" (vertical)");
  }

  if (font->double_draw_gap) {
    bl_msg_printf(" (double drawing)");
  }

  bl_msg_printf("\n");
}

#endif
