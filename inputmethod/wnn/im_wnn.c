/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>

#include <pobl/bl_mem.h>  /* malloc/alloca/free */
#include <pobl/bl_str.h>  /* strdup/bl_str_sep/bl_snprintf */
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */

#include <ui_im.h>

#include "../im_common.h"
#include "../im_info.h"
#include "wnnlib.h"

#if 0
#define IM_WNN_DEBUG 1
#endif

/* Don't forget to modify digit_to_wstr() if MAX_DIGIT_NUM is changed. */
#define MAX_DIGIT_NUM 3 /* 999 */
#define CAND_WINDOW_ROWS 5

typedef struct im_wnn {
  /* input method common object */
  ui_im_t im;

  char buf[1024];
  int is_enabled;
  int is_cand;

  vt_char_encoding_t term_encoding;

  char *encoding_name; /* encoding of conversion engine */

  /* conv is NULL if term_encoding == wnn encoding */
  ef_parser_t *parser_term; /* for term encoding */
  ef_conv_t *conv;          /* for term encoding */

  jcConvBuf *convbuf;
  int dan;

} im_wnn_t;

/* --- static variables --- */

static int ref_count = 0;
static ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
static ef_parser_t *parser_wchar = NULL;

/* --- static functions --- */

static wchar *digit_to_wstr(wchar *dst, int digit) {
  int num;

  if (digit >= 1000) {
    /* Ignore it because MAX_DIGIT_NUM is 3 (=999). */
    digit -= (digit / 1000) * 1000;
  }

  if (digit >= 100) {
    num = digit / 100;
    *(dst++) = num + 0x30;
    digit -= num * 100;
  } else if (((digit - 1) / CAND_WINDOW_ROWS + 1) * CAND_WINDOW_ROWS >= 100) {
    *(dst++) = ' ';
  }

  if (digit >= 10) {
    num = digit / 10;
    *(dst++) = num + 0x30;
    digit -= num * 10;
  } else if (((digit - 1) / CAND_WINDOW_ROWS + 1) * CAND_WINDOW_ROWS >= 10) {
    *(dst++) = ' ';
  }

  *(dst++) = digit + 0x30;

  return dst;
}

static void wchar_parser_set_str(ef_parser_t *parser, u_char *str, size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

static void wchar_parser_destroy(ef_parser_t *parser) { free(parser); }

static int wchar_parser_next_char(ef_parser_t *parser, ef_char_t *ch) {
  wchar wch;

  if (parser->is_eos) {
    return 0;
  }

  ef_parser_mark(parser);

  wch = ((wchar*)parser->str)[0];

  if (wch < 0x100) {
    ch->size = 1;

    if (wch < 0x80) {
      ch->ch[0] = wch;
      ch->cs = US_ASCII;
    } else {
      ch->ch[0] = (wch & 0x7f);
      ch->cs = JISX0201_KATA;
    }
  } else if ((wch & 0x8080) == 0x8080) {
    ef_int_to_bytes(ch->ch, 2, wch & ~0x8080);
    ch->size = 2;
    ch->cs = JISX0208_1983;
  } else {
    ef_parser_reset(parser);

    return 0;
  }

  ef_parser_n_increment(parser, 2);
  ch->property = 0;

  return 1;
}

static ef_parser_t *wchar_parser_new(void) {
  ef_parser_t *parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->set_str = wchar_parser_set_str;
  parser->destroy = wchar_parser_destroy;
  parser->next_char = wchar_parser_next_char;

  return parser;
}

static void preedit(im_wnn_t *wnn, char *preedit,                                      /* wchar */
                    size_t preedit_len, int rev_pos, int rev_len, char *candidateword, /* wchar */
                    size_t candidateword_len, char *pos) {
  int x;
  int y;

  if (preedit == NULL) {
    goto candidate;
  } else if (preedit_len == 0) {
    if (wnn->im.preedit.filled_len > 0) {
      /* Stop preediting. */
      wnn->im.preedit.filled_len = 0;
    }
  } else {
    ef_char_t ch;
    vt_char_t *p;
    u_int num_chars;
    u_int len;
    u_char *tmp = NULL;
    size_t pos_len;

    wnn->im.preedit.cursor_offset = rev_pos;

    num_chars = 0;
    (*parser_wchar->init)(parser_wchar);
    (*parser_wchar->set_str)(parser_wchar, preedit, preedit_len);
    while ((*parser_wchar->next_char)(parser_wchar, &ch)) {
      num_chars++;
    }

    pos_len = strlen(pos);

    if ((p = realloc(wnn->im.preedit.chars, sizeof(vt_char_t) * (num_chars + pos_len))) ==
        NULL) {
      return;
    }

    (*parser_wchar->init)(parser_wchar);
    if ((len = im_convert_encoding(parser_wchar, wnn->conv, preedit, &tmp, preedit_len))) {
      preedit = tmp;
      preedit_len = len;
    }

    (*syms->vt_str_init)(wnn->im.preedit.chars = p,
                         wnn->im.preedit.num_chars = (num_chars + pos_len));
    wnn->im.preedit.filled_len = 0;

    (*wnn->parser_term->init)(wnn->parser_term);
    (*wnn->parser_term->set_str)(wnn->parser_term, preedit, preedit_len);
    while ((*wnn->parser_term->next_char)(wnn->parser_term, &ch)) {
      int is_fullwidth;
      int is_comb;

      if ((*syms->vt_convert_to_internal_ch)(wnn->im.vtparser, &ch) <= 0) {
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

      if (wnn->im.preedit.cursor_offset <= wnn->im.preedit.filled_len &&
          wnn->im.preedit.filled_len < wnn->im.preedit.cursor_offset + rev_len) {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_BG_COLOR,
                             VT_FG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      } else {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_FG_COLOR,
                             VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      }

      p++;
      wnn->im.preedit.filled_len++;
    }

    for (; pos_len > 0; pos_len--) {
      (*syms->vt_char_set)(p++, *(pos++), US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0,
                           LS_UNDERLINE_SINGLE, 0, 0);
      wnn->im.preedit.filled_len++;
    }

    if (tmp) {
      free(tmp);
    }
  }

  (*wnn->im.listener->draw_preedit_str)(wnn->im.listener->self, wnn->im.preedit.chars,
                                        wnn->im.preedit.filled_len, wnn->im.preedit.cursor_offset);

candidate:
  if (candidateword == NULL) {
    return;
  } else if (candidateword_len == 0) {
    if (wnn->im.stat_screen) {
      (*wnn->im.stat_screen->destroy)(wnn->im.stat_screen);
      wnn->im.stat_screen = NULL;
    }
  } else {
    u_char *tmp = NULL;

    (*wnn->im.listener->get_spot)(wnn->im.listener->self, wnn->im.preedit.chars,
                                  wnn->im.preedit.segment_offset, &x, &y);

    if (wnn->im.stat_screen == NULL) {
      if (!(wnn->im.stat_screen = (*syms->ui_im_status_screen_new)(
                wnn->im.disp, wnn->im.font_man, wnn->im.color_man, wnn->im.vtparser,
                (*wnn->im.listener->is_vertical)(wnn->im.listener->self),
                (*wnn->im.listener->get_line_height)(wnn->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_status_screen_new() failed.\n");
#endif

        return;
      }
    } else {
      (*wnn->im.stat_screen->show)(wnn->im.stat_screen);
      (*wnn->im.stat_screen->set_spot)(wnn->im.stat_screen, x, y);
    }

    (*parser_wchar->init)(parser_wchar);
    if (im_convert_encoding(parser_wchar, wnn->conv, candidateword, &tmp, candidateword_len)) {
      candidateword = tmp;
    }

    (*wnn->im.stat_screen->set)(wnn->im.stat_screen, wnn->parser_term, candidateword);

    if (tmp) {
      free(tmp);
    }
  }
}

static void commit(im_wnn_t *wnn, const char *str, size_t len) {
  u_char conv_buf[256];
  size_t filled_len;

  (*parser_wchar->init)(parser_wchar);
  (*parser_wchar->set_str)(parser_wchar, (u_char*)str, len);

  (*wnn->conv->init)(wnn->conv);

  while (!parser_wchar->is_eos) {
    filled_len = (*wnn->conv->convert)(wnn->conv, conv_buf, sizeof(conv_buf), parser_wchar);

    if (filled_len == 0) {
      /* finished converting */
      break;
    }

    (*wnn->im.listener->write_to_term)(wnn->im.listener->self, conv_buf, filled_len);
  }
}

static int insert_char(im_wnn_t *wnn, u_char key_char) {
  static struct {
    wchar a;
    wchar i;
    wchar u;
    wchar e;
    wchar o;

  } kana_table[] = {
      /* a */ /* i */ /* u */ /* e */ /* o */
      {0xa4a2, 0xa4a4, 0xa4a6, 0xa4a8, 0xa4aa},
      {0xa4d0, 0xa4d3, 0xa4d6, 0xa4d9, 0xa4dc}, /* b */
      {0xa4ab, 0xa4ad, 0xa4af, 0xa4b1, 0xa4b3}, /* c */
      {0xa4c0, 0xa4c2, 0xa4c5, 0xa4c7, 0xa4c9}, /* d */
      {0xa4e3, 0xa4a3, 0xa4e5, 0xa4a7, 0xa4e7}, /* xy */
      {
       0, 0, 0xa4d5, 0, 0,
      },                                        /* f */
      {0xa4ac, 0xa4ae, 0xa4b0, 0xa4b2, 0xa4b4}, /* g */
      {0xa4cf, 0xa4d2, 0xa4d5, 0xa4d8, 0xa4db}, /* h */
      {0xa4e3, 0, 0xa4e5, 0xa4a7, 0xa4e7},      /* ch/sh */
      {
       0, 0xa4b8, 0, 0, 0,
      },                                        /* j */
      {0xa4ab, 0xa4ad, 0xa4af, 0xa4b1, 0xa4b3}, /* k */
      {0xa4a1, 0xa4a3, 0xa4a5, 0xa4a7, 0xa4a9}, /* l */
      {0xa4de, 0xa4df, 0xa4e0, 0xa4e1, 0xa4e2}, /* m */
      {0xa4ca, 0xa4cb, 0xa4cc, 0xa4cd, 0xa4ce}, /* n */
      {
       0, 0, 0, 0, 0,
      },
      {0xa4d1, 0xa4d4, 0xa4d7, 0xa4da, 0xa4dd}, /* p */
      {
       0, 0, 0, 0, 0,
      },
      {0xa4e9, 0xa4ea, 0xa4eb, 0xa4ec, 0xa4ed}, /* r */
      {0xa4b5, 0xa4b7, 0xa4b9, 0xa4bb, 0xa4bd}, /* s */
      {0xa4bf, 0xa4c1, 0xa4c4, 0xa4c6, 0xa4c8}, /* t */
      {
       0, 0, 0, 0, 0,
      },
      {
       0, 0, 0, 0, 0,
      },
      {0xa4ef, 0xa4f0, 0, 0xa4f1, 0xa4f2},      /* w */
      {0xa4a1, 0xa4a3, 0xa4a5, 0xa4a7, 0xa4a9}, /* x */
      {0xa4e4, 0, 0xa4e6, 0, 0xa4e8},           /* y */
      {0xa4b6, 0xa4b8, 0xa4ba, 0xa4bc, 0xa4be}, /* z */
  };
  static wchar sign_table1[] = {
      0xa1aa, 0xa1c9, 0xa1f4, 0xa1f0, 0xa1f3, 0xa1f5, 0xa1c7, 0xa1ca, 0xa1cb, 0xa1f6, 0xa1dc,
      0xa1a4, 0xa1bc, 0xa1a3, 0xa1bf, 0xa3b0, 0xa3b1, 0xa3b2, 0xa3b3, 0xa3b4, 0xa3b5, 0xa3b6,
      0xa3b7, 0xa3b8, 0xa3b9, 0xa1a7, 0xa1a8, 0xa1e3, 0xa1e1, 0xa1e4, 0xa1a9, 0xa1f7,
  };
  static wchar sign_table2[] = {
      0xa1ce, 0xa1ef, 0xa1cf, 0xa1b0, 0xa1b2,
  };
  static wchar sign_table3[] = {
      0xa1d0, 0xa1c3, 0xa1d1, 0xa1c1,
  };
  wchar wch;

  if (wnn->dan) {
    jcDeleteChar(wnn->convbuf, 1);
  }

  if (key_char == 'a' || key_char == 'i' || key_char == 'u' || key_char == 'e' || key_char == 'o') {
    if (wnn->dan == 'f' - 'a') {
      if (key_char != 'u') {
        jcInsertChar(wnn->convbuf, 0xa4d5); /* hu */
        wnn->dan = 'x' - 'a';
      }
    } else if (wnn->dan == 'j' - 'a') {
      if (key_char != 'i') {
        jcInsertChar(wnn->convbuf, 0xa4b8); /* zi */
        wnn->dan = 'e' - 'a';
      }
    }

    if (key_char == 'a') {
      wch = kana_table[wnn->dan].a;
      wnn->dan = 0;
    } else if (key_char == 'i') {
      if (wnn->dan == 'i' - 'a') {
        wnn->dan = 0;

        return 0; /* shi/chi */
      }

      wch = kana_table[wnn->dan].i;
      wnn->dan = 0;
    } else if (key_char == 'u') {
      if (wnn->dan == 'j' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b8); /* zi */
        wnn->dan = 'e' - 'a';
      }
      wch = kana_table[wnn->dan].u;
      wnn->dan = 0;
    } else if (key_char == 'e') {
      if (wnn->dan == 'f' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4d5); /* hu */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'j' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b8); /* zi */
        wnn->dan = 'x' - 'a';
      }
      wch = kana_table[wnn->dan].e;
      wnn->dan = 0;
    } else /* if( key_char == 'o') */
    {
      if (wnn->dan == 'f' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4d5); /* hu */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'j' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b8); /* zi */
        wnn->dan = 'e' - 'a';
      }
      wch = kana_table[wnn->dan].o;
      wnn->dan = 0;
    }
  } else if (('!' <= key_char && key_char <= '@') || ('[' <= key_char && key_char <= '_') ||
             ('{' <= key_char && key_char <= '~')) {
    if (wnn->dan) {
      jcInsertChar(wnn->convbuf, wnn->dan + 'a');
      wnn->dan = 0;
    }

    if (key_char <= '@') {
      wch = sign_table1[key_char - '!'];
    } else if (key_char <= '_') {
      wch = sign_table2[key_char - '['];
    } else {
      wch = sign_table3[key_char - '{'];
    }
  } else {
    if (wnn->dan == 'n' - 'a' && key_char != 'a' && key_char != 'i' && key_char != 'u' &&
        key_char != 'e' && key_char != 'o' && key_char != 'y') {
      if (key_char == 'n') {
        wch = 0xa4f3; /* n */
        wnn->dan = 0;
      } else {
        jcInsertChar(wnn->convbuf, 0xa4f3);
        wch = key_char;
        wnn->dan = key_char - 'a';
      }
    } else if (key_char == wnn->dan + 'a') {
      jcInsertChar(wnn->convbuf, 0xa4c3);
      wch = key_char;
    } else if (key_char == 'y') {
      if (wnn->dan == 'k' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4ad); /* ki */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'g' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4ae); /* gi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 's' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b7); /* si */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'z' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b8); /* zi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 't' - 'a' || wnn->dan == 'c' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4c1); /* ti */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'd' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4c2); /* di */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'n' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4cb); /* ni */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'h' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4d2); /* hi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'b' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4d3); /* bi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'p' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4d4); /* pi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'm' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4df); /* mi */
        wnn->dan = 'x' - 'a';
      } else if (wnn->dan == 'r' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4ea); /* ri */
        wnn->dan = 'x' - 'a';
      }

      if (wnn->dan == 'x' - 'a') {
        wnn->dan = 'e' - 'a';
        wch = 'y';
      } else if (wnn->dan == 'v' - 'a') {
        if (key_char == 'u') {
          wch = 0xa5f4;
          wnn->dan = 0;
        } else {
          jcInsertChar(wnn->convbuf, 0xa5f4); /* v */
          wnn->dan = 'x' - 'a';

          return insert_char(wnn, key_char);
        }
      } else {
        goto normal;
      }
    } else if (key_char == 'h') {
      if (wnn->dan == 'c' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4c1); /* ti */
        wnn->dan = 'i' - 'a';
      } else if (wnn->dan == 's' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4b7); /* si */
        wnn->dan = 'i' - 'a';
      } else if (wnn->dan == 'd' - 'a') {
        jcInsertChar(wnn->convbuf, 0xa4c7); /* di */
        wnn->dan = 'e' - 'a';
      } else {
        goto normal;
      }

      wch = 'h';
    } else {
    normal:
      if (wnn->dan) {
        jcInsertChar(wnn->convbuf, wnn->dan + 'a');
      }

      wch = key_char;

      if ('a' <= key_char && key_char <= 'z') {
        wnn->dan = key_char - 'a';
      } else {
        wnn->dan = 0;
      }
    }
  }

  if (wch == 0) {
    return 1;
  }

  if (jcInsertChar(wnn->convbuf, wch) != 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " InsertChar failed.\n");
#endif
  }

  return 0;
}

static int fix(im_wnn_t *wnn) {
  if (wnn->convbuf->displayEnd > wnn->convbuf->displayBuf) {
    wnn->dan = 0;
    wnn->is_cand = 0;
    preedit(wnn, "", 0, 0, 0, "", 0, "");
    commit(wnn, wnn->convbuf->displayBuf,
           (wnn->convbuf->displayEnd - wnn->convbuf->displayBuf) * 2);
    jcFix(wnn->convbuf);
    jcClear(wnn->convbuf);

    return 0;
  } else {
    return 1;
  }
}

/*
 * methods of ui_im_t
 */

static void destroy(ui_im_t *im) {
  im_wnn_t *wnn;
  struct wnn_buf *buf;

  wnn = (im_wnn_t*)im;

  (*wnn->parser_term->destroy)(wnn->parser_term);

  if (wnn->conv) {
    (*wnn->conv->destroy)(wnn->conv);
  }

  buf = wnn->convbuf->wnn;
  jcDestroyBuffer(wnn->convbuf, 1);
  jcClose(buf);

  free(wnn);

  ref_count--;

#ifdef IM_WNN_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was destroyed. ref_count: %d\n", ref_count);
#endif

  if (ref_count == 0) {
    (*parser_wchar->destroy)(parser_wchar);
    parser_wchar = NULL;
  }
}

static int switch_mode(ui_im_t *im) {
  im_wnn_t *wnn;

  wnn = (im_wnn_t*)im;

  if ((wnn->is_enabled = (!wnn->is_enabled))) {
    preedit(wnn, NULL, 0, 0, 0, NULL, 0, "");
  } else {
    jcClear(wnn->convbuf);
    preedit(wnn, "", 0, 0, 0, "", 0, "");
  }

  return 1;
}

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  wchar kana[2] = {0xa4ab, 0xa4ca};
  im_wnn_t *wnn;
  wchar *cand = NULL;
  size_t cand_len = 0;
  int ret = 0;
  char *pos = "";

  wnn = (im_wnn_t*)im;

  if (key_char == ' ' && (event->state & ShiftMask)) {
    switch_mode(im);

    if (!wnn->is_enabled) {
      return 0;
    }
  } else if (!wnn->is_enabled) {
    return 1;
  } else if (key_char == '\r' || key_char == '\n') {
    ret = fix(wnn);
  } else if (ksym == XK_BackSpace || ksym == XK_Delete || key_char == 0x08 /* Ctrl+h */) {
    if (wnn->im.preedit.filled_len > 0) {
      wnn->dan = 0;
      wnn->is_cand = 0;

      if (jcIsConverted(wnn->convbuf, 0)) {
        jcCancel(wnn->convbuf);
        jcBottom(wnn->convbuf);
      } else {
        jcDeleteChar(wnn->convbuf, 1);
      }
    } else {
      ret = 1;
    }
  } else if (key_char != ' ' && key_char != '\0') {
    if (wnn->im.preedit.filled_len > 0 && jcIsConverted(wnn->convbuf, 0)) {
      if (key_char < ' ') {
        if (key_char == 0x03 || key_char == 0x07) /* Ctrl+c, Ctrl+g */
        {
          jcCancel(wnn->convbuf);
          jcBottom(wnn->convbuf);
        }

        ret = 1;
      } else {
        fix(wnn);
      }
    } else if (key_char < ' ') {
      ret = 1;
    } else {
      wnn->is_cand = 0;
    }

    if (ret == 0 && insert_char(wnn, key_char) != 0) {
      ret = 1;
    }
  } else {
    if (key_char == ' ' && wnn->im.preedit.filled_len == 0) {
      ret = 1;
    }

    if (key_char == ' ' || ksym == XK_Up || ksym == XK_Down) {
      if (!jcIsConverted(wnn->convbuf, 0)) {
        wchar kanji[2] = {0xb4c1, 0xbbfa};

        if (key_char != ' ') {
          ret = 1;
        }

        if (jcConvert(wnn->convbuf, 0, 0, 0) != 0) {
#ifdef DEBUG
          bl_debug_printf(BL_DEBUG_TAG " jcConvert failed.\n");
#endif
        }

        cand = kanji;
        cand_len = 2;

        wnn->dan = 0;
        wnn->is_cand = 0;
      } else {
        int ncand;
        int curcand;

        if (jcNext(wnn->convbuf, 0, ksym == XK_Up ? JC_PREV : JC_NEXT) != 0) {
#ifdef DEBUG
          bl_debug_printf(BL_DEBUG_TAG " jcNext failed.\n");
#endif
        }

        if (jcCandidateInfo(wnn->convbuf, 0, &ncand, &curcand) == 0 &&
            (!wnn->is_cand ||
             (ksym == XK_Up
                  ? (curcand % CAND_WINDOW_ROWS == CAND_WINDOW_ROWS - 1 || curcand == ncand - 1)
                  : (curcand % CAND_WINDOW_ROWS == 0)))) {
          wchar tmp[1024];
          wchar *src;
          wchar *dst;
          int count;
          int beg = curcand - curcand % CAND_WINDOW_ROWS;

          wnn->is_cand = 1;

          for (count = 0; count < CAND_WINDOW_ROWS; count++) {
            if (jcGetCandidate(wnn->convbuf, beg + count, tmp, sizeof(tmp)) == 0) {
              cand_len += (MAX_DIGIT_NUM + 1);
              for (src = tmp; *src; src++) {
                cand_len++;
              }

              if (count < CAND_WINDOW_ROWS - 1 && beg + count < ncand - 1) {
                cand_len++; /* '\n' */
              }
            }
          }

          if ((cand = alloca(cand_len * sizeof(wchar)))) {
            dst = cand;

            for (count = 0; count < CAND_WINDOW_ROWS; count++) {
              if (jcGetCandidate(wnn->convbuf, beg + count, tmp, sizeof(tmp)) == 0) {
                dst = digit_to_wstr(dst, beg + count + 1);
                *(dst++) = ' ';

                for (src = tmp; *src; src++) {
                  *(dst++) = *src;
                }

                if (count < CAND_WINDOW_ROWS - 1 && beg + count < ncand - 1) {
                  *(dst++) = '\n';
                }
              }
            }
          }

          cand_len = dst - cand;
        }

        if ((pos = alloca(4 + DIGIT_STR_LEN(int)*2 + 1))) {
          sprintf(pos, " [%d/%d]", curcand + 1, ncand);
        }
      }
    } else if (ksym == XK_Right) {
      if (event->state & ShiftMask) {
        jcExpand(wnn->convbuf, 0, jcIsConverted(wnn->convbuf, 0));
      } else {
        jcMove(wnn->convbuf, 0, JC_FORWARD);
      }

      wnn->dan = 0;
      wnn->is_cand = 0;
    } else if (ksym == XK_Left) {
      if (event->state & ShiftMask) {
        jcShrink(wnn->convbuf, 0, jcIsConverted(wnn->convbuf, 0));
      } else {
        jcMove(wnn->convbuf, 0, JC_BACKWARD);
      }

      wnn->dan = 0;
      wnn->is_cand = 0;
    } else {
      ret = 1;
    }
  }

  if (jcIsConverted(wnn->convbuf, 0)) {
    preedit(wnn, wnn->convbuf->displayBuf,
            (wnn->convbuf->displayEnd - wnn->convbuf->displayBuf) * 2,
            wnn->convbuf->clauseInfo[wnn->convbuf->curLCStart].dispp - wnn->convbuf->displayBuf,
            wnn->convbuf->clauseInfo[wnn->convbuf->curLCEnd].dispp -
                wnn->convbuf->clauseInfo[wnn->convbuf->curLCStart].dispp,
            cand, cand_len * sizeof(wchar), pos);
  } else {
    preedit(wnn, wnn->convbuf->displayBuf,
            (wnn->convbuf->displayEnd - wnn->convbuf->displayBuf) * 2, jcDotOffset(wnn->convbuf), 0,
            (char*)kana, sizeof(kana), pos);
  }

  return ret;
}

static int is_active(ui_im_t *im) { return ((im_wnn_t*)im)->is_enabled; }

static void focused(ui_im_t *im) {
  im_wnn_t *wnn;

  wnn = (im_wnn_t*)im;

  if (wnn->im.cand_screen) {
    (*wnn->im.cand_screen->show)(wnn->im.cand_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_wnn_t *wnn;

  wnn = (im_wnn_t*)im;

  if (wnn->im.cand_screen) {
    (*wnn->im.cand_screen->hide)(wnn->im.cand_screen);
  }
}

/* --- global functions --- */

ui_im_t *im_wnn_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                    ui_im_export_syms_t *export_syms, char *server, u_int mod_ignore_mask) {
  im_wnn_t *wnn;
  struct wnn_buf *buf;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  if (ref_count == 0) {
    syms = export_syms;
    parser_wchar = wchar_parser_new();
  }

  if (!(wnn = calloc(1, sizeof(im_wnn_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  wnn->term_encoding = term_encoding;
  wnn->encoding_name = (*syms->vt_get_char_encoding_name)(term_encoding);

  if (!(wnn->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
    goto error;
  }

  if (!(wnn->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }

  if (!(buf = jcOpen(server, "", 0, "", bl_msg_printf, bl_msg_printf, 0))) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " jcOpen failed.\n");
#endif

    goto error;
  }

  wnn->convbuf = jcCreateBuffer(buf, 0, 0);

  /*
   * set methods of ui_im_t
   */
  wnn->im.destroy = destroy;
  wnn->im.key_event = key_event;
  wnn->im.switch_mode = switch_mode;
  wnn->im.is_active = is_active;
  wnn->im.focused = focused;
  wnn->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_WNN_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)wnn;

error:
  if (ref_count == 0) {
    if (parser_wchar) {
      (*parser_wchar->destroy)(parser_wchar);
      parser_wchar = NULL;
    }
  }

  if (wnn) {
    if (wnn->parser_term) {
      (*wnn->parser_term->destroy)(wnn->parser_term);
    }

    if (wnn->conv) {
      (*wnn->conv->destroy)(wnn->conv);
    }

    buf = wnn->convbuf->wnn;
    jcDestroyBuffer(wnn->convbuf, 1);
    jcClose(buf);

    free(wnn);
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_wnn_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if ((result = malloc(sizeof(im_info_t)))) {
    result->id = strdup("wnn");
    result->name = strdup("Wnn");
    result->num_args = 0;
    result->args = NULL;
    result->readable_args = NULL;
  }

  return result;
}
