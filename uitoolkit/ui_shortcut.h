/*
 *	$Id$
 */

/*
 * This manages short-cut keys of ui_screen key events.
 */

#ifndef __UI_SHORTCUT_H__
#define __UI_SHORTCUT_H__

#include "ui.h"
#include <pobl/bl_types.h>

typedef enum ui_key_func {
  IM_HOTKEY,
  EXT_KBD,
  OPEN_SCREEN,
  OPEN_PTY,
  NEXT_PTY,
  PREV_PTY,
  HSPLIT_SCREEN,
  VSPLIT_SCREEN,
  NEXT_SCREEN,
  PREV_SCREEN,
  CLOSE_SCREEN,
  HEXPAND_SCREEN,
  VEXPAND_SCREEN,
  PAGE_UP,
  PAGE_DOWN,
  SCROLL_UP,
  SCROLL_DOWN,
  INSERT_SELECTION,
  EXIT_PROGRAM,

  MAX_KEY_MAPS

} ui_key_func_t;

typedef struct ui_key {
  KeySym ksym;
  u_int state;
  int is_used;

} ui_key_t;

typedef struct ui_str_key {
  KeySym ksym;
  u_int state;
  char* str;

} ui_str_key_t;

typedef struct ui_shortcut {
  ui_key_t map[MAX_KEY_MAPS];
  ui_str_key_t* str_map;
  u_int str_map_size;

} ui_shortcut_t;

int ui_shortcut_init(ui_shortcut_t* shortcut);

int ui_shortcut_final(ui_shortcut_t* shortcut);

int ui_shortcut_match(ui_shortcut_t* shortcut, ui_key_func_t func, KeySym sym, u_int state);

char* ui_shortcut_str(ui_shortcut_t* shortcut, KeySym sym, u_int state);

int ui_shortcut_parse(ui_shortcut_t* shortcut, char* key, char* oper);

#endif
