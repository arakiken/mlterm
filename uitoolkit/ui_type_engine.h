/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_TYPE_ENGINE_H__
#define __UI_TYPE_ENGINE_H__

typedef enum ui_type_engine {
  TYPE_XCORE, /* Contains WIN32 native fonts. */
  TYPE_XFT,
  TYPE_CAIRO,

  TYPE_ENGINE_MAX

} ui_type_engine_t;

ui_type_engine_t ui_get_type_engine_by_name(char* name);

char* ui_get_type_engine_name(ui_type_engine_t mode);

#endif
