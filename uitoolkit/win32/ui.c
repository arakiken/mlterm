/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui.h"

#include <stdio.h>         /* sscanf */
#include <string.h>        /* strcmp */
#include <pobl/bl_types.h> /* size_t */
#include <pobl/bl_util.h>  /* BL_ARRAY_SIZE */

#if 0
#define SELF_TEST
#endif

#define TABLE_SIZE (BL_ARRAY_SIZE(keysym_table))

static struct {
  char *str;
  KeySym /* WORD */ ksym; /* 16bit */

} keysym_table[] = {
    /* Sort ascending by ASCII code. */
    {"BackSpace", XK_BackSpace},
    {"Delete", XK_Delete},
    {"Down", XK_Down},
    {"End", XK_End},
    {"Escape", XK_Escape},
    {"F1", XK_F1},
    {"F10", XK_F10},
    {"F11", XK_F11},
    {"F12", XK_F12},
    {"F13", XK_F13},
    {"F14", XK_F14},
    {"F15", XK_F15},
    {"F16", XK_F16},
    {"F17", XK_F17},
    {"F18", XK_F18},
    {"F19", XK_F19},
    {"F2", XK_F2},
    {"F20", XK_F20},
    {"F21", XK_F21},
    {"F22", XK_F22},
    {"F23", XK_F23},
    {"F24", XK_F24},
    {"F3", XK_F3},
    {"F4", XK_F4},
    {"F5", XK_F5},
    {"F6", XK_F6},
    {"F7", XK_F7},
    {"F8", XK_F8},
    {"F9", XK_F9},
    {"Henkan_Mode", XK_Henkan_Mode},
    {"Home", XK_Home},
    {"Insert", XK_Insert},
    {"Left", XK_Left},
    {"Muhenkan", XK_Muhenkan},
    {"Next", XK_Next},
    {"Prior", XK_Prior},
    {"Return", XK_Return},
    {"Right", XK_Right},
    {"Tab", XK_Tab},
    {"Up", XK_Up},
    {"Zenkaku_Hankaku", XK_Zenkaku_Hankaku},
    {"ampersand", '&'},
    {"apostrophe", '\''},
    {"asterisk", '*'},
    {"at", '@'},
    {"colon", ':'},
    {"comma", ','},
    {"dollar", '$'},
    {"equal", '='},
    {"exclam", '!'},
    {"greater", '>'},
    {"less", '<'},
    {"minus", '-'},
    {"numbersign", '#'},
    {"parenleft", '('},
    {"parenright", ')'},
    {"percent", '%'},
    {"period", '.'},
    {"plus", '+'},
    {"question", '?'},
    {"quotedbl", '\"'},
    {"semicolon", ';'},
    {"slash", '/'},
    {"space", ' '},
};

/* --- global functions --- */

int XParseGeometry(char *str, int *xpos, int *ypos, unsigned int *width, unsigned int *height) {
  int x, y, w, h;

  if (sscanf(str, "%ux%u+%d+%d", &w, &h, &x, &y) == 4) {
    *xpos = x;
    *ypos = y;
    *width = w;
    *height = h;

    return XValue | YValue | WidthValue | HeightValue;
  } else if (sscanf(str, "%ux%u", &w, &h) == 2) {
    *width = w;
    *height = h;

    return WidthValue | HeightValue;
  } else if (sscanf(str, "+%d+%d", &x, &y) == 2) {
    *xpos = x;
    *ypos = y;

    return XValue | YValue;
  } else {
    return 0;
  }
}

KeySym XStringToKeysym(const char *str) {
#ifdef SELF_TEST
  int debug_count = 0;
#endif
  size_t prev_idx;
  size_t idx;
  size_t distance;

  if (str[1] == '\0') {
    char c = *str;

    if ('0' <= c && c <= '9') {
      return c;
    } else {
      c |= 0x20; /* lower case */

      if ('a' <= c && c <= 'z') {
        return *str;
      }
    }
  }

  prev_idx = -1;

  /* +1 => roundup */
  idx = (TABLE_SIZE + 1) / 2;

  /* idx + distance == TABLE_SIZE - 1 */
  distance = TABLE_SIZE - idx - 1;

  while (1) {
    int cmp;

    if ((cmp = strcmp(keysym_table[idx].str, str)) == 0) {
#ifdef SELF_TEST
      fprintf(stderr, "%.2d/%.2d:", debug_count, TABLE_SIZE);
#endif

      return keysym_table[idx].ksym;
    } else {
      size_t next_idx;

#ifdef SELF_TEST
      debug_count++;
#endif

      /* +1 => roundup */
      if ((distance = (distance + 1) / 2) == 0) {
        break;
      }

      if (cmp > 0) {
        if (idx < distance) {
          /* idx - distance == 0 */
          distance = idx;
        }

        next_idx = idx - distance;
      } else /* if( cmp < 0) */
      {
        if (idx + distance >= TABLE_SIZE) {
          /* idx + distance == TABLE_SIZE - 1 */
          distance = TABLE_SIZE - idx - 1;
        }

        next_idx = idx + distance;
      }

      if (next_idx == prev_idx) {
        break;
      }

      prev_idx = idx;
      idx = next_idx;
    }
  }

  return NoSymbol;
}

#ifdef BL_DEBUG

#include <assert.h>
#include <pobl/bl_debug.h>

void TEST_xstringtokeysym(void) {
  size_t count;
  struct {
    char *str;
    KeySym ksym;
  } array[] =
  {
    { "a", 'a' },
    { "hoge", NoSymbol },
    { "zzzz", NoSymbol }
  };

  for (count = 0; count < TABLE_SIZE; count++) {
    assert(XStringToKeysym(keysym_table[count].str) == keysym_table[count].ksym);
  }

  for (count = 0; count < BL_ARRAY_SIZE(array); count++) {
    assert(XStringToKeysym(array[count].str) == array[count].ksym);
  }

  bl_msg_printf("PASS XStringToKeysym() test.\n");
}
#endif
