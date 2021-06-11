/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_shortcut.h"

#include <stdio.h>       /* sscanf */
#include <string.h>      /* strchr/memcpy */
#include <pobl/bl_def.h> /* HAVE_WINDOWS_H */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_file.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

#ifndef CommandMask
#define CommandMask (0)
#endif

/*
 * !! Notice !!
 * Mod1Mask - Mod5Mask are not distinguished.
 */

typedef struct key_func_table {
  char *name;
  ui_key_func_t func;

} key_func_table_t;

/* --- static variables --- */

static char *key_file = "mlterm/key";

/*
 * Button*Mask is disabled until Button* is specified in ~/.mlterm/key to avoid
 * such a problem as
 * http://sourceforge.net/mailarchive/message.php?msg_id=30866232
 */
static int button_mask = 0;

/* --- static variables --- */

static key_func_table_t key_func_table[] = {
  { "IM_HOTKEY", IM_HOTKEY, },
  { "EXT_KBD", EXT_KBD, },
  { "OPEN_SCREEN", OPEN_SCREEN, },
  { "OPEN_PTY", OPEN_PTY, },
  { "NEXT_PTY", NEXT_PTY, },
  { "PREV_PTY", PREV_PTY, },
  { "VSPLIT_SCREEN", VSPLIT_SCREEN, },
  { "HSPLIT_SCREEN", HSPLIT_SCREEN, },
  { "NEXT_SCREEN", NEXT_SCREEN, },
  { "PREV_SCREEN", PREV_SCREEN, },
  { "CLOSE_SCREEN", CLOSE_SCREEN, },
  { "HEXPAND_SCREEN", HEXPAND_SCREEN, },
  { "VEXPAND_SCREEN", VEXPAND_SCREEN, },
  { "PAGE_UP", PAGE_UP, },
  { "SCROLL_UP", SCROLL_UP, },
  { "SCROLL_UP_TO_MARK", SCROLL_UP_TO_MARK, },
  { "SCROLL_DOWN_TO_MARK", SCROLL_DOWN_TO_MARK, },
  { "INSERT_SELECTION", INSERT_SELECTION, },
#ifdef USE_XLIB
  { "INSERT_CLIPBOARD", INSERT_CLIPBOARD, },
#endif
  { "RESET", RESET, },
  { "COPY_MODE", COPY_MODE, },
  { "SET_MARK", SET_MARK, },
  { "EXIT_PROGRAM", EXIT_PROGRAM, },

  /* obsoleted: alias of OPEN_SCREEN */
  { "NEW_PTY", OPEN_SCREEN, },
};

/* --- static functions --- */

static int read_conf(ui_shortcut_t *shortcut, char *filename) {
  bl_file_t *from;
  char *key;
  char *value;

  if (!(from = bl_file_open(filename, "r"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  while (bl_conf_io_read(from, &key, &value)) {
    /*
     * [shortcut key]=[operation]
     */
    ui_shortcut_parse(shortcut, key, value);
  }

  bl_file_close(from);

  return 1;
}

/* --- global functions --- */

void ui_shortcut_init(ui_shortcut_t *shortcut) {
  char *rcpath;

  ui_key_t default_key_map[] = {
    /* IM_HOTKEY */
    { 0, 0, 0, },

    /* EXT_KBD(obsolete) */
    { 0, 0, 0, },

#if defined(USE_QUARTZ) || (defined(USE_SDL2) && defined(__APPLE__))
    /* OPEN_SCREEN */
    { XK_F1, CommandMask, 1, },

    /* OPEN_PTY */
    { XK_F2, CommandMask, 1, },

    /* NEXT_PTY */
    { XK_F3, CommandMask, 1, },

    /* PREV_PTY */
    { XK_F4, CommandMask, 1, },
#else
    /* OPEN_SCREEN */
    { XK_F1, ControlMask, 1, },

    /* OPEN_PTY */
    { XK_F2, ControlMask, 1, },

    /* NEXT_PTY */
    { XK_F3, ControlMask, 1, },

    /* PREV_PTY */
    { XK_F4, ControlMask, 1, },
#endif

    /* HSPLIT_SCREEN */
    { XK_F1, ShiftMask, 1, },

    /* VSPLIT_SCREEN */
    { XK_F2, ShiftMask, 1, },

    /* NEXT_SCREEN */
    { XK_F3, ShiftMask, 1, },

    /* PREV_SCREEN */
    { XK_F4, ShiftMask, 1, },

    /* CLOSE_SCREEN */
    { XK_F5, ShiftMask, 1, },

    /* HEXPAND_SCREEN */
    { XK_F6, ShiftMask, 1, },

    /* VEXPAND_SCREEN */
    { XK_F7, ShiftMask, 1, },

    /* PAGE_UP */
    { XK_Prior, ShiftMask, 1, },

    /* SCROLL_UP */
    { XK_Up, ShiftMask, 1, },

    /* SCROLL_UP_TO_MARK */
    { XK_Up, ControlMask|ShiftMask, 1, },

    /* SCROLL_DOWN_TO_MARK */
    { XK_Down, ControlMask|ShiftMask, 1, },

    /* INSERT_SELECTION */
#if defined(USE_QUARTZ) || (defined(USE_SDL2) && defined(__APPLE__))
    { 'v', CommandMask, 1, },
#else
    { XK_Insert, ShiftMask, 1, },
#endif

#ifdef USE_XLIB
    /* INSERT_CLIPBOARD */
    { 0, 0, 0, },
#endif

    /* RESET */
    { XK_Pause, 0, 1, },

    /* COPY_MODE */
    { XK_Return, ControlMask|ShiftMask, 1, },

    /* SET_MARK */
    { 'm', ControlMask|ShiftMask, 1, },

#ifdef DEBUG
    /* EXIT_PROGRAM(only for debug) */
    { XK_F1, ControlMask | ShiftMask, 1, },
#else
    { 0, 0, 0, },
#endif
  };

  memcpy(&shortcut->map, &default_key_map, sizeof(default_key_map));

  if ((shortcut->str_map = malloc(2 * sizeof(ui_str_key_t)))) {
    shortcut->str_map_size = 2;

    shortcut->str_map[0].ksym = 0;
    shortcut->str_map[0].state = Button1Mask | ControlMask;

    shortcut->str_map[0].str =
#ifdef HAVE_WINDOWS_H
      strdup("menu:mlterm-menu.exe");
#else
      strdup("menu:mlterm-menu");
#endif

    shortcut->str_map[1].ksym = 0;
    shortcut->str_map[1].state = Button3Mask | ControlMask;
    shortcut->str_map[1].str =
#ifdef HAVE_WINDOWS_H
      strdup("menu:mlconfig.exe");
#else
      strdup("menu:mlconfig");
#endif
    button_mask |= (Button1Mask | Button3Mask);
  } else {
    shortcut->str_map_size = 0;
  }

  if ((rcpath = bl_get_sys_rc_path(key_file))) {
    read_conf(shortcut, rcpath);
    free(rcpath);
  }

  if ((rcpath = bl_get_user_rc_path(key_file))) {
    read_conf(shortcut, rcpath);
    free(rcpath);
  }
}

void ui_shortcut_final(ui_shortcut_t *shortcut) {
  u_int count;

  for (count = 0; count < shortcut->str_map_size; count++) {
    free(shortcut->str_map[count].str);
  }

  free(shortcut->str_map);
}

int ui_shortcut_match(ui_shortcut_t *shortcut, ui_key_func_t func, KeySym ksym, u_int state) {
  if (shortcut->map[func].is_used == 0) {
    return 0;
  }

  if ('A' <= ksym && ksym <= 'Z') {
    ksym += 0x20;
  }

  /* ingoring except these masks */
  state &= (ModMask | ControlMask | ShiftMask | CommandMask | button_mask);

  if (state & button_mask) {
    state &= ~Mod2Mask;  /* XXX NumLock */
  }

  if (shortcut->map[func].ksym == ksym &&
      shortcut->map[func].state ==
          (state |
           ((state & ModMask) && (shortcut->map[func].state & ModMask) == ModMask ? ModMask : 0))) {
    return 1;
  } else {
    return 0;
  }
}

char *ui_shortcut_str(ui_shortcut_t *shortcut, KeySym ksym, u_int state) {
  u_int count;

  if ('A' <= ksym && ksym <= 'Z') {
    ksym += 0x20;
  }

  /* ingoring except these masks */
  state &= (ModMask | ControlMask | ShiftMask | CommandMask | button_mask);

  if (state & button_mask) {
    state &= ~Mod2Mask;  /* XXX NumLock */
  }

  for (count = 0; count < shortcut->str_map_size; count++) {
    if (shortcut->str_map[count].ksym == ksym &&
        shortcut->str_map[count].state ==
            (state |
             ((state & ModMask) && (shortcut->str_map[count].state & ModMask) == ModMask ? ModMask
                                                                                         : 0))) {
      return shortcut->str_map[count].str;
    }
  }

  return NULL;
}

int ui_shortcut_parse(ui_shortcut_t *shortcut, char *key, char *oper) {
  char *p;
  KeySym ksym;
  u_int state;
  int count;

  if (strcmp(key, "UNUSED") == 0) {
    goto replace_shortcut_map;
  }

  state = 0;

  while ((p = strchr(key, '+')) != NULL) {
    *(p++) = '\0';

    if (strcmp(key, "Control") == 0) {
      state |= ControlMask;
    } else if (strcmp(key, "Shift") == 0) {
      state |= ShiftMask;
    } else if (strcmp(key, "Mod") == 0 || strcmp(key, "Alt") == 0) {
      state |= ModMask;
    } else if (strncmp(key, "Mod", 3) == 0) {
      switch (key[3]) {
        case 0:
          state |= ModMask;
          break;
        case '1':
          state |= Mod1Mask;
          break;
        case '2':
          state |= Mod2Mask;
          break;
        case '3':
          state |= Mod3Mask;
          break;
        case '4':
          state |= Mod4Mask;
          break;
        case '5':
          state |= Mod5Mask;
          break;
#ifdef DEBUG
        default:
          bl_warn_printf(BL_DEBUG_TAG " unrecognized Mod mask(%s)\n", key);
          break;
#endif
      }
    } else if (strcmp(key, "Command") == 0) {
      state |= CommandMask;
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " unrecognized mask(%s)\n", key);
    }
#endif

    key = p;
  }

  if (strncmp(key, "Button", 6) == 0) {
    state |= (Button1Mask << (key[6] - '1'));
    ksym = 0;
  } else if ((ksym = XStringToKeysym(key)) != NoSymbol) {
    if ('A' <= ksym && ksym <= 'Z') {
      ksym += 0x20;
    }
  } else {
    return 0;
  }

  for (count = 0; count < BL_ARRAY_SIZE(key_func_table); count++) {
    ui_key_t *map_entry;

    map_entry = shortcut->map + key_func_table[count].func;
    if (map_entry->ksym == ksym && map_entry->state == state) {
      map_entry->is_used = 0;
      break;
    }
  }

  for (count = 0; count < shortcut->str_map_size; count++) {
    if (shortcut->str_map[count].ksym == ksym && shortcut->str_map[count].state == state) {
      free(shortcut->str_map[count].str);
      shortcut->str_map[count] = shortcut->str_map[--shortcut->str_map_size];
      break;
    }
  }

  if (*oper == '"') {
    char *str;
    char *p;
    ui_str_key_t *str_map;

    if (!(str = bl_str_unescape(++oper)) || !(p = strrchr(str, '\"')) ||
        !(str_map =
              realloc(shortcut->str_map, sizeof(ui_str_key_t) * (shortcut->str_map_size + 1)))) {
      free(str);

      return 0;
    }

    *p = '\0';
    str_map[shortcut->str_map_size].ksym = ksym;
    str_map[shortcut->str_map_size].state = state;
    str_map[shortcut->str_map_size].str = str;

    shortcut->str_map_size++;
    shortcut->str_map = str_map;
  } else {
  replace_shortcut_map:
    for (count = 0; count < BL_ARRAY_SIZE(key_func_table); count++) {
      if (strcmp(oper, key_func_table[count].name) == 0) {
        if (strcmp(key, "UNUSED") == 0) {
          shortcut->map[key_func_table[count].func].is_used = 0;

          return 1;
        } else {
          shortcut->map[key_func_table[count].func].is_used = 1;
          shortcut->map[key_func_table[count].func].ksym = ksym;
          shortcut->map[key_func_table[count].func].state = state;

          goto success;
        }
      }
    }

    return 0;
  }

success:
  if (state & ButtonMask) {
    int mask;

    for (mask = Button1Mask; mask <= Button7Mask; mask <<= 1) {
      if (state & mask) {
        button_mask |= mask;
        break;
      }
    }
  }

  return 1;
}
