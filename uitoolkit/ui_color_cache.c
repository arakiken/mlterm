/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_color_cache.h"

#include <stdio.h>
#include <pobl/bl_debug.h>

/* --- static variables --- */

static ui_color_cache_t **color_caches;
static u_int num_of_caches;

/* --- static functions --- */

static ui_color_cache_256ext_t *acquire_color_cache_256ext(ui_display_t *disp) {
  u_int count;
  ui_color_cache_256ext_t *cache;

  for (count = 0; count < num_of_caches; count++) {
    if (color_caches[count]->disp == disp && color_caches[count]->cache_256ext) {
      color_caches[count]->cache_256ext->ref_count++;

      return color_caches[count]->cache_256ext;
    }
  }

  if ((cache = calloc(1, sizeof(ui_color_cache_256ext_t))) == NULL) {
    return NULL;
  }

  cache->ref_count = 1;

  return cache;
}

static ui_color_t *get_cached_256ext_xcolor(ui_color_cache_t *color_cache, vt_color_t color) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  if (!IS_256EXT_COLOR(color)) {
    return NULL;
  }

  if (!color_cache->cache_256ext &&
      !(color_cache->cache_256ext = acquire_color_cache_256ext(color_cache->disp))) {
    return NULL;
  }

  if (color_cache->cache_256ext->is_loaded[color - 16]) {
    if (IS_256_COLOR(color) || !vt_ext_color_is_changed(color)) {
      return &color_cache->cache_256ext->xcolors[color - 16];
    } else {
      u_int count;

      for (count = 0; count < num_of_caches; count++) {
        if (color_caches[count]->cache_256ext) {
          ui_unload_xcolor(color_caches[count]->disp,
                           &color_caches[count]->cache_256ext->xcolors[color - 16]);
          color_caches[count]->cache_256ext->is_loaded[color - 16] = 0;
        }
      }
    }
  }

  if (!vt_get_color_rgba(color, &red, &green, &blue, &alpha) ||
      !ui_load_rgb_xcolor(color_cache->disp, &color_cache->cache_256ext->xcolors[color - 16], red,
                          green, blue, alpha)) {
    return NULL;
  }

/*
 * 16-479 colors ignore color_cache->fade_ratio.
 */

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " new color %x %x\n", color,
                  color_cache->cache_256ext->xcolors[color - 16].pixel);
#endif

  color_cache->cache_256ext->is_loaded[color - 16] = 1;

  return &color_cache->cache_256ext->xcolors[color - 16];
}

static ui_color_t *get_cached_vtsys_xcolor(ui_color_cache_t *color_cache, vt_color_t color) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;

  if (!IS_VTSYS_COLOR(color)) {
    return NULL;
  }

  if (color_cache->is_loaded[color]) {
    return &color_cache->xcolors[color];
  }

  if (!vt_get_color_rgba(color, &red, &green, &blue, &alpha) ||
      !ui_load_rgb_xcolor(color_cache->disp, &color_cache->xcolors[color], red, green, blue,
                          alpha)) {
    return NULL;
  }

  if (color_cache->fade_ratio < 100) {
    if (!ui_xcolor_fade(color_cache->disp, &color_cache->xcolors[color], color_cache->fade_ratio)) {
      return NULL;
    }
  }

#ifdef DEBUG
#ifdef UI_COLOR_HAS_RGB
  bl_debug_printf(BL_DEBUG_TAG " new color %x red %x green %x blue %x\n", color,
                  color_cache->xcolors[color].red, color_cache->xcolors[color].green,
                  color_cache->xcolors[color].blue);
#endif
#endif

  color_cache->is_loaded[color] = 1;

  return &color_cache->xcolors[color];
}

/* --- global functions --- */

ui_color_cache_t *ui_acquire_color_cache(ui_display_t *disp, u_int8_t fade_ratio) {
  u_int count;
  ui_color_cache_t *color_cache;
  void *p;

  for (count = 0; count < num_of_caches; count++) {
    if (color_caches[count]->disp == disp && color_caches[count]->fade_ratio == fade_ratio) {
      color_caches[count]->ref_count++;

      return color_caches[count];
    }
  }

  if ((p = realloc(color_caches, sizeof(ui_color_cache_t*) * (num_of_caches + 1))) == NULL) {
    return NULL;
  }

  color_caches = p;

  if ((color_cache = calloc(1, sizeof(ui_color_cache_t))) == NULL) {
    return NULL;
  }

  color_cache->disp = disp;

  color_cache->fade_ratio = fade_ratio;

  if (!ui_load_rgb_xcolor(color_cache->disp, &color_cache->black, 0, 0, 0, 0xff)) {
    free(color_cache);

    return NULL;
  }

  color_cache->ref_count = 1;

  color_caches[num_of_caches++] = color_cache;

  return color_cache;
}

void ui_release_color_cache(ui_color_cache_t *color_cache) {
  u_int count;

  if (--color_cache->ref_count > 0) {
    return;
  }

  for (count = 0; count < num_of_caches; count++) {
    if (color_caches[count] == color_cache) {
      color_caches[count] = color_caches[--num_of_caches];

      ui_color_cache_unload(color_cache);
      ui_unload_xcolor(color_cache->disp, &color_cache->black);

      free(color_cache);

      if (num_of_caches == 0) {
        free(color_caches);
        color_caches = NULL;
      }

      return;
    }
  }
}

void ui_color_cache_unload(ui_color_cache_t *color_cache) {
  vt_color_t color;

  for (color = 0; color < sizeof(color_cache->xcolors) / sizeof(color_cache->xcolors[0]); color++) {
    if (color_cache->is_loaded[color]) {
      ui_unload_xcolor(color_cache->disp, &color_cache->xcolors[color]);
      color_cache->is_loaded[color] = 0;
    }
  }

  if (color_cache->cache_256ext && --color_cache->cache_256ext->ref_count == 0) {
    ui_color_cache_256ext_t *cache_256ext;

    cache_256ext = color_cache->cache_256ext;
    for (color = 0; color < sizeof(cache_256ext->xcolors) / sizeof(cache_256ext->xcolors[0]);
         color++) {
      if (cache_256ext->is_loaded[color]) {
        ui_unload_xcolor(color_cache->disp, &cache_256ext->xcolors[color]);
        cache_256ext->is_loaded[color] = 0;
      }
    }

    free(cache_256ext);
    color_cache->cache_256ext = NULL;
  }
}

void ui_color_cache_unload_all(void) {
  u_int count;

  for (count = 0; count < num_of_caches; count++) {
    ui_color_cache_unload(color_caches[count]);
  }
}

/* Not cached */
int ui_load_xcolor(ui_color_cache_t *color_cache, ui_color_t *xcolor, char *name) {
  if (!ui_load_named_xcolor(color_cache->disp, xcolor, name)) {
    return 0;
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " new color %x\n", xcolor->pixel);
#endif

  if (color_cache->fade_ratio < 100) {
    if (!ui_xcolor_fade(color_cache->disp, xcolor, color_cache->fade_ratio)) {
      return 0;
    }
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " new color %s %x\n", name, xcolor->pixel);
#endif

  return 1;
}

/* Always return non-null value. */
ui_color_t *ui_get_cached_xcolor(ui_color_cache_t *color_cache, vt_color_t color) {
  ui_color_t *xcolor;

  if ((xcolor = get_cached_vtsys_xcolor(color_cache, color))) {
    return xcolor;
  }

  if ((xcolor = get_cached_256ext_xcolor(color_cache, color))) {
    return xcolor;
  }

  bl_msg_printf("Loading color 0x%x failed. Using black color instead.\n", color);

  return &color_cache->black;
}
