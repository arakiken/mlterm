/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_FONT_CONFIG_H__
#define __UI_FONT_CONFIG_H__

#include <pobl/bl_map.h>
#include <pobl/bl_types.h>

#include "ui_font.h"

BL_MAP_TYPEDEF(ui_font_name, vt_font_t, char *);

typedef struct ui_font_config {
  /* Public(readonly) */
  ui_type_engine_t type_engine;
  ui_font_present_t font_present;

  /*
   * Private
   * font_configs whose difference is only FONT_AA share these members.
   */
  BL_MAP(ui_font_name) font_name_table;

  u_int ref_count;

} ui_font_config_t;

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
int ui_use_aafont(void);

int ui_is_using_aafont(void);
#endif

ui_font_config_t *ui_acquire_font_config(ui_type_engine_t type_engine,
                                         ui_font_present_t font_present);

int ui_release_font_config(ui_font_config_t *font_config);

ui_font_config_t *ui_font_config_new(ui_type_engine_t type_engine, ui_font_present_t font_present);

int ui_font_config_delete(ui_font_config_t *font_config);

int ui_customize_font_file(const char *file, char *key, char *value, int save);

char *ui_get_config_font_name(ui_font_config_t *font_config, u_int font_size, vt_font_t font);

char *ui_get_config_font_name2(const char *file, u_int font_size, char *font_cs);

char *ui_get_config_font_names_all(ui_font_config_t *font_config, u_int font_size);

char *ui_font_config_dump(ui_font_config_t *font_config);

char *ui_get_charset_name(ef_charset_t cs);

#endif
