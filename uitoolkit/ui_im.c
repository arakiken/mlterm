/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */
#include <stdio.h>       /* sprintf */
#include <pobl/bl_mem.h> /* malloc/alloca/free */
#include <pobl/bl_str.h> /* bl_str_alloca_dup bl_str_sep */
#include <pobl/bl_locale.h>

#include <vt_str.h>
#include <vt_parser.h>

#include "ui_im.h"
#include "ui_event_source.h"

#ifdef USE_IM_PLUGIN

#ifndef LIBDIR
#define IM_DIR "/usr/local/lib/mlterm/"
#else
#define IM_DIR LIBDIR "/mlterm/"
#endif

typedef ui_im_t *(*ui_im_new_func_t)(u_int64_t magic, vt_char_encoding_t term_encoding,
                                     ui_im_export_syms_t *syms, char *engine,
                                     u_int mod_ignore_mask);

/* --- static variables --- */

static ui_im_export_syms_t im_export_syms = {
    vt_str_init, vt_str_delete, vt_char_combine, vt_char_set, vt_get_char_encoding_name,
    vt_get_char_encoding, vt_convert_to_internal_ch, vt_isciikey_state_new,
    vt_isciikey_state_delete, vt_convert_ascii_to_iscii, vt_char_encoding_parser_new,
    vt_char_encoding_conv_new, ui_im_candidate_screen_new, ui_im_status_screen_new,
    ui_event_source_add_fd, ui_event_source_remove_fd

};

#if 1
/* restroing locale which was overwritten by SCIM */
#define RESTORE_LOCALE 1
#endif

/* --- static functions --- */

static void *im_dlopen(char *im_name) {
  char *libname;
  void *handle;

  if (!(libname = alloca(strlen(im_name) + 4))) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif

    return NULL;
  }

  sprintf(libname, "im-%s", im_name);

  if (!(handle = bl_dl_open(IM_DIR, libname))) {
    handle = bl_dl_open("", libname);
  }

  return handle;
}

static int dlsym_im_new_func(char *im_name, ui_im_new_func_t *func, bl_dl_handle_t *handle) {
  char *symname;
#ifdef PLUGIN_MODULE_SUFFIX
  char *im_name2;
#endif

  if (!im_name || !(symname = alloca(strlen(im_name) + 8))) {
    return 0;
  }

  sprintf(symname, "im_%s_new", im_name);

#ifdef PLUGIN_MODULE_SUFFIX
  if ((im_name2 = alloca(strlen(im_name) + 3 + 1))) {
    sprintf(im_name2, "%s-" PLUGIN_MODULE_SUFFIX, im_name);

    if (!(*handle = im_dlopen(im_name2))) {
#endif
      if (!(*handle = im_dlopen(im_name))) {
        return 0;
      }
#ifdef PLUGIN_MODULE_SUFFIX
    }
  }
#endif

  if (!(*func = (ui_im_new_func_t)bl_dl_func_symbol(*handle, symname))) {
    bl_dl_close(*handle);

    return 0;
  }

  return 1;
}

/* --- global functions --- */

ui_im_t *ui_im_new(ui_display_t *disp, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                   void *vtparser, ui_im_event_listener_t *im_listener,
                   char *input_method, u_int mod_ignore_mask) {
  ui_im_t *im;
  ui_im_new_func_t func;
  bl_dl_handle_t handle;
  char *im_name;
  char *im_attr;
#ifdef RESTORE_LOCALE
  char *cur_locale;
#endif

  if (input_method == NULL) {
    return NULL;
  }

  if (strcmp(input_method, "none") == 0) {
    return NULL;
  }

  if (strchr(input_method, ':')) {
    im_attr = bl_str_alloca_dup(input_method);

    if ((im_name = bl_str_sep(&im_attr, ":")) == NULL) {
#ifdef DEBUG
      bl_error_printf("%s is illegal input method.\n", input_method);
#endif

      return NULL;
    }
  } else {
    im_name = bl_str_alloca_dup(input_method);
    im_attr = NULL;
  }

#ifdef RESTORE_LOCALE
  cur_locale = bl_str_alloca_dup(bl_get_locale());
#endif

  if (!dlsym_im_new_func(im_name, &func, &handle)) {
#ifdef RESTORE_LOCALE
    bl_locale_init(cur_locale);
#endif
    bl_error_printf("%s: Could not load.\n", im_name);
    return NULL;
  }

#ifdef RESTORE_LOCALE
  bl_locale_init(cur_locale);
#endif

  if (!(im = (*func)(IM_API_COMPAT_CHECK_MAGIC, vt_parser_get_encoding((vt_parser_t*)vtparser),
                     &im_export_syms, im_attr, mod_ignore_mask))) {
    bl_error_printf("%s: Could not open.\n", im_name);

    /*
     * Even if ibus daemon was not found, ibus_init() has been
     * already called in im_ibus_new().
     * So if im-ibus module is unloaded here, ibus_init()
     * will be called again and segfault will happen when
     * im-ibus module is loaded next time.
     * Fcitx is also the same.
     */
    if (strcmp(im_name, "ibus") != 0 && strcmp(im_name, "fcitx") != 0) {
      bl_dl_close(handle);
    } else {
      bl_dl_close_at_exit(handle);
    }

    return NULL;
  }

  /*
   * initializations for ui_im_t
   */
  im->handle = handle;
  im->name = strdup(im_name);
  im->disp = disp;
  im->font_man = font_man;
  im->color_man = color_man;
  im->vtparser = vtparser;
  im->listener = im_listener;
  im->cand_screen = NULL;
  im->stat_screen = NULL;
  im->preedit.chars = NULL;
  im->preedit.num_of_chars = 0;
  im->preedit.filled_len = 0;
  im->preedit.segment_offset = 0;
  im->preedit.cursor_offset = UI_IM_PREEDIT_NOCURSOR;

  return im;
}

void ui_im_delete(ui_im_t *im) {
  bl_dl_handle_t handle;
  int do_close;

  if (strcmp(im->name, "ibus") == 0 || strcmp(im->name, "fcitx") == 0) {
    do_close = 0;
  } else {
    do_close = 1;
  }

  free(im->name);

  if (im->cand_screen) {
    (*im->cand_screen->delete)(im->cand_screen);
  }

  if (im->stat_screen) {
    (*im->stat_screen->delete)(im->stat_screen);
  }

  if (im->preedit.chars) {
    vt_str_delete(im->preedit.chars, im->preedit.num_of_chars);
  }

  handle = im->handle;

  (*im->delete)(im);

  /*
   * Don't unload libim-ibus.so or libim-fcitx.so because it depends
   * on glib which works unexpectedly.
   */
  if (do_close) {
    bl_dl_close(handle);
  }
}

void ui_im_redraw_preedit(ui_im_t *im, int is_focused) {
  (*im->listener->draw_preedit_str)(im->listener->self, im->preedit.chars, im->preedit.filled_len,
                                    im->preedit.cursor_offset);

  if (!im->cand_screen && !im->stat_screen) {
    return;
  }

  if (is_focused) {
    int x;
    int y;

    if ((*im->listener->get_spot)(im->listener->self, im->preedit.chars, im->preedit.segment_offset,
                                  &x, &y)) {
      if (im->stat_screen && (im->cand_screen && im->preedit.filled_len)) {
        (*im->stat_screen->hide)(im->stat_screen);
        (*im->cand_screen->show)(im->cand_screen);
        (*im->cand_screen->set_spot)(im->cand_screen, x, y);
      } else if (im->stat_screen) {
        (*im->stat_screen->show)(im->stat_screen);
        (*im->stat_screen->set_spot)(im->stat_screen, x, y);
      } else if (im->cand_screen && im->preedit.filled_len) {
        (*im->cand_screen->show)(im->cand_screen);
        (*im->cand_screen->set_spot)(im->cand_screen, x, y);
      }
    }
  } else {
    if (im->cand_screen) {
      (*im->cand_screen->hide)(im->cand_screen);
    }

    if (im->stat_screen) {
      (*im->stat_screen->hide)(im->stat_screen);
    }
  }
}

#else /* ! USE_IM_PLUGIN */

ui_im_t *ui_im_new(ui_display_t *disp, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                   void *vtparser, ui_im_event_listener_t *im_listener,
                   char *input_method, u_int mod_ignore_mask) {
  return NULL;
}

void ui_im_delete(ui_im_t *im) {}

void ui_im_redraw_preedit(ui_im_t *im, int is_focused) {}

#endif
