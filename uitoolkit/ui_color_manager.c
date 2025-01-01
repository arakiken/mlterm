/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_color_manager.h"

#include <stdio.h>  /* sprintf */
#include <string.h> /* memset */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* strdup */

enum {
  _FG_COLOR = 0x0,
  _BG_COLOR = 0x1,
  _BOLD_COLOR = 0x2,
  _UNDERLINE_COLOR = 0x3,
  _BLINKING_COLOR = 0x4,
  _REVERSE_COLOR = 0x5,
  _ITALIC_COLOR = 0x6,
  _CROSSED_OUT_COLOR = 0x7,
  _CUR_FG_COLOR = 0x8,
  _CUR_BG_COLOR = 0x9,
  MAX_SYS_COLORS = 0xa,
};

/* --- static functions --- */

static int sys_color_set(ui_color_manager_t *color_man, const char *name, int color) {
  ui_color_t xcolor;

  if (bl_compare_str(color_man->sys_colors[color].name, name) == 0) {
    /* Not changed (specified color name is not changed) */
    return 0;
  }

  if (name) {
    if (!ui_load_xcolor(color_man->color_cache, &xcolor, name)) {
      if (!color_man->sys_colors[color].name && color <= _BG_COLOR) {
        /* _FG_COLOR and _BG_COLOR are necessarily loaded. */
        name = "black";
        xcolor = color_man->color_cache->black;
      } else {
        /* Not changed (specified color name is illegal) */
        return 0;
      }
    }
  }

  if (color_man->sys_colors[color].name) {
    ui_unload_xcolor(color_man->color_cache->disp, &color_man->sys_colors[color].xcolor);
    free(color_man->sys_colors[color].name);
  }

  if (name) {
    if (color == _BG_COLOR && color_man->alpha < 255) {
      u_int8_t red;
      u_int8_t green;
      u_int8_t blue;
      u_int8_t alpha;

      ui_get_xcolor_rgba(&red, &green, &blue, &alpha, &xcolor);

      /*
       * If alpha of bg color is already less than 255,
       * default alpha value is not applied.
       */
      if (alpha == 255) {
        ui_unload_xcolor(color_man->color_cache->disp, &xcolor);
        ui_load_rgb_xcolor(color_man->color_cache->disp, &xcolor, red, green, blue,
                           color_man->alpha);
      }
    }

    color_man->sys_colors[color].name = strdup(name);
    color_man->sys_colors[color].xcolor = xcolor;
  } else {
    color_man->sys_colors[color].name = NULL;
  }

  return 1;
}

/* --- global functions --- */

ui_color_manager_t *ui_color_manager_new(ui_display_t *disp,
                                         char *fg_color, /* can be NULL(=> "black".) */
                                         char *bg_color, /* can be NULL(=> "white".) */
                                         char *cursor_fg_color, /* can be NULL(=> reversed color) */
                                         char *cursor_bg_color, /* can be NULL(=> reversed color) */
                                         char *bd_color, /* can be NULL */
                                         char *ul_color, /* can be NULL */
                                         char *bl_color, /* can be NULL */
                                         char *rv_color, /* can be NULL */
                                         char *it_color, /* can be NULL */
                                         char *co_color  /* can be NULL */) {
  ui_color_manager_t *color_man;

  if ((color_man = calloc(1, sizeof(ui_color_manager_t))) == NULL) {
    return NULL;
  }

  if (!(color_man->color_cache = ui_acquire_color_cache(disp, 100))) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " ui_aquire_color_cache failed.\n");
#endif

    free(color_man);

    return NULL;
  }

  color_man->alpha = 255;

  sys_color_set(color_man, fg_color ? fg_color : "black", _FG_COLOR);
  sys_color_set(color_man, bg_color ? bg_color : "white", _BG_COLOR);
  sys_color_set(color_man, cursor_fg_color, _CUR_FG_COLOR);
  sys_color_set(color_man, cursor_bg_color, _CUR_BG_COLOR);
  sys_color_set(color_man, bd_color, _BOLD_COLOR);
  sys_color_set(color_man, ul_color, _UNDERLINE_COLOR);
  sys_color_set(color_man, bl_color, _BLINKING_COLOR);
  sys_color_set(color_man, rv_color, _REVERSE_COLOR);
  sys_color_set(color_man, it_color, _ITALIC_COLOR);
  sys_color_set(color_man, co_color, _CROSSED_OUT_COLOR);

  return color_man;
}

void ui_color_manager_destroy(ui_color_manager_t *color_man) {
  int count;

  for (count = 0; count < MAX_SYS_COLORS; count++) {
    if (color_man->sys_colors[count].name) {
      ui_unload_xcolor(color_man->color_cache->disp, &color_man->sys_colors[count].xcolor);
      free(color_man->sys_colors[count].name);
    }
  }

  ui_release_color_cache(color_man->color_cache);

  if (color_man->alt_color_cache) {
    ui_release_color_cache(color_man->alt_color_cache);
  }

  free(color_man);
}

int ui_color_manager_set_fg_color(ui_color_manager_t *color_man,
                                  const char *name /* never NULL */) {
  return sys_color_set(color_man, name, _FG_COLOR);
}

int ui_color_manager_set_bg_color(ui_color_manager_t *color_man,
                                  const char *name /* never NULL */) {
  return sys_color_set(color_man, name, _BG_COLOR);
}

int ui_color_manager_set_cursor_fg_color(ui_color_manager_t *color_man,
                                         const char *name /* can be NULL */) {
  return sys_color_set(color_man, name, _CUR_FG_COLOR);
}

int ui_color_manager_set_cursor_bg_color(ui_color_manager_t *color_man,
                                         const char *name /* can be NULL */) {
  return sys_color_set(color_man, name, _CUR_BG_COLOR);
}

int ui_color_manager_set_alt_color(ui_color_manager_t *color_man,
                                   vt_color_t color, /* VT_BOLD_COLOR - VT_CROSSED_OUT_COLOR */
                                   const char *name        /* never NULL */) {
  return sys_color_set(color_man, name, color - VT_FG_COLOR);
}

char *ui_color_manager_get_fg_color(ui_color_manager_t *color_man) {
  return color_man->sys_colors[_FG_COLOR].name;
}

char *ui_color_manager_get_bg_color(ui_color_manager_t *color_man) {
  return color_man->sys_colors[_BG_COLOR].name;
}

char *ui_color_manager_get_cursor_fg_color(ui_color_manager_t *color_man) {
  return color_man->sys_colors[_CUR_FG_COLOR].name;
}

char *ui_color_manager_get_cursor_bg_color(ui_color_manager_t *color_man) {
  return color_man->sys_colors[_CUR_BG_COLOR].name;
}

char *ui_color_manager_get_alt_color(ui_color_manager_t *color_man,
                                     vt_color_t color /* VT_BOLD_COLOR - VT_CROSSED_OUT_COLOR */) {
  return color_man->sys_colors[color - VT_FG_COLOR].name;
}

ui_color_t *ui_get_xcolor(ui_color_manager_t *color_man, vt_color_t color) {
  if (color_man->is_reversed) {
    if (color == VT_FG_COLOR) {
      color = VT_BG_COLOR;
    } else if (color == VT_BG_COLOR) {
      color = VT_FG_COLOR;
    }
  }

  if (IS_FG_BG_COLOR(color)) {
    return &color_man->sys_colors[color - VT_FG_COLOR].xcolor;
  } else if (IS_ALT_COLOR(color)) {
    if (color_man->sys_colors[color - VT_FG_COLOR].name) {
      return &color_man->sys_colors[color - VT_FG_COLOR].xcolor;
    } else {
      return &color_man->sys_colors[_FG_COLOR].xcolor;
    }
  } else {
    return ui_get_cached_xcolor(color_man->color_cache, color);
  }
}

/*
 * If fading status is changed, 1 is returned.
 */
int ui_color_manager_fade(ui_color_manager_t *color_man,
                          u_int fade_ratio /* valid value is 0 - 99 */) {
  ui_color_cache_t *color_cache;

  if (fade_ratio >= 100) {
    return 0;
  }

  if (fade_ratio == color_man->color_cache->fade_ratio) {
    return 0;
  }

  if (color_man->alt_color_cache && fade_ratio == color_man->alt_color_cache->fade_ratio) {
    color_cache = color_man->alt_color_cache;
    color_man->alt_color_cache = color_man->color_cache;
  } else {
    if ((color_cache = ui_acquire_color_cache(color_man->color_cache->disp, fade_ratio)) == NULL) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " ui_aquire_color_cache failed.\n");
#endif
      return 0;
    }

    if (color_man->color_cache->fade_ratio == 100) {
      if (color_man->alt_color_cache) {
        ui_release_color_cache(color_man->alt_color_cache);
      }

      color_man->alt_color_cache = color_man->color_cache;
    }
  }

  color_man->color_cache = color_cache;

  ui_color_manager_reload(color_man);

  return 1;
}

/*
 * If fading status is changed, 1 is returned.
 */
int ui_color_manager_unfade(ui_color_manager_t *color_man) {
  ui_color_cache_t *color_cache;

  if (color_man->alt_color_cache == NULL || color_man->color_cache->fade_ratio == 100) {
    return 0;
  }

  color_cache = color_man->alt_color_cache;
  color_man->alt_color_cache = color_man->color_cache;
  color_man->color_cache = color_cache;

  ui_color_manager_reload(color_man);

  return 1;
}

int ui_color_manager_reverse_video(ui_color_manager_t *color_man) {
  if (color_man->is_reversed) {
    return 0;
  }

  color_man->is_reversed = 1;

  return 1;
}

int ui_color_manager_restore_video(ui_color_manager_t *color_man) {
  if (!color_man->is_reversed) {
    return 0;
  }

  color_man->is_reversed = 0;

  return 1;
}

/*
 * Swap the color of VT_BG_COLOR <=> that of cursor fg color.
 * Deal VT_BG_COLOR as cursor fg color.
 */
int ui_color_manager_adjust_cursor_fg_color(ui_color_manager_t *color_man) {
  struct sys_color tmp_color;

  if (!color_man->sys_colors[_CUR_FG_COLOR].name) {
    return 0;
  }

  tmp_color = color_man->sys_colors[_BG_COLOR];
  color_man->sys_colors[_BG_COLOR] = color_man->sys_colors[_CUR_FG_COLOR];
  color_man->sys_colors[_CUR_FG_COLOR] = tmp_color;

  return 1;
}

/*
 * Swap the color of VT_FG_COLOR <=> that of cursor bg color.
 * Deal VT_FG_COLOR as cursor bg color.
 */
int ui_color_manager_adjust_cursor_bg_color(ui_color_manager_t *color_man) {
  struct sys_color tmp_color;

  if (!color_man->sys_colors[_CUR_BG_COLOR].name) {
    return 0;
  }

  tmp_color = color_man->sys_colors[_FG_COLOR];
  color_man->sys_colors[_FG_COLOR] = color_man->sys_colors[_CUR_BG_COLOR];
  color_man->sys_colors[_CUR_BG_COLOR] = tmp_color;

  return 1;
}

/*
 * Reload system colors.
 */
void ui_color_manager_reload(ui_color_manager_t *color_man) {
  int color;

  for (color = 0; color < MAX_SYS_COLORS; color++) {
    if (color_man->sys_colors[color].name) {
      char *name;

      name = color_man->sys_colors[color].name;
      color_man->sys_colors[color].name = NULL;

      /* reload */
      sys_color_set(color_man, name, color);

      free(name);
    }
  }
}

int ui_change_true_transbg_alpha(ui_color_manager_t *color_man, u_int8_t alpha) {
#ifndef SUPPORT_TRUE_TRANSPARENT_BG

  return 0;

#else

#ifdef USE_WIN32GUI
  if (alpha == 0) {
    /* If alpha == 0, window disappears completely. */
    return 0;
  }
#else
  if (color_man->color_cache->disp->depth != 32) {
    return 0;
  } else
#endif
  if (alpha == color_man->alpha) {
    return -1;
  } else {
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;
    u_int8_t cur_alpha;

    ui_get_xcolor_rgba(&red, &green, &blue, &cur_alpha, &color_man->sys_colors[_BG_COLOR].xcolor);

    if (cur_alpha == color_man->alpha) {
      ui_unload_xcolor(color_man->color_cache->disp, &color_man->sys_colors[_BG_COLOR].xcolor);
      ui_load_rgb_xcolor(color_man->color_cache->disp, &color_man->sys_colors[_BG_COLOR].xcolor,
                         red, green, blue, alpha);
    }

    color_man->alpha = alpha;

    return 1;
  }

#endif
}
