/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * im_scim_mod_if.c - SCIM plugin for mlterm (part of module interface)
 *
 * Copyright (C) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 *
 * $Id$
 *
 */

#include <pobl/bl_locale.h> /* bl_get_locale */
#include <pobl/bl_debug.h>  /* bl_error_printf, bl_warn_printf */
#include <pobl/bl_str.h>    /* bl_str_alloca_dup */

#include <ui_im.h>
#include "../im_common.h"
#include "../im_info.h"

#include "im_scim.h"

typedef struct im_scim {
  /* input method common object */
  ui_im_t im;

  im_scim_context_t context;

  vt_char_encoding_t term_encoding;

  ef_parser_t *parser_term; /* for term encoding  */
  ef_conv_t *conv;

} im_scim_t;

/* --- static variables --- */

static int ref_count = 0;
static int initialized = 0;
static ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
static ef_parser_t *parser_utf8 = NULL;
static int panel_fd = -1;

/* --- static functions --- */

/*
 * methods of ui_im_t
 */

static void delete(ui_im_t *im) {
  im_scim_t *scim;

  scim = (im_scim_t *)im;

  im_scim_destroy_context(scim->context);

  if (scim->parser_term) {
    (*scim->parser_term->delete)(scim->parser_term);
  }

  if (scim->conv) {
    (*scim->conv->delete)(scim->conv);
  }

  free(scim);

  ref_count--;

  if (ref_count == 0) {
    if (panel_fd >= 0) {
      (*syms->ui_event_source_remove_fd)(panel_fd);
      panel_fd = -1;
    }

    im_scim_finalize();

    if (parser_utf8) {
      (*parser_utf8->delete)(parser_utf8);
      parser_utf8 = NULL;
    }

    initialized = 0;
  }
}

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  return im_scim_key_event(((im_scim_t *)im)->context, ksym, event);
}

static int switch_mode(ui_im_t *im) { return im_scim_switch_mode(((im_scim_t *)im)->context); }

static int is_active(ui_im_t *im) { return im_scim_is_on(((im_scim_t *)im)->context); }

static void focused(ui_im_t *im) { im_scim_focused(((im_scim_t *)im)->context); }

static void unfocused(ui_im_t *im) { im_scim_unfocused(((im_scim_t *)im)->context); }

/*
 * callbacks (im_scim.cpp --> im_scim_mod_if.c)
 */

static void commit(void *ptr, char *utf8_str) {
  im_scim_t *scim;
  u_char conv_buf[256];
  size_t filled_len;

  scim = (im_scim_t *)ptr;

  if (scim->term_encoding == VT_UTF8) {
    (*scim->im.listener->write_to_term)(scim->im.listener->self, (u_char *)utf8_str,
                                        strlen(utf8_str));
    goto skip;
  }

  (*parser_utf8->init)(parser_utf8);
  (*parser_utf8->set_str)(parser_utf8, (u_char *)utf8_str, strlen(utf8_str));

  (*scim->conv->init)(scim->conv);

  while (!parser_utf8->is_eos) {
    filled_len = (*scim->conv->convert)(scim->conv, conv_buf, sizeof(conv_buf), parser_utf8);

    if (filled_len == 0) {
      /* finished converting */
      break;
    }

    (*scim->im.listener->write_to_term)(scim->im.listener->self, conv_buf, filled_len);
  }

skip:
  if (scim->im.cand_screen) {
    (*scim->im.cand_screen->delete)(scim->im.cand_screen);
    scim->im.cand_screen = NULL;
  }
}

static void preedit_update(void *ptr, char *utf8_str, int cursor_offset) {
  im_scim_t *scim;
  u_char *str;
  vt_char_t *p;
  ef_char_t ch;
  u_int count = 0;
  u_int index = 0;
  int saved_segment_offset;

  scim = (im_scim_t *)ptr;

  if (scim->im.preedit.chars) {
    (*syms->vt_str_delete)(scim->im.preedit.chars, scim->im.preedit.num_chars);
    scim->im.preedit.chars = NULL;
  }

  saved_segment_offset = scim->im.preedit.segment_offset;

  scim->im.preedit.num_chars = 0;
  scim->im.preedit.filled_len = 0;
  scim->im.preedit.segment_offset = -1;
  scim->im.preedit.cursor_offset = UI_IM_PREEDIT_NOCURSOR;

  if (utf8_str == NULL) {
    goto draw;
  }

  if (!strlen(utf8_str)) {
    goto draw;
  }

  if (scim->term_encoding != VT_UTF8) {
    /* utf8 -> term encoding */
    (*parser_utf8->init)(parser_utf8);
    (*scim->conv->init)(scim->conv);

    if (!(im_convert_encoding(parser_utf8, scim->conv, (u_char *)utf8_str, &str,
                              strlen(utf8_str) + 1))) {
      return;
    }
  } else {
    str = (u_char *)utf8_str;
  }

  /*
   * count number of characters to allocate im.preedit.chars
   */

  (*scim->parser_term->init)(scim->parser_term);
  (*scim->parser_term->set_str)(scim->parser_term, (u_char *)str, strlen(str));

  while ((*scim->parser_term->next_char)(scim->parser_term, &ch)) {
    count++;
  }

  /*
   * allocate im.preedit.chars
   */
  if (!(scim->im.preedit.chars = malloc(sizeof(vt_char_t) * count))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    scim->im.preedit.chars = NULL;
    scim->im.preedit.num_chars = 0;
    scim->im.preedit.filled_len = 0;

    if (scim->term_encoding != VT_UTF8) {
      free(str);
    }

    return;
  }

  scim->im.preedit.num_chars = count;

  /*
   * u_char --> vt_char_t
   */

  p = scim->im.preedit.chars;
  (*syms->vt_str_init)(p, count);

  (*scim->parser_term->init)(scim->parser_term);
  (*scim->parser_term->set_str)(scim->parser_term, (u_char *)str, strlen(str));

  index = 0;

  while ((*scim->parser_term->next_char)(scim->parser_term, &ch)) {
    vt_color_t fg_color = VT_FG_COLOR;
    vt_color_t bg_color = VT_BG_COLOR;
    u_int attr;
    int is_fullwidth = 0;
    int is_comb = 0;
    int is_underline = 0;
    int is_bold = 0;

    if (index == cursor_offset) {
      scim->im.preedit.cursor_offset = cursor_offset;
    }

    if ((*syms->vt_convert_to_internal_ch)(scim->im.vtparser, &ch) <= 0) {
      continue;
    }

    if (ch.property & EF_FULLWIDTH) {
      is_fullwidth = 1;
    } else if (ch.property & EF_AWIDTH) {
      /* TODO: check col_size_of_width_a */
      is_fullwidth = 1;
    }

    attr = im_scim_preedit_char_attr(scim->context, index);
    if (attr & CHAR_ATTR_UNDERLINE) {
      is_underline = 1;
    }
    if (attr & CHAR_ATTR_REVERSE) {
      if (scim->im.preedit.segment_offset == -1) {
        scim->im.preedit.segment_offset = index;
      }
      fg_color = VT_BG_COLOR;
      bg_color = VT_FG_COLOR;
    }
    if (attr & CHAR_ATTR_BOLD) {
      is_bold = 1;
    }

    if (ch.property & EF_COMBINING) {
      is_comb = 1;

      if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb,
                                   fg_color, bg_color, is_bold, 0, is_underline, 0, 0, 0)) {
        index++;
        continue;
      }

      /*
       * if combining failed , char is normally appended.
       */
    }

    (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, fg_color, bg_color,
                         is_bold, 0, is_underline, 0, 0, 0);

    p++;
    scim->im.preedit.filled_len++;
    index++;
  }

  if (scim->term_encoding != VT_UTF8) {
    free(str);
  }

  if (scim->im.preedit.filled_len && scim->im.preedit.cursor_offset == UI_IM_PREEDIT_NOCURSOR) {
    scim->im.preedit.cursor_offset = scim->im.preedit.filled_len;
  }

draw:
  (*scim->im.listener->draw_preedit_str)(scim->im.listener->self, scim->im.preedit.chars,
                                         scim->im.preedit.filled_len,
                                         scim->im.preedit.cursor_offset);

  /* Drop the current candidates since the segment is changed */
  if (saved_segment_offset != scim->im.preedit.segment_offset && scim->im.cand_screen) {
    (*scim->im.cand_screen->delete)(scim->im.cand_screen);
    scim->im.cand_screen = NULL;
  }
}

static void candidate_update(void *ptr, int is_vertical_lookup, uint num_candiate, char **str,
                             int index) {
  im_scim_t *scim;
  int x;
  int y;
  int i;

  scim = (im_scim_t *)ptr;

  (*scim->im.listener->get_spot)(scim->im.listener->self, scim->im.preedit.chars,
                                 scim->im.preedit.segment_offset, &x, &y);

  if (scim->im.cand_screen == NULL) {
    if (index == 0) {
      return;
    }

    if (!(scim->im.cand_screen = (*syms->ui_im_candidate_screen_new)(
              scim->im.disp, scim->im.font_man, scim->im.color_man, scim->im.vtparser,
              (*scim->im.listener->is_vertical)(scim->im.listener->self), is_vertical_lookup,
              (*scim->im.listener->get_line_height)(scim->im.listener->self), x, y))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ui_im_candidate_screen_new() failed.\n");
#endif

      return;
    }

    scim->im.cand_screen->listener.self = scim;
    scim->im.cand_screen->listener.selected = NULL; /* TODO */
  }

  if (!(*scim->im.cand_screen->init)(scim->im.cand_screen, num_candiate, num_candiate)) {
    (*scim->im.cand_screen->delete)(scim->im.cand_screen);
    scim->im.cand_screen = NULL;

    return;
  }

  (*scim->im.cand_screen->set_spot)(scim->im.cand_screen, x, y);

  for (i = 0; i < num_candiate; i++) {
    u_char *p = NULL;

    if (scim->term_encoding != VT_UTF8) {
      (*parser_utf8->init)(parser_utf8);
      if (im_convert_encoding(parser_utf8, scim->conv, str[i], &p, strlen(str[i]) + 1)) {
        (*scim->im.cand_screen->set)(scim->im.cand_screen, scim->parser_term, p, i);
        free(p);
      }
    } else {
      (*scim->im.cand_screen->set)(scim->im.cand_screen, scim->parser_term, str[i], i);
    }
  }

  (*scim->im.cand_screen->select)(scim->im.cand_screen, index);
}

static void candidate_show(void *ptr) {
  im_scim_t *scim;

  scim = (im_scim_t *)ptr;

  if (scim->im.cand_screen) {
    (*scim->im.cand_screen->show)(scim->im.cand_screen);
  }
}

static void candidate_hide(void *ptr) {
  im_scim_t *scim;

  scim = (im_scim_t *)ptr;

  if (scim->im.cand_screen) {
    (*scim->im.cand_screen->hide)(scim->im.cand_screen);
  }
}

static im_scim_callbacks_t callbacks = {commit,         preedit_update, candidate_update,
                                        candidate_show, candidate_hide, NULL};

/*
 * panel
 */

static void panel_read_handler(void) {
  if (!im_scim_receive_panel_event()) {
    (*syms->ui_event_source_remove_fd)(panel_fd);
    panel_fd = -1;
  }
}

/* --- module entry point --- */

ui_im_t *im_scim_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                     ui_im_export_syms_t *export_syms, char *unused, u_int mod_ignore_mask) {
  im_scim_t *scim = NULL;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

#if 1
#define RESTORE_LOCALE
#endif

  if (!initialized) {
    char *cur_locale;

#ifdef RESTORE_LOCALE
    /*
     * Workaround against make_locale() of m17nlib.
     */
    cur_locale = bl_str_alloca_dup(bl_get_locale());
#else
    cur_locale = bl_get_locale();
#endif
    if (!im_scim_initialize(cur_locale)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " failed to initialize SCIM.");
#endif

      return NULL;
    }
#ifdef RESTORE_LOCALE
    /* restoring */
    /*
     * TODO: remove valgrind warning.
     * The memory space pointed to by sys_locale in bl_locale.c
     * was freed by setlocale() in m17nlib.
     */
    bl_locale_init(cur_locale);
#endif

    syms = export_syms;

    if ((panel_fd = im_scim_get_panel_fd()) >= 0) {
      (*syms->ui_event_source_add_fd)(panel_fd, panel_read_handler);
    }

    if (!(parser_utf8 = (*syms->vt_char_encoding_parser_new)(VT_UTF8))) {
      goto error;
    }

    initialized = 1;
  }

  if (!(scim = malloc(sizeof(im_scim_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  scim->context = NULL;
  scim->term_encoding = term_encoding;
  scim->conv = NULL;

  if (scim->term_encoding != VT_UTF8) {
    if (!(scim->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
      goto error;
    }
  }

  if (!(scim->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }

  if (!(scim->context = im_scim_create_context(scim, &callbacks))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " im_scim_create_context failed.\n");
#endif

    goto error;
  }

  /*
   * set methods of ui_im_t
   */
  scim->im.delete = delete;
  scim->im.key_event = key_event;
  scim->im.switch_mode = switch_mode;
  scim->im.is_active = is_active;
  scim->im.focused = focused;
  scim->im.unfocused = unfocused;

  ref_count++;

  return (ui_im_t *)scim;

error:

  if (scim) {
    if (scim->context) {
      im_scim_destroy_context(scim->context);
    }

    if (scim->conv) {
      (*scim->conv->delete)(scim->conv);
    }

    if (scim->parser_term) {
      (*scim->parser_term->delete)(scim->parser_term);
    }

    free(scim);
  }

  if (ref_count == 0) {
    if (panel_fd >= 0) {
      (*syms->ui_event_source_remove_fd)(panel_fd);
      panel_fd = -1;
    }

    im_scim_finalize();

    if (parser_utf8) {
      (*parser_utf8->delete)(parser_utf8);
      parser_utf8 = NULL;
    }
  }

  return NULL;
}

/* --- module entry point for external tools --- */

im_info_t *im_scim_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->id = strdup("scim");
  result->name = strdup("SCIM");
  result->num_args = 0;
  result->args = NULL;
  result->readable_args = NULL;

  return result;
}
