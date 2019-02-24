/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <canna/jrkanji.h>

#include <pobl/bl_mem.h> /* malloc/alloca/free */
#include <pobl/bl_str.h> /* bl_str_alloca_dup bl_str_sep bl_snprintf*/

#include <ui_im.h>

#include "../im_common.h"
#include "../im_info.h"

#if 0
#define IM_CANNA_DEBUG 1
#endif

/* When canna encoding is the same as terminal, conv is NULL. */
#define NEED_TO_CONV(canna) ((canna)->conv)

typedef struct im_canna {
  /* input method common object */
  ui_im_t im;

  char buf[1024];
  jrKanjiStatus key_status;
  int is_enabled;
  int is_selecting_cand;
  char *mode;

  vt_char_encoding_t term_encoding;

  char *encoding_name; /* encoding of conversion engine */

  /* conv is NULL if term_encoding == canna encoding */
  ef_parser_t *parser_term; /* for term encoding */
  ef_conv_t *conv;          /* for term encoding */

} im_canna_t;

/* --- static variables --- */

static int ref_count = 0;
static ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
static ef_parser_t *parser_eucjp = NULL;

/* --- static functions --- */

static void change_mode(im_canna_t *canna, int mode) {
  jrKanjiStatusWithValue ksv;

  ksv.ks = &canna->key_status;
  ksv.buffer = canna->buf;
  ksv.bytes_buffer = sizeof(canna->buf);
  ksv.val = mode;

  jrKanjiControl(0, KC_CHANGEMODE, &ksv);
}

static void preedit(im_canna_t *canna, char *preedit,             /* eucjp(null terminated) */
                    int rev_pos, int rev_len, char *candidateword /* eucjp(null terminated) */
                    ) {
  int x;
  int y;
  ef_char_t ch;
  vt_char_t *p;
  u_int num_chars;
  size_t preedit_len;

  if (preedit == NULL) {
    goto candidate;
  } else if ((preedit_len = strlen(preedit)) == 0) {
    if (canna->im.preedit.filled_len > 0) {
      /* Stop preediting. */
      canna->im.preedit.filled_len = 0;
    }
  } else {
    int cols = 0;
    u_char *tmp = NULL;

    canna->im.preedit.cursor_offset = -1;
    num_chars = 0;
    (*parser_eucjp->init)(parser_eucjp);
    (*parser_eucjp->set_str)(parser_eucjp, preedit, preedit_len);
    while ((*parser_eucjp->next_char)(parser_eucjp, &ch)) {
      num_chars++;

      if (cols >= 0) {
        /* eucjp */
        if (IS_FULLWIDTH_CS(ch.cs)) {
          cols += 2;
        } else if (ch.cs == JISX0201_KATA) {
          cols += 3;
        } else {
          cols++;
        }

        if (canna->im.preedit.cursor_offset == -1 && cols > rev_pos) {
          canna->im.preedit.cursor_offset = num_chars - 1;
        }

        if (cols >= rev_pos + rev_len) {
          if (rev_len == 0) {
            canna->im.preedit.cursor_offset = num_chars;
          } else {
            rev_len = num_chars - canna->im.preedit.cursor_offset;
          }

          cols = -1;
        }
      }
    }

    if ((p = realloc(canna->im.preedit.chars, sizeof(vt_char_t) * num_chars)) == NULL) {
      return;
    }

    if (NEED_TO_CONV(canna)) {
      (*parser_eucjp->init)(parser_eucjp);
      if (im_convert_encoding(parser_eucjp, canna->conv, preedit, &tmp, preedit_len)) {
        preedit = tmp;
        preedit_len = strlen(preedit);
      }
    }

    (*syms->vt_str_init)(canna->im.preedit.chars = p,
                         canna->im.preedit.num_chars = num_chars);
    canna->im.preedit.filled_len = 0;

    (*canna->parser_term->init)(canna->parser_term);
    (*canna->parser_term->set_str)(canna->parser_term, preedit, preedit_len);

    while ((*canna->parser_term->next_char)(canna->parser_term, &ch)) {
      int is_fullwidth;
      int is_comb;

      if ((*syms->vt_convert_to_internal_ch)(canna->im.vtparser, &ch) <= 0) {
        continue;
      }

      if (ch.property & EF_FULLWIDTH) {
        is_fullwidth = 1;
      } else if (ch.property & EF_AWIDTH) {
        /* TODO: check col_size_of_width_a */
        is_fullwidth = 1;
      } else {
        is_fullwidth = IS_FULLWIDTH_CS(ch.cs);
      }

      if (ch.property & EF_COMBINING) {
        is_comb = 1;

        if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb,
                                     VT_FG_COLOR, VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0)) {
          continue;
        }

        /*
         * if combining failed , char is normally appended.
         */
      } else {
        is_comb = 0;
      }

      if (canna->im.preedit.cursor_offset <= canna->im.preedit.filled_len &&
          canna->im.preedit.filled_len < canna->im.preedit.cursor_offset + rev_len) {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_BG_COLOR,
                             VT_FG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      } else {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_FG_COLOR,
                             VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      }

      p++;
      canna->im.preedit.filled_len++;
    }

    if (tmp) {
      free(tmp);
    }
  }

  (*canna->im.listener->draw_preedit_str)(canna->im.listener->self, canna->im.preedit.chars,
                                          canna->im.preedit.filled_len,
                                          canna->im.preedit.cursor_offset);

candidate:
  if (candidateword == NULL) {
    return;
  } else if (strlen(candidateword) == 0) {
    if (canna->im.stat_screen) {
      (*canna->im.stat_screen->destroy)(canna->im.stat_screen);
      canna->im.stat_screen = NULL;
    }
  } else {
    u_char *tmp = NULL;

    (*canna->im.listener->get_spot)(canna->im.listener->self, canna->im.preedit.chars,
                                    canna->im.preedit.segment_offset, &x, &y);

    if (canna->im.stat_screen == NULL) {
      if (!(canna->im.stat_screen = (*syms->ui_im_status_screen_new)(
                canna->im.disp, canna->im.font_man, canna->im.color_man, canna->im.vtparser,
                (*canna->im.listener->is_vertical)(canna->im.listener->self),
                (*canna->im.listener->get_line_height)(canna->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_status_screen_new() failed.\n");
#endif

        return;
      }
    } else {
      (*canna->im.stat_screen->show)(canna->im.stat_screen);
      (*canna->im.stat_screen->set_spot)(canna->im.stat_screen, x, y);
    }

    if (NEED_TO_CONV(canna)) {
      (*parser_eucjp->init)(parser_eucjp);
      if (im_convert_encoding(parser_eucjp, canna->conv, candidateword, &tmp,
                              strlen(candidateword))) {
        candidateword = tmp;
      }
    }

    (*canna->im.stat_screen->set)(canna->im.stat_screen, canna->parser_term, candidateword);

    if (tmp) {
      free(tmp);
    }
  }
}

static void commit(im_canna_t *canna, const char *str) {
  u_char conv_buf[256];
  size_t filled_len;
  size_t len;

#ifdef IM_CANNA_DEBUG
  bl_debug_printf(BL_DEBUG_TAG "str: %s\n", str);
#endif

  len = strlen(str);

  if (!NEED_TO_CONV(canna)) {
    (*canna->im.listener->write_to_term)(canna->im.listener->self, (u_char*)str, len);

    return;
  }

  (*parser_eucjp->init)(parser_eucjp);
  (*parser_eucjp->set_str)(parser_eucjp, (u_char*)str, len);

  (*canna->conv->init)(canna->conv);

  while (!parser_eucjp->is_eos) {
    filled_len = (*canna->conv->convert)(canna->conv, conv_buf, sizeof(conv_buf), parser_eucjp);

    if (filled_len == 0) {
      /* finished converting */
      break;
    }

    (*canna->im.listener->write_to_term)(canna->im.listener->self, conv_buf, filled_len);
  }
}

/*
 * methods of ui_im_t
 */

static void destroy(ui_im_t *im) {
  im_canna_t *canna;

  canna = (im_canna_t*)im;

  (*canna->parser_term->destroy)(canna->parser_term);

  if (canna->conv) {
    (*canna->conv->destroy)(canna->conv);
  }

  free(canna->mode);

  ref_count--;

#ifdef IM_CANNA_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was destroyed. ref_count: %d\n", ref_count);
#endif

  free(canna);

  if (ref_count == 0) {
    (*parser_eucjp->destroy)(parser_eucjp);
    parser_eucjp = NULL;
    jrKanjiControl(0, KC_FINALIZE, 0);
  }
}

static int switch_mode(ui_im_t *im) {
  im_canna_t *canna;

  canna = (im_canna_t*)im;

  change_mode(canna, canna->is_enabled ? CANNA_MODE_AlphaMode : CANNA_MODE_HenkanMode);

  if ((canna->is_enabled = (!canna->is_enabled))) {
    preedit(canna, NULL, 0, 0, canna->key_status.mode);
    jrKanjiControl(0, KC_SETWIDTH, 60);
  } else {
    preedit(canna, "", 0, 0, "");
  }

  return 1;
}

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_canna_t *canna;

  canna = (im_canna_t*)im;

  if (key_char == ' ' && (event->state & ShiftMask)) {
    switch_mode(im);

    return 0;
  } else if (canna->is_enabled) {
    int len;
    u_char *cand;

    if (canna->is_selecting_cand) {
      if (ksym == XK_Up) {
        ksym = XK_Left;
      } else if (ksym == XK_Down) {
        ksym = XK_Right;
      } else if (ksym == XK_Right) {
        ksym = XK_Down;
      } else if (ksym == XK_Left) {
        ksym = XK_Up;
      }
    }

#if 0
    if (key_char == ' ' && (event->state & ShiftMask)) {
      ksym = CANNA_KEY_Shift_Space;
    } else
#endif
        if (canna->im.preedit.filled_len == 0) {
      if (key_char == '\0') {
        return 1;
      }

      ksym = key_char;
    } else if (ksym == XK_Up) {
      if (event->state & ShiftMask) {
        ksym = CANNA_KEY_Shift_Up;
      } else if (event->state & ControlMask) {
        ksym = CANNA_KEY_Cntrl_Up;
      } else {
        ksym = CANNA_KEY_Up;
      }
    } else if (ksym == XK_Down) {
      if (event->state & ShiftMask) {
        ksym = CANNA_KEY_Shift_Down;
      } else if (event->state & ControlMask) {
        ksym = CANNA_KEY_Cntrl_Down;
      } else {
        ksym = CANNA_KEY_Down;
      }
    } else if (ksym == XK_Right) {
      if (event->state & ShiftMask) {
        ksym = CANNA_KEY_Shift_Right;
      } else if (event->state & ControlMask) {
        ksym = CANNA_KEY_Cntrl_Right;
      } else {
        ksym = CANNA_KEY_Right;
      }
    } else if (ksym == XK_Left) {
      if (event->state & ShiftMask) {
        ksym = CANNA_KEY_Shift_Left;
      } else if (event->state & ControlMask) {
        ksym = CANNA_KEY_Cntrl_Left;
      } else {
        ksym = CANNA_KEY_Left;
      }
    } else if (ksym == XK_Insert) {
      ksym = CANNA_KEY_Insert;
    } else if (ksym == XK_Prior) {
      ksym = CANNA_KEY_PageUp;
    } else if (ksym == XK_Next) {
      ksym = CANNA_KEY_PageDown;
    } else if (ksym == XK_Home) {
      ksym = CANNA_KEY_Home;
    } else if (ksym == XK_End) {
      ksym = CANNA_KEY_End;
    } else if (XK_F1 <= ksym && ksym <= XK_F10) {
      ksym = CANNA_KEY_F1 + (ksym - XK_F1);
    } else if (ksym == XK_Hiragana_Katakana) {
      ksym = CANNA_KEY_HIRAGANA;
    } else if (ksym == XK_Zenkaku_Hankaku) {
      ksym = CANNA_KEY_HANKAKUZENKAKU;
    } else if (key_char == '\0') {
      return 1;
    } else {
      ksym = key_char;
    }

    len = jrKanjiString(0, ksym, canna->buf, sizeof(canna->buf), &canna->key_status);

    if (canna->key_status.info & KanjiModeInfo) {
      free(canna->mode);
      canna->mode = strdup(canna->key_status.mode);
    }

    if ((canna->key_status.info & KanjiGLineInfo) && canna->key_status.gline.length > 0) {
      u_char *p;

      cand = canna->key_status.gline.line;

      if ((p = alloca(strlen(cand) + 1))) {
        cand = strcpy(p, cand);
        while (*p) {
          if (*p < 0x80) {
            if (p[0] == ' ' && p[1] == ' ') {
              u_char *p2;

              *(p++) = '\n';
              p2 = p;

              while (*(++p) == ' ')
                ;
              strcpy(p2, p);
            } else {
              p++;
            }
          } else {
            if (p[0] == 0xa1 && p[1] == 0xa1 && p[2] == 0xa3 && 0xb1 <= p[3] && p[3] <= 0xb9) {
              p[0] = ' ';
              p[1] = '\n';
            }
            p += 2;
          }
        }
      }

      canna->is_selecting_cand = 1;
    } else {
      cand = canna->mode;
      canna->is_selecting_cand = 0;
    }

    if (len > 0) {
      canna->buf[len] = '\0';
      commit(canna, canna->buf);
      preedit(canna, "", 0, 0, cand);
    } else {
      preedit(canna, canna->key_status.length > 0 ? canna->key_status.echoStr : "",
              canna->key_status.revPos, canna->key_status.revLen, cand);
    }

    return 0;
  }

  return 1;
}

static int is_active(ui_im_t *im) { return ((im_canna_t*)im)->is_enabled; }

static void focused(ui_im_t *im) {
  im_canna_t *canna;

  canna = (im_canna_t*)im;

  if (canna->im.cand_screen) {
    (*canna->im.cand_screen->show)(canna->im.cand_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_canna_t *canna;

  canna = (im_canna_t*)im;

  if (canna->im.cand_screen) {
    (*canna->im.cand_screen->hide)(canna->im.cand_screen);
  }
}

/* --- global functions --- */

ui_im_t *im_canna_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                      ui_im_export_syms_t *export_syms, char *engine, u_int mod_ignore_mask) {
  im_canna_t *canna;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  if (ref_count == 0) {
    jrKanjiControl(0, KC_INITIALIZE, 0);
    syms = export_syms;

    if (!(parser_eucjp = (*syms->vt_char_encoding_parser_new)(VT_EUCJP))) {
      return NULL;
    }
  }

  if (!(canna = calloc(1, sizeof(im_canna_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  canna->term_encoding = term_encoding;
  canna->encoding_name = (*syms->vt_get_char_encoding_name)(term_encoding);

  if (canna->term_encoding != VT_EUCJP) {
    if (!(canna->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
      goto error;
    }
  }

  if (!(canna->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }

  /*
   * set methods of ui_im_t
   */
  canna->im.destroy = destroy;
  canna->im.key_event = key_event;
  canna->im.switch_mode = switch_mode;
  canna->im.is_active = is_active;
  canna->im.focused = focused;
  canna->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_CANNA_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)canna;

error:
  if (ref_count == 0) {
    if (parser_eucjp) {
      (*parser_eucjp->destroy)(parser_eucjp);
      parser_eucjp = NULL;
    }

    jrKanjiControl(0, KC_FINALIZE, 0);
  }

  if (canna) {
    if (canna->parser_term) {
      (*canna->parser_term->destroy)(canna->parser_term);
    }

    if (canna->conv) {
      (*canna->conv->destroy)(canna->conv);
    }

    free(canna);
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_canna_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if ((result = malloc(sizeof(im_info_t)))) {
    result->id = strdup("canna");
    result->name = strdup("Canna");
    result->num_args = 0;
    result->args = NULL;
    result->readable_args = NULL;
  }

  return result;
}
