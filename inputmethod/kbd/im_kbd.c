/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * im_kbd.c - keyboard mapping input method for mlterm
 *
 *    based on x_kbd.c written by Araki Ken.
 *
 * Copyright (C) 2001, 2002, 2003 Araki Ken <arakiken@users.sf.net>
 * Copyright (C) 2004 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$Id$
 */

#include <stdio.h>          /* sprintf */
#include <pobl/bl_mem.h>    /* malloc/alloca/free */
#include <pobl/bl_str.h>    /* bl_snprintf */
#include <pobl/bl_locale.h> /* bl_get_locale */
#include <mef/ef_utf16_parser.h>
#include <vt_iscii.h>
#include <ui_im.h>

#include "../im_info.h"

#if 0
#define IM_KBD_DEBUG 1
#endif

typedef enum kbd_type {
  KBD_TYPE_UNKNOWN,
  KBD_TYPE_ARABIC,
  KBD_TYPE_HEBREW,
  KBD_TYPE_ISCII,

  KBD_TYPE_MAX

} kbd_type_t;

typedef enum kbd_mode {
  KBD_MODE_ASCII = 0,

  /* arabic or hebrew */
  KBD_MODE_ON,

  /* iscii */
  KBD_MODE_ISCII_INSCRIPT,
  KBD_MODE_ISCII_PHONETIC,

  KBD_MODE_MAX

} kbd_mode_t;

typedef struct im_kbd {
  ui_im_t im;

  kbd_type_t type;
  kbd_mode_t mode;

  vt_isciikey_state_t isciikey_state;

  ef_parser_t *parser;

} im_kbd_t;

/* --- static variables --- */

static int ref_count = 0;
static int initialized = 0;
static ef_parser_t *parser_ascii = NULL;

/* mlterm internal symbols */
static ui_im_export_syms_t *syms = NULL;

static u_char *arabic_conv_tbl[] = {
    "\x06\x37",         /* ' */
    NULL,               /* ( */
    NULL,               /* ) */
    NULL,               /* * */
    NULL,               /* + */
    "\x06\x48",         /* , */
    NULL,               /* - */
    "\x06\x32",         /* . */
    "\x06\x38",         /* / */
    "\x06\x60",         /* 0 */
    "\x06\x61",         /* 1 */
    "\x06\x62",         /* 2 */
    "\x06\x63",         /* 3 */
    "\x06\x64",         /* 4 */
    "\x06\x65",         /* 5 */
    "\x06\x66",         /* 6 */
    "\x06\x67",         /* 7 */
    "\x06\x68",         /* 8 */
    "\x06\x69",         /* 9 */
    NULL,               /* : */
    "\x06\x43",         /* ; */
    "\x00\x2c",         /* < */
    NULL,               /* = */
    "\x00\x2e",         /* > */
    "\x06\x1f",         /* ? */
    NULL,               /* @ */
    "\x06\x50",         /* A */
    "\x06\x44\x06\x22", /* B */
    "\x00\x7b",         /* C */
    "\x00\x5b",         /* D */
    "\x06\x4f",         /* E */
    "\x00\x5d",         /* F */
    "\x06\x44\x06\x23", /* G */
    "\x06\x23",         /* H */
    "\x00\xf7",         /* I */
    "\x06\x40",         /* J */
    "\x06\x0c",         /* K */
    "\x00\x2f",         /* L */
    "\x00\x27",         /* M */
    "\x06\x22",         /* N */
    "\x00\xd7",         /* O */
    "\x06\x1b",         /* P */
    "\x06\x4e",         /* Q */
    "\x06\x4c",         /* R */
    "\x06\x4d",         /* S */
    "\x06\x44\x06\x25", /* T */
    "\x00\x60",         /* U */
    "\x00\x7d",         /* V */
    "\x06\x4b",         /* W */
    "\x06\x52",         /* X */
    "\x06\x25",         /* Y */
    "\x00\x7e",         /* Z */
    "\x06\x2c",         /* [ */
    NULL,               /* \ */
    "\x06\x2f",         /* ] */
    NULL,               /* ^ */
    NULL,               /* _ */
    "\x06\x30",         /* ` */
    "\x06\x34",         /* a */
    "\x06\x44\x06\x27", /* b */
    "\x06\x24",         /* c */
    "\x06\x4a",         /* d */
    "\x06\x2b",         /* e */
    "\x06\x28",         /* f */
    "\x06\x44",         /* g */
    "\x06\x27",         /* h */
    "\x06\x47",         /* i */
    "\x06\x2a",         /* j */
    "\x06\x46",         /* k */
    "\x06\x45",         /* l */
    "\x06\x29",         /* m */
    "\x06\x49",         /* n */
    "\x06\x2e",         /* o */
    "\x06\x2d",         /* p */
    "\x06\x36",         /* q */
    "\x06\x42",         /* r */
    "\x06\x33",         /* s */
    "\x06\x41",         /* t */
    "\x06\x39",         /* u */
    "\x06\x31",         /* v */
    "\x06\x35",         /* w */
    "\x06\x21",         /* x */
    "\x06\x3a",         /* y */
    "\x06\x26",         /* z */
    "\x00\x3c",         /* { */
    NULL,               /* | */
    "\x00\x3e",         /* } */
    "\x06\x51",         /* ~ */

};

static u_char *hebrew_conv_tbl[] = {
    "\x00\x3b", /* ' */
    NULL,       /* ( */
    NULL,       /* ) */
    NULL,       /* * */
    NULL,       /* + */
    "\x05\xea", /* , */
    NULL,       /* - */
    "\x05\xe5", /* . */
    "\x00\x2e", /* / */
    NULL,       /* 0 */
    NULL,       /* 1 */
    NULL,       /* 2 */
    NULL,       /* 3 */
    NULL,       /* 4 */
    NULL,       /* 5 */
    NULL,       /* 6 */
    NULL,       /* 7 */
    NULL,       /* 8 */
    NULL,       /* 9 */
    NULL,       /* : */
    "\x05\xe3", /* ; */
    NULL,       /* < */
    NULL,       /* = */
    NULL,       /* > */
    NULL,       /* ? */
    NULL,       /* @ */
    NULL,       /* A */
    NULL,       /* B */
    NULL,       /* C */
    NULL,       /* D */
    NULL,       /* E */
    NULL,       /* F */
    NULL,       /* G */
    NULL,       /* H */
    NULL,       /* I */
    NULL,       /* J */
    NULL,       /* K */
    NULL,       /* L */
    NULL,       /* M */
    NULL,       /* N */
    NULL,       /* O */
    NULL,       /* P */
    NULL,       /* Q */
    NULL,       /* R */
    NULL,       /* S */
    NULL,       /* T */
    NULL,       /* U */
    NULL,       /* V */
    NULL,       /* W */
    NULL,       /* X */
    NULL,       /* Y */
    NULL,       /* Z */
    NULL,       /* [ */
    NULL,       /* \ */
    NULL,       /* ] */
    NULL,       /* ^ */
    NULL,       /* _ */
    "\x00\x3b", /* ` */
    "\x05\xe9", /* a */
    "\x05\xe0", /* b */
    "\x05\xd1", /* c */
    "\x05\xd2", /* d */
    "\x05\xe7", /* e */
    "\x05\xdb", /* f */
    "\x05\xe2", /* g */
    "\x05\xd9", /* h */
    "\x05\xdf", /* i */
    "\x05\xd7", /* j */
    "\x05\xdc", /* k */
    "\x05\xda", /* l */
    "\x05\xe6", /* m */
    "\x05\xde", /* n */
    "\x05\xdd", /* o */
    "\x05\xe4", /* p */
    "\x00\x2f", /* q */
    "\x05\xe8", /* r */
    "\x05\xd3", /* s */
    "\x05\xd0", /* t */
    "\x05\xd5", /* u */
    "\x05\xd4", /* v */
    "\x00\x27", /* w */
    "\x05\xe1", /* x */
    "\x05\xd8", /* y */
    "\x05\xd6", /* z */
    NULL,       /* { */
    NULL,       /* | */
    NULL,       /* } */
    NULL,       /* ~ */

};

/* --- static functions --- */

static kbd_type_t find_kbd_type(char *locale) {
  if (locale) {
    if (strncmp(locale, "ar", 2) == 0) {
      return KBD_TYPE_ARABIC;
    }

    if (strncmp(locale, "he", 2) == 0) {
      return KBD_TYPE_HEBREW;
    }
  }

  return KBD_TYPE_UNKNOWN;
}

/*
 * methods of ui_im_t
 */

static void destroy(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->isciikey_state) {
    (*syms->vt_isciikey_state_destroy)(kbd->isciikey_state);
  }

  if (kbd->parser) {
    (*kbd->parser->destroy)(kbd->parser);
  }

  ref_count--;

#ifdef IM_KBD_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was destroyed. ref_count: %d\n", ref_count);
#endif

  free(kbd);

  if (initialized && ref_count == 0) {
    (*parser_ascii->destroy)(parser_ascii);
    parser_ascii = NULL;

    initialized = 0;
  }
}

static int key_event_arabic_hebrew(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_kbd_t *kbd;
  size_t len;
  u_char *c;

  kbd = (im_kbd_t*)im;

  if (kbd->mode != KBD_MODE_ON) {
    return 1;
  }

  if (event->state & ~ShiftMask) {
    return 1;
  }

  if (key_char < 0x27 || key_char > 0x7e) {
    return 1;
  }

  if (kbd->type == KBD_TYPE_ARABIC) {
    if (!(c = arabic_conv_tbl[key_char - 0x27])) {
      return 1;
    }
  } else { /* kbd->type == KBD_TYPE_HEBREW */
    if (!(c = hebrew_conv_tbl[key_char - 0x27])) {
      return 1;
    }
  }

  if (*c == 0x0) {
    /* "\x00\xNN" */
    len = 1 + strlen(c + 1);
  } else {
    len = strlen(c);
  }

  (*kbd->im.listener->write_to_term)(kbd->im.listener->self, c, len, kbd->parser);

  return 0;
}

static int key_event_iscii(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_kbd_t *kbd;
  u_char buf[512];
  size_t len;

  kbd = (im_kbd_t*)im;

  if (kbd->mode == KBD_MODE_ASCII) {
    return 1;
  }

  if (event->state & ~ShiftMask) {
    return 1;
  }

  if (key_char < 0x21 || key_char > 0x7e) {
    return 1;
  }

  len = (*syms->vt_convert_ascii_to_iscii)(kbd->isciikey_state, buf, sizeof(buf), &key_char, 1);

  (*kbd->im.listener->write_to_term)(kbd->im.listener->self, buf, len, kbd->parser);

  return 0;
}

static int switch_mode(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->type == KBD_TYPE_UNKNOWN) {
    return 0;
  }

  if (kbd->type == KBD_TYPE_ARABIC || kbd->type == KBD_TYPE_HEBREW) {
    if (kbd->mode == KBD_MODE_ASCII) {
      kbd->mode = KBD_MODE_ON;
    } else {
      kbd->mode = KBD_MODE_ASCII;
    }
  } else /* kbd->type == KBD_TYPE_ISCII */
  {
    if (kbd->isciikey_state) {
      (*syms->vt_isciikey_state_destroy)(kbd->isciikey_state);
      kbd->isciikey_state = NULL;
    }

    /* Inscript => Phonetic => US ASCII => Inscript ... */

    if (kbd->mode == KBD_MODE_ASCII) {
      kbd->isciikey_state = (*syms->vt_isciikey_state_new)(1);
      kbd->mode = KBD_MODE_ISCII_INSCRIPT;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to inscript.\n");
#endif
    } else if (kbd->mode == KBD_MODE_ISCII_INSCRIPT) {
      kbd->isciikey_state = (*syms->vt_isciikey_state_new)(0);
      kbd->mode = KBD_MODE_ISCII_PHONETIC;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to phonetic.\n");
#endif
    } else {
      kbd->mode = KBD_MODE_ASCII;

#ifdef IM_KBD_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " switched to ascii.\n");
#endif
    }

    if ((kbd->mode == KBD_MODE_ISCII_INSCRIPT || kbd->mode == KBD_MODE_ISCII_PHONETIC) &&
        (kbd->isciikey_state == NULL)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " vt_isciikey_state_new() failed.\n");
#endif
      kbd->mode = KBD_MODE_ASCII;
    }
  }

  if (kbd->mode == KBD_MODE_ASCII) {
    if (kbd->im.stat_screen) {
      (*kbd->im.stat_screen->destroy)(kbd->im.stat_screen);
      kbd->im.stat_screen = NULL;
    }
  } else {
    int x;
    int y;

    (*kbd->im.listener->get_spot)(kbd->im.listener->self, NULL, 0, &x, &y);

    if (kbd->im.stat_screen == NULL) {
      if (!(kbd->im.stat_screen = (*syms->ui_im_status_screen_new)(
                kbd->im.disp, kbd->im.font_man, kbd->im.color_man, kbd->im.vtparser,
                (*kbd->im.listener->is_vertical)(kbd->im.listener->self),
                (*kbd->im.listener->get_line_height)(kbd->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_satus_screen_new() failed.\n");
#endif

        return 0;
      }
    }

    switch (kbd->mode) {
      case KBD_MODE_ON:
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii,
                                    kbd->type == KBD_TYPE_ARABIC ? "Arabic" : "Hebrew");
        break;
      case KBD_MODE_ISCII_INSCRIPT:
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii, "ISCII:inscript");
        break;
      case KBD_MODE_ISCII_PHONETIC:
        (*kbd->im.stat_screen->set)(kbd->im.stat_screen, parser_ascii, "ISCII:phonetic");
        break;
      default:
        break;
    }
  }

  return 1;
}

static int is_active(ui_im_t *im) { return (((im_kbd_t*)im)->mode != KBD_MODE_ASCII); }

static void focused(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->im.stat_screen) {
    (*kbd->im.stat_screen->show)(kbd->im.stat_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_kbd_t *kbd;

  kbd = (im_kbd_t*)im;

  if (kbd->im.stat_screen) {
    (*kbd->im.stat_screen->hide)(kbd->im.stat_screen);
  }
}

/* --- global functions --- */

ui_im_t *im_kbd_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                    ui_im_export_syms_t *export_syms, char *opt, /* arabic/hebrew/iscii */
                    u_int mod_ignore_mask) {
  im_kbd_t *kbd;
  kbd_type_t type;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  if (opt && strcmp(opt, "arabic") == 0) {
    type = KBD_TYPE_ARABIC;
  } else if (opt && strcmp(opt, "hebrew") == 0) {
    type = KBD_TYPE_HEBREW;
  } else if (opt && strncmp(opt, "iscii", 5) == 0) {
    type = KBD_TYPE_ISCII;
  } else {
    type = find_kbd_type(bl_get_locale());

    if (type == KBD_TYPE_UNKNOWN) {
      if (IS_ISCII_ENCODING(term_encoding)) {
        type = KBD_TYPE_ISCII;
      } else {
        return NULL;
      }
    }
  }

  if (!initialized) {
    syms = export_syms;

    if (!(parser_ascii = (*syms->vt_char_encoding_parser_new)(VT_ISO8859_1))) {
      return NULL;
    }

    initialized = 1;
  }

  kbd = NULL;

  if (!(kbd = malloc(sizeof(im_kbd_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  kbd->type = type;
  kbd->mode = KBD_MODE_ASCII;
  kbd->isciikey_state = NULL;
  kbd->parser = NULL;

  if (kbd->type == KBD_TYPE_ARABIC || kbd->type == KBD_TYPE_HEBREW) {
    if (!(kbd->parser = ef_utf16_parser_new())) {
      goto error;
    }
  } else /* if( kbd->type == KBD_TYPE_ISCII */
  {
    vt_char_encoding_t iscii_encoding;

    if (IS_ISCII_ENCODING(term_encoding)) {
      iscii_encoding = term_encoding;
    } else if (!opt ||
               (iscii_encoding = (*syms->vt_get_char_encoding)(opt)) == VT_UNKNOWN_ENCODING) {
      iscii_encoding = VT_ISCII_HINDI;
    }

    if (!(kbd->parser = (*syms->vt_char_encoding_parser_new)(iscii_encoding))) {
      goto error;
    }
  }

  /*
   * set methods of ui_im_t
   */
  kbd->im.destroy = destroy;
  kbd->im.key_event = (kbd->type == KBD_TYPE_ISCII) ? key_event_iscii : key_event_arabic_hebrew;
  kbd->im.switch_mode = switch_mode;
  kbd->im.is_active = is_active;
  kbd->im.focused = focused;
  kbd->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_KBD_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)kbd;

error:
  if (kbd) {
    if (kbd->parser) {
      (*kbd->parser->destroy)(kbd->parser);
    }

    free(kbd);
  }

  if (initialized && ref_count) {
    (*parser_ascii->destroy)(parser_ascii);
    parser_ascii = NULL;

    initialized = 0;
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_kbd_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if (!(result = malloc(sizeof(im_info_t)))) {
    return NULL;
  }

  result->num_args = 13;

  if (!(result->args = malloc(sizeof(char*) * result->num_args))) {
    free(result);
    return NULL;
  }

  if (!(result->readable_args = malloc(sizeof(char*) * result->num_args))) {
    free(result->args);
    free(result);
    return NULL;
  }

  switch (find_kbd_type(locale)) {
    case KBD_TYPE_ARABIC:
      result->readable_args[0] = strdup("Arabic");
      break;
    case KBD_TYPE_HEBREW:
      result->readable_args[0] = strdup("Hebrew");
      break;
    /* case KBD_TYPE_UNKNOWN: */
    default:
      if (strncmp(encoding, "ISCII", 5) == 0) {
        result->readable_args[0] = malloc(6 /* "Indic " */ + 2 /* () */ + strlen(encoding + 5) + 1);
        sprintf(result->readable_args[0], "Indic (%s)", encoding + 5);
      } else {
        result->readable_args[0] = strdup("unknown");
      }
      break;
  }

  result->readable_args[1] = strdup("Arabic");
  result->readable_args[2] = strdup("Hebrew");
  result->readable_args[3] = strdup("Indic (ASSAMESE)");
  result->readable_args[4] = strdup("Indic (BENGALI)");
  result->readable_args[5] = strdup("Indic (GUJARATI)");
  result->readable_args[6] = strdup("Indic (HINDI)");
  result->readable_args[7] = strdup("Indic (KANNADA)");
  result->readable_args[8] = strdup("Indic (MALAYALAM)");
  result->readable_args[9] = strdup("Indic (ORIYA)");
  result->readable_args[10] = strdup("Indic (PUNJABI)");
  result->readable_args[11] = strdup("Indic (TAMIL)");
  result->readable_args[12] = strdup("Indic (TELUGU)");

  result->args[0] = strdup("");
  result->args[1] = strdup("arabic");
  result->args[2] = strdup("hebrew");
  result->args[3] = strdup("isciiassamese");
  result->args[4] = strdup("isciibengali");
  result->args[5] = strdup("isciigujarati");
  result->args[6] = strdup("isciihindi");
  result->args[7] = strdup("isciikannada");
  result->args[8] = strdup("isciimalayalam");
  result->args[9] = strdup("isciioriya");
  result->args[10] = strdup("isciipunjabi");
  result->args[11] = strdup("isciitamil");
  result->args[12] = strdup("isciitelugu");

  result->id = strdup("kbd");
  result->name = strdup("keyboard");

  return result;
}
