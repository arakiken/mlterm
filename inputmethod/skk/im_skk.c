/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>

#include <pobl/bl_mem.h>  /* malloc/alloca/free */
#include <pobl/bl_str.h>  /* bl_str_alloca_dup bl_str_sep bl_snprintf*/
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_conf_io.h>

#include <ui_im.h>

#include "../im_info.h"
#include "ef_str_parser.h"
#include "dict.h"

#if 0
#define IM_SKK_DEBUG 1
#endif

#define MAX_CAPTION_LEN 64

typedef u_int16_t wchar;

typedef enum input_mode {
  HIRAGANA,
  KATAKANA,
  ALPHABET_FULL,
  ALPHABET,
  MAX_INPUT_MODE

} input_mode_t;

typedef struct im_skk {
  /* input method common object */
  ui_im_t im;

  int is_enabled;
  /*
   * 1: direct input 2: convert 3: convert with okurigana
   * 4: convert with sokuon okurigana
   */
  int is_preediting;

  vt_char_encoding_t term_encoding;

  char *encoding_name; /* encoding of conversion engine */

  /* conv is NULL if term_encoding == skk encoding */
  ef_parser_t *parser_term; /* for term encoding */
  ef_conv_t *conv;          /* for term encoding */

  ef_char_t preedit[MAX_CAPTION_LEN];
  u_int preedit_len;

  void *candidate;

  char *status[MAX_INPUT_MODE];

  int dan;
  int prev_dan;

  input_mode_t mode;

  int8_t sticky_shift;
  int8_t start_candidate;

  int8_t is_editing_new_word;
  ef_char_t new_word[MAX_CAPTION_LEN];
  u_int new_word_len;

  ef_char_t preedit_orig[MAX_CAPTION_LEN];
  u_int preedit_orig_len;
  int is_preediting_orig;
  int prev_dan_orig;
  input_mode_t mode_orig;
  ef_char_t visual_chars[2];

  void *completion;

} im_skk_t;

/* --- static variables --- */

ui_im_export_syms_t *syms = NULL; /* mlterm internal symbols */
static int ref_count = 0;
static KeySym sticky_shift_ksym;
static int sticky_shift_ch;

/* --- static functions --- */

static void preedit_add(im_skk_t *skk, wchar wch) {
  ef_char_t ch;

  if (skk->preedit_len >= sizeof(skk->preedit) / sizeof(skk->preedit[0])) {
    return;
  }

  if (wch > 0xff) {
    if (skk->mode == KATAKANA && 0xa4a1 <= wch && wch <= 0xa4f3) {
      wch += 0x100;
    }

    ch.ch[0] = (wch >> 8) & 0x7f;
    ch.ch[1] = wch & 0x7f;
    ch.size = 2;
    ch.cs = JISX0208_1983;
  } else {
    ch.ch[0] = wch;
    ch.size = 1;
    ch.cs = US_ASCII;
  }
  ch.property = 0;

  skk->preedit[skk->preedit_len++] = ch;
}

static void preedit_delete(im_skk_t *skk) { --skk->preedit_len; }

static void candidate_clear(im_skk_t *skk);

static void preedit_clear(im_skk_t *skk) {
  if (skk->is_preediting && skk->mode == ALPHABET) {
    skk->mode = HIRAGANA;
  }

  skk->preedit_len = 0;
  skk->is_preediting = 0;
  skk->dan = 0;
  skk->prev_dan = 0;

  candidate_clear(skk);
}

static void preedit_to_visual(im_skk_t *skk) {
  if (skk->prev_dan) {
    if (skk->is_preediting == 4) {
      skk->preedit[skk->preedit_len] = skk->visual_chars[1];
      skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0];
      skk->preedit_len++;
    } else {
      skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0];
    }
  }
}

static void preedit_backup_visual_chars(im_skk_t *skk) {
  if (skk->prev_dan) {
    if (skk->is_preediting == 4) {
      skk->visual_chars[1] = skk->preedit[skk->preedit_len - 1];
      skk->visual_chars[0] = skk->preedit[skk->preedit_len - 2];
    } else {
      skk->visual_chars[0] = skk->preedit[skk->preedit_len - 1];
    }
  }
}

static void preedit(im_skk_t *skk, ef_char_t *preedit, u_int preedit_len, int rev_len,
                    char *candidateword, /* already converted to term encoding */
                    size_t candidateword_len, char *pos) {
  int x;
  int y;
  int rev_pos = 0;

  if (skk->preedit_orig_len > 0) {
    ef_char_t *p;

    if ((p = alloca(sizeof(*p) * (skk->preedit_orig_len + 1 + preedit_len + skk->new_word_len)))) {
      memcpy(p, skk->preedit_orig, sizeof(*p) * skk->preedit_orig_len);
      p[skk->preedit_orig_len].ch[0] = ':';
      p[skk->preedit_orig_len].size = 1;
      p[skk->preedit_orig_len].property = 0;
      p[skk->preedit_orig_len].cs = US_ASCII;

      if (skk->new_word_len > 0) {
        memcpy(p + skk->preedit_orig_len + 1, skk->new_word, sizeof(*p) * skk->new_word_len);
      }

      if (preedit_len > 0) {
        memcpy(p + skk->preedit_orig_len + 1 + skk->new_word_len, preedit,
               preedit_len * sizeof(*p));
      }

      preedit = p;
      rev_pos = skk->preedit_orig_len + 1 + skk->new_word_len;
      preedit_len += rev_pos;
    }
  }

  if (preedit == NULL) {
    goto candidate;
  } else if (preedit_len == 0) {
    if (skk->im.preedit.filled_len > 0) {
      /* Stop preediting. */
      skk->im.preedit.filled_len = 0;
    }
  } else {
    ef_char_t ch;
    vt_char_t *p;
    size_t pos_len;
    u_int count;

    pos_len = strlen(pos);

    if ((p = realloc(skk->im.preedit.chars, sizeof(vt_char_t) * (preedit_len + pos_len))) == NULL) {
      return;
    }

    (*syms->vt_str_init)(skk->im.preedit.chars = p,
                         skk->im.preedit.num_chars = (preedit_len + pos_len));

    skk->im.preedit.filled_len = 0;
    for (count = 0; count < preedit_len; count++) {
      int is_fullwidth;
      int is_comb;

      ch = preedit[count];

      if ((*syms->vt_convert_to_internal_ch)(skk->im.vtparser, &ch) <= 0) {
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

      if (rev_pos <= skk->im.preedit.filled_len && skk->im.preedit.filled_len < rev_pos + rev_len) {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_BG_COLOR,
                             VT_FG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      } else {
        (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, VT_FG_COLOR,
                             VT_BG_COLOR, 0, 0, LS_UNDERLINE_SINGLE, 0, 0);
      }

      p++;
      skk->im.preedit.filled_len++;
    }

    for (; pos_len > 0; pos_len--) {
      (*syms->vt_char_set)(p++, *(pos++), US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0,
                           LS_UNDERLINE_SINGLE, 0, 0);
      skk->im.preedit.filled_len++;
    }
  }

  (*skk->im.listener->draw_preedit_str)(skk->im.listener->self, skk->im.preedit.chars,
                                        skk->im.preedit.filled_len, skk->im.preedit.cursor_offset);

candidate:
  if (candidateword == NULL) {
    return;
  } else if (candidateword_len == 0 && (candidateword_len = strlen(candidateword)) == 0) {
    if (skk->im.stat_screen) {
      (*skk->im.stat_screen->delete)(skk->im.stat_screen);
      skk->im.stat_screen = NULL;
    }
  } else {
    u_char *tmp = NULL;

    (*skk->im.listener->get_spot)(skk->im.listener->self, skk->im.preedit.chars,
                                  skk->im.preedit.segment_offset, &x, &y);

    if (skk->im.stat_screen == NULL) {
      if (!(skk->im.stat_screen = (*syms->ui_im_status_screen_new)(
                skk->im.disp, skk->im.font_man, skk->im.color_man, skk->im.vtparser,
                (*skk->im.listener->is_vertical)(skk->im.listener->self),
                (*skk->im.listener->get_line_height)(skk->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_status_screen_new() failed.\n");
#endif

        return;
      }
    } else {
      (*skk->im.stat_screen->show)(skk->im.stat_screen);
      (*skk->im.stat_screen->set_spot)(skk->im.stat_screen, x, y);
    }

    (*skk->im.stat_screen->set)(skk->im.stat_screen, skk->parser_term, candidateword);

    if (tmp) {
      free(tmp);
    }
  }
}

static void commit(im_skk_t *skk) {
  ef_parser_t *parser;
  u_char conv_buf[256];
  size_t filled_len;

  parser = ef_str_parser_init(skk->preedit, skk->preedit_len);
  (*skk->conv->init)(skk->conv);

  while (!parser->is_eos) {
    filled_len = (*skk->conv->convert)(skk->conv, conv_buf, sizeof(conv_buf), parser);
    if (filled_len == 0) {
      /* finished converting */
      break;
    }

    (*skk->im.listener->write_to_term)(skk->im.listener->self, conv_buf, filled_len);
  }
}

static int insert_char(im_skk_t *skk, u_char key_char) {
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
      {0xa4e3, 0xa4a3, 0xa4e5, 0xa4a7, 0xa4e7}, /* xy/dh */
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

  if (skk->dan) {
    preedit_delete(skk);
  }

  if (key_char == 'a' || key_char == 'i' || key_char == 'u' || key_char == 'e' || key_char == 'o') {
    if (skk->dan == 'f' - 'a') {
      if (key_char != 'u') {
        preedit_add(skk, 0xa4d5); /* hu */
        skk->dan = 'x' - 'a';
      }
    } else if (skk->dan == 'j' - 'a') {
      if (key_char != 'i') {
        preedit_add(skk, 0xa4b8); /* zi */
        skk->dan = 'e' - 'a';
      }
    }

    if (key_char == 'a') {
      wch = kana_table[skk->dan].a;
      skk->dan = 0;
    } else if (key_char == 'i') {
      if (skk->dan == 'i' - 'a') {
        skk->dan = 0;

        return 0; /* shi/chi */
      }

      wch = kana_table[skk->dan].i;
      skk->dan = 0;
    } else if (key_char == 'u') {
      if (skk->dan == 'j' - 'a') {
        preedit_add(skk, 0xa4b8); /* zi */
        skk->dan = 'e' - 'a';
      }
      wch = kana_table[skk->dan].u;
      skk->dan = 0;
    } else if (key_char == 'e') {
      if (skk->dan == 'f' - 'a') {
        preedit_add(skk, 0xa4d5); /* hu */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'j' - 'a') {
        preedit_add(skk, 0xa4b8); /* zi */
        skk->dan = 'x' - 'a';
      }
      wch = kana_table[skk->dan].e;
      skk->dan = 0;
    } else /* if( key_char == 'o') */
    {
      if (skk->dan == 'f' - 'a') {
        preedit_add(skk, 0xa4d5); /* hu */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'j' - 'a') {
        preedit_add(skk, 0xa4b8); /* zi */
        skk->dan = 'e' - 'a';
      }
      wch = kana_table[skk->dan].o;
      skk->dan = 0;
    }
  } else if (('!' <= key_char && key_char <= '@') || ('[' <= key_char && key_char <= '_') ||
             ('{' <= key_char && key_char <= '~')) {
    if (skk->dan) {
      preedit_add(skk, skk->dan + 'a');
      skk->dan = 0;
    }

    if (key_char <= '@') {
      wch = sign_table1[key_char - '!'];
    } else if (key_char <= '_') {
      wch = sign_table2[key_char - '['];
    } else {
      wch = sign_table3[key_char - '{'];
    }
  } else {
    if (skk->dan == 'n' - 'a' && key_char != 'a' && key_char != 'i' && key_char != 'u' &&
        key_char != 'e' && key_char != 'o' && key_char != 'y') {
      if (key_char == 'n') {
        wch = 0xa4f3; /* n */
        skk->dan = 0;
      } else {
        preedit_add(skk, 0xa4f3);
        wch = key_char;
        skk->dan = key_char - 'a';
      }
    } else if (key_char == skk->dan + 'a') {
      preedit_add(skk, 0xa4c3);
      wch = key_char;
    } else if (key_char == 'y') {
      if (skk->dan == 'k' - 'a') {
        preedit_add(skk, 0xa4ad); /* ki */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'g' - 'a') {
        preedit_add(skk, 0xa4ae); /* gi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 's' - 'a') {
        preedit_add(skk, 0xa4b7); /* si */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'z' - 'a') {
        preedit_add(skk, 0xa4b8); /* zi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 't' - 'a' || skk->dan == 'c' - 'a') {
        preedit_add(skk, 0xa4c1); /* ti */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'd' - 'a') {
        preedit_add(skk, 0xa4c2); /* di */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'n' - 'a') {
        preedit_add(skk, 0xa4cb); /* ni */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'h' - 'a') {
        preedit_add(skk, 0xa4d2); /* hi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'b' - 'a') {
        preedit_add(skk, 0xa4d3); /* bi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'p' - 'a') {
        preedit_add(skk, 0xa4d4); /* pi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'm' - 'a') {
        preedit_add(skk, 0xa4df); /* mi */
        skk->dan = 'x' - 'a';
      } else if (skk->dan == 'r' - 'a') {
        preedit_add(skk, 0xa4ea); /* ri */
        skk->dan = 'x' - 'a';
      }

      if (skk->dan == 'x' - 'a') {
        skk->dan = 'e' - 'a';
        wch = 'y';
      } else if (skk->dan == 'v' - 'a') {
        if (key_char == 'u') {
          wch = 0xa5f4;
          skk->dan = 0;
        } else {
          preedit_add(skk, 0xa5f4); /* v */
          skk->dan = 'x' - 'a';

          return insert_char(skk, key_char);
        }
      } else {
        goto normal;
      }
    } else if (key_char == 'h') {
      if (skk->dan == 'c' - 'a') {
        preedit_add(skk, 0xa4c1); /* ti */
        skk->dan = 'i' - 'a';
      } else if (skk->dan == 's' - 'a') {
        preedit_add(skk, 0xa4b7); /* si */
        skk->dan = 'i' - 'a';
      } else if (skk->dan == 'd' - 'a') {
        preedit_add(skk, 0xa4c7); /* di */
        skk->dan = 'e' - 'a';
      } else {
        goto normal;
      }

      wch = 'h';
    } else {
    normal:
      if (skk->dan) {
        preedit_add(skk, skk->dan + 'a');
      }

      wch = key_char;

      if ('a' <= key_char && key_char <= 'z') {
        skk->dan = key_char - 'a';
      } else {
        skk->dan = 0;
      }
    }
  }

  if (wch == 0) {
    return 1;
  }

  preedit_add(skk, wch);

  return 0;
}

static void insert_alphabet_full(im_skk_t *skk, u_char key_char) {
  if (('a' <= key_char && key_char <= 'z') || ('A' <= key_char && key_char <= 'Z') ||
      ('0' <= key_char && key_char <= '9')) {
    preedit_add(skk, 0x2300 + key_char + 0x8080);
  } else if (0x20 <= key_char && key_char <= 0x7e) {
    insert_char(skk, key_char);
  }
}

/*
 * methods of ui_im_t
 */

static void delete(ui_im_t *im) {
  im_skk_t *skk;

  skk = (im_skk_t*)im;

  (*skk->parser_term->delete)(skk->parser_term);

  if (skk->conv) {
    (*skk->conv->delete)(skk->conv);
  }

  free(skk->status[HIRAGANA]);
  free(skk->status[KATAKANA]);
  free(skk->status[ALPHABET_FULL]);

  if (skk->completion) {
    dict_completion_finish(&skk->completion);
  }

  if (skk->candidate) {
    dict_candidate_finish(&skk->candidate);
  }

  free(skk);

  ref_count--;

  if (ref_count == 0) {
    dict_final();
  }

#ifdef IM_SKK_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count);
#endif
}

static int switch_mode(ui_im_t *im) {
  im_skk_t *skk;

  skk = (im_skk_t*)im;

  if ((skk->is_enabled = (!skk->is_enabled))) {
    skk->mode = HIRAGANA;
    preedit(skk, "", 0, 0, skk->status[skk->mode], 0, "");
  } else {
    preedit_clear(skk);
    preedit(skk, "", 0, 0, "", 0, "");
  }

  return 1;
}

static void start_to_register_new_word(im_skk_t *skk) {
  memcpy(skk->preedit_orig, skk->preedit, sizeof(skk->preedit[0]) * skk->preedit_len);
  if (skk->prev_dan) {
    if (skk->is_preediting == 4) {
      skk->preedit_len--;
    }

    skk->preedit_orig[skk->preedit_len - 1].ch[0] = skk->prev_dan + 'a';
    skk->preedit_orig[skk->preedit_len - 1].size = 1;
    skk->preedit_orig[skk->preedit_len - 1].cs = US_ASCII;
    skk->preedit_orig[skk->preedit_len - 1].property = 0;
  }

  skk->preedit_orig_len = skk->preedit_len;
  skk->is_preediting_orig = skk->is_preediting;
  skk->dan = 0;
  skk->prev_dan_orig = skk->prev_dan;
  skk->mode_orig = skk->mode;
  candidate_clear(skk);

  skk->new_word_len = 0;
  skk->is_editing_new_word = 1;
  preedit_clear(skk);
  skk->is_preediting = 0;
}

static void stop_to_register_new_word(im_skk_t *skk) {
  memcpy(skk->preedit, skk->preedit_orig, sizeof(skk->preedit_orig[0]) * skk->preedit_orig_len);
  skk->preedit_len = skk->preedit_orig_len;
  skk->preedit_orig_len = 0;
  skk->dan = 0;
  skk->prev_dan = skk->prev_dan_orig;
  skk->mode = skk->mode_orig;

  skk->new_word_len = 0;
  skk->is_editing_new_word = 0;
  skk->is_preediting = skk->is_preediting_orig;

  preedit_to_visual(skk);
}

static void candidate_set(im_skk_t *skk, int step) {
  if (skk->preedit_len == 0) {
    return;
  }

  if (skk->prev_dan) {
    if (skk->is_preediting == 4) {
      skk->visual_chars[1] = skk->preedit[--skk->preedit_len];
    }

    skk->visual_chars[0] = skk->preedit[skk->preedit_len - 1];
    skk->preedit[skk->preedit_len - 1].ch[0] = skk->prev_dan + 'a';
    skk->preedit[skk->preedit_len - 1].size = 1;
    skk->preedit[skk->preedit_len - 1].cs = US_ASCII;
    skk->preedit[skk->preedit_len - 1].property = 0;
  }

  skk->preedit_len = dict_candidate(skk->preedit, skk->preedit_len, &skk->candidate, step);

  if (!skk->candidate) {
    if (skk->is_editing_new_word) {
      return;
    }

#if 0
    if (skk->prev_dan) {
      skk->preedit[skk->preedit_len - 1] = skk->visual_chars[0];

      if (skk->is_preediting == 4) {
        skk->preedit_len++;
      }
    }
#endif

    start_to_register_new_word(skk);

    return;
  }

  if (skk->prev_dan) {
    skk->preedit[skk->preedit_len++] = skk->visual_chars[0];

    if (skk->is_preediting == 4) {
      skk->preedit[skk->preedit_len++] = skk->visual_chars[1];
    }
  }

  if (skk->dan) {
    ef_char_t *ch;

    ch = skk->preedit + (skk->preedit_len++);
    ch->ch[0] = skk->dan + 'a';
    ch->cs = US_ASCII;
    ch->size = 0;
    ch->property = 0;
  }
}

static void candidate_unset(im_skk_t *skk) {
  if (skk->candidate) {
    skk->preedit_len = dict_candidate_reset_and_finish(skk->preedit, &skk->candidate);
  }

  preedit_to_visual(skk);
}

static void candidate_clear(im_skk_t *skk) {
  if (skk->candidate) {
    dict_candidate_finish(&skk->candidate);
  }
}

static int fix(im_skk_t *skk) {
  if (skk->preedit_len > 0) {
    if (skk->candidate) {
      dict_candidate_add_to_local(skk->candidate);
    }

    if (skk->is_editing_new_word) {
      memcpy(skk->new_word + skk->new_word_len, skk->preedit,
             skk->preedit_len * sizeof(skk->preedit[0]));
      skk->new_word_len += skk->preedit_len;
      preedit(skk, "", 0, 0, skk->status[skk->mode], 0, "");
    } else {
      preedit(skk, "", 0, 0, skk->status[skk->mode], 0, "");
      commit(skk);
    }
    preedit_clear(skk);
    candidate_clear(skk);
  } else if (skk->is_editing_new_word) {
    if (skk->new_word_len > 0) {
      dict_add_new_word_to_local(skk->preedit_orig, skk->preedit_orig_len, skk->new_word,
                                 skk->new_word_len);

      candidate_clear(skk);
      stop_to_register_new_word(skk);
      candidate_set(skk, 0);
      commit(skk);
      preedit_clear(skk);
      candidate_clear(skk);
    } else {
      stop_to_register_new_word(skk);
      candidate_clear(skk);
    }
  } else {
    return 1;
  }

  return 0;
}

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_skk_t *skk;
  int ret = 0;
  char *pos = "";
  char *cand = NULL;
  u_int cand_len = 0;
  int preedited_alphabet;

  skk = (im_skk_t*)im;

  if (skk->preedit_len > 0 && !skk->candidate && (ksym == XK_Tab || ksym == XK_ISO_Left_Tab)) {
    skk->preedit_len =
        dict_completion(skk->preedit, skk->preedit_len, &skk->completion,
                        ksym == XK_ISO_Left_Tab || (event->state & ShiftMask) ? -1 : 1);
    goto end;
  } else if (skk->completion) {
    if ((event->state & ControlMask) && key_char == '\x07') {
      skk->preedit_len = dict_completion_reset_and_finish(skk->preedit, &skk->completion);

      goto end;
    } else if (ksym == XK_Shift_L || ksym == XK_Shift_R || ksym == XK_Control_L ||
               ksym == XK_Control_R) {
      /* Starting Shift+Tab or Control+g */
      return 0;
    }

    dict_completion_finish(&skk->completion);
  }

  if (key_char == ' ' && (event->state & ShiftMask)) {
    fix(skk);
    switch_mode(im);

    if (!skk->is_enabled) {
      return 0;
    }

    goto end;
  } else if (!skk->is_enabled) {
    return 1;
  }

  /* skk is enabled */

  if (skk->mode != ALPHABET && skk->mode != ALPHABET_FULL) {
    if (sticky_shift_ch ? (key_char == sticky_shift_ch) :
                          (sticky_shift_ksym ? (ksym == sticky_shift_ksym) : 0)) {
      if (skk->sticky_shift) {
        skk->sticky_shift = 0;
      } else {
        skk->sticky_shift = 1;

        return 0;
      }
    } else if (skk->sticky_shift) {
      if ('a' <= key_char && key_char <= 'z') {
        key_char -= 0x20;
      }
      event->state |= ShiftMask;
      skk->sticky_shift = 0;
    }
  }

  if ((skk->mode != ALPHABET && key_char == 'l') ||
      (skk->mode != ALPHABET_FULL && key_char == 'L')) {
    if (!skk->is_editing_new_word) {
      fix(skk);
    }
    skk->mode = (key_char == 'l') ? ALPHABET : ALPHABET_FULL;
    cand = skk->status[skk->mode];
  } else if (key_char == '\r' || key_char == '\n') {
    if ((event->state & ControlMask) && key_char == '\n') {
      if (!skk->is_editing_new_word || skk->preedit_len > 0) {
        fix(skk);
      }

      if (skk->mode == ALPHABET || skk->mode == ALPHABET_FULL) {
        /* Ctrl+j */
        skk->mode = HIRAGANA;
        cand = skk->status[skk->mode];
      }
    } else {
      preedited_alphabet = (skk->mode == ALPHABET && skk->is_preediting);

      ret = fix(skk);

      if (preedited_alphabet) {
        cand = skk->status[skk->mode];
      }
    }
  } else if (ksym == XK_BackSpace || ksym == XK_Delete || key_char == 0x08 /* Ctrl+h */) {
    if (skk->preedit_len > 0) {
      if (skk->candidate) {
        candidate_unset(skk);
        cand = skk->status[skk->mode];
      } else {
        skk->dan = skk->prev_dan = 0;
        skk->is_preediting = 1;

        if (skk->preedit_len == 1) {
          preedit_clear(skk);
        } else {
          preedit_delete(skk);
        }
      }
    } else if (skk->is_editing_new_word) {
      if (skk->new_word_len > 0) {
        skk->new_word_len--;
      }
    } else {
      ret = 1;
    }
  } else if ((event->state & ControlMask) && key_char == '\x07') {
    /* Ctrl+g */
    if (skk->candidate) {
      candidate_unset(skk);
      candidate_clear(skk);
      skk->dan = skk->prev_dan = 0;
      skk->is_preediting = 1;
    } else if (skk->is_editing_new_word && skk->preedit_len == 0) {
      stop_to_register_new_word(skk);
    } else {
      preedit_clear(skk);
    }
    cand = skk->status[skk->mode];
  } else if (key_char != ' ' && key_char != '\0') {
    if (key_char < ' ') {
      ret = 1;
    } else if (skk->mode != ALPHABET && key_char == 'q') {
      if (skk->mode == HIRAGANA) {
        skk->mode = KATAKANA;
      } else {
        skk->mode = HIRAGANA;
      }
      cand = skk->status[skk->mode];
    } else if (skk->mode != ALPHABET && !skk->is_preediting && key_char == '/') {
      skk->is_preediting = 1;
      skk->mode = ALPHABET;
      cand = skk->status[skk->mode];
    } else {
      if (skk->candidate && !skk->dan) {
        preedited_alphabet = (skk->mode == ALPHABET && skk->is_preediting);

        fix(skk);

        if (preedited_alphabet) {
          cand = skk->status[skk->mode];
        }
      }

      if (skk->mode != ALPHABET && skk->mode != ALPHABET_FULL) {
        while (event->state & ShiftMask) {
          if ('A' <= key_char && key_char <= 'Z') {
            key_char += 0x20;
          } else if (key_char < 'a' || 'z' < key_char) {
            break;
          }

          if (skk->preedit_len == 0) {
            skk->is_preediting = 1;
          } else if (skk->is_preediting && !skk->dan) {
            skk->is_preediting = 2;
          }

          break;
        }
      }

      if (skk->mode == ALPHABET) {
        preedit_add(skk, key_char);

        if (!skk->is_preediting) {
          fix(skk);
        }
      } else if (skk->mode == ALPHABET_FULL) {
        insert_alphabet_full(skk, key_char);

        fix(skk);
      } else if (insert_char(skk, key_char) != 0) {
        ret = 1;
      } else if (skk->is_preediting >= 2) {
        if (skk->dan) {
          if (skk->dan == skk->prev_dan) {
            /* sokuon */
            skk->is_preediting++;
          } else {
            skk->prev_dan = skk->dan;
          }
        } else {
          if (!skk->prev_dan &&
              (key_char == 'i' || key_char == 'u' || key_char == 'e' || key_char == 'o')) {
            skk->prev_dan = key_char - 'a';
          }

          skk->is_preediting++;
          candidate_set(skk, 0);
        }
      } else if (!skk->dan && !skk->is_preediting) {
        fix(skk);
      }
    }
  } else if (skk->is_preediting && (key_char == ' ' || ksym == XK_Up || ksym == XK_Down ||
                                    ksym == XK_Right || ksym == XK_Left)) {
    int update = 0;
    int step;
    int cur_index;
    u_int num;

    if (!skk->candidate) {
      if (key_char != ' ') {
        return 1;
      }

      step = 0;
      skk->start_candidate = 1;
    } else {
      dict_candidate_get_state(skk->candidate, &cur_index, &num);

      if (ksym == XK_Left) {
        step = -1;

        if (cur_index % CAND_UNIT == 0) {
          update = 1;
        }
      } else if (ksym == XK_Up) {
        step = -CAND_UNIT;
        update = 1;
      } else if (ksym == XK_Down) {
        step = CAND_UNIT;
        update = 1;
      } else {
        step = 1;

        if (cur_index == num - 1) {
          if (!skk->is_editing_new_word) {
            preedit_backup_visual_chars(skk);
            candidate_unset(skk);
            candidate_clear(skk);
            start_to_register_new_word(skk);
            cand = skk->status[skk->mode];

            goto end;
          } else {
            update = 1;
          }
        } else if ((cur_index % CAND_UNIT == CAND_UNIT - 1) || skk->start_candidate) {
          update = 1;
        }
      }

      skk->start_candidate = 0;
    }

    preedited_alphabet = (skk->mode == ALPHABET && skk->is_preediting);

    candidate_set(skk, step);

    if (preedited_alphabet && !skk->is_preediting) {
      cand = skk->status[skk->mode];
    }

    if (skk->candidate) {
      dict_candidate_get_state(skk->candidate, &cur_index, &num);

      if (update) {
        if ((cand = alloca(1024))) {
          dict_candidate_get_list(skk->candidate, cand, 1024, skk->conv);
        }
      }

      if ((pos = alloca(4 + DIGIT_STR_LEN(int)*2 + 1))) {
        sprintf(pos, " [%d/%d]", cur_index + 1, num);
      }
    }
  } else if (skk->is_editing_new_word) {
    if (key_char == ' ') {
      preedit_add(skk, key_char);
      fix(skk);
    }

    ret = 0;
  } else {
    ret = 1;
  }

end:
  preedit(skk, skk->preedit, skk->preedit_len, skk->is_preediting ? skk->preedit_len : 0, cand,
          cand_len, pos);

  return ret;
}

static int is_active(ui_im_t *im) { return ((im_skk_t*)im)->is_enabled; }

static void focused(ui_im_t *im) {
  im_skk_t *skk;

  skk = (im_skk_t*)im;

  if (skk->im.stat_screen) {
    (*skk->im.stat_screen->show)(skk->im.stat_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_skk_t *skk;

  skk = (im_skk_t*)im;

  if (skk->im.stat_screen) {
    (*skk->im.stat_screen->hide)(skk->im.stat_screen);
  }
}

static void set_sticky_shift_key(char *key) {
  int digit;
  if (strlen(key) == 1) {
    sticky_shift_ch = *key;
  } else if (sscanf(key, "\\x%x", &digit) == 1) {
    sticky_shift_ch = digit;
  } else {
    sticky_shift_ksym = (*syms->XStringToKeysym)(key);
    sticky_shift_ch = 0;

    return;
  }

  sticky_shift_ksym = 0;
}

/* --- global functions --- */

ui_im_t *im_skk_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                    ui_im_export_syms_t *export_syms, char *options, u_int mod_ignore_mask) {
  im_skk_t *skk;
  ef_parser_t *parser;
  char *env;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  if (ref_count == 0) {
    syms = export_syms;
  }

  if (!(skk = calloc(1, sizeof(im_skk_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    goto error;
  }

  if ((env = getenv("SKK_DICTIONARY"))) {
    dict_set_global(env);
  }

  if ((env = getenv("SKK_STICKY_SHIFT_KEY"))) {
    set_sticky_shift_key(env);
  }

  /* Same processing as skk_widget_new() in mc_im.c */
  if (options) {
#if 1
    /* XXX Compat with 3.8.3 or before. */
    if (!strchr(options, '=')) {
      dict_set_global(options);
    } else
#endif
    {
      char *next;

      do {
        if ((next = strchr(options, ','))) {
          *(next++) = '\0';
        }

        if (strncmp(options, "sskey=", 6) == 0) {
          set_sticky_shift_key(options + 6);
        } else if (strncmp(options, "dict=", 5) == 0) {
          dict_set_global(options + 5);
        }
      } while ((options = next));
    }
  }

  skk->term_encoding = term_encoding;
  skk->encoding_name = (*syms->vt_get_char_encoding_name)(term_encoding);

  if (!(skk->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
    goto error;
  }

  if (!(skk->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }

  skk->status[HIRAGANA] = "\xa4\xab\xa4\xca";
  skk->status[KATAKANA] = "\xa5\xab\xa5\xca";
  skk->status[ALPHABET_FULL] = "\xc1\xb4\xb1\xd1";
  skk->status[ALPHABET] = "SKK";

  if (term_encoding == VT_EUCJP || !(parser = (*syms->vt_char_encoding_parser_new)(VT_EUCJP))) {
    input_mode_t mode;

    for (mode = 0; mode <= ALPHABET_FULL; mode++) {
      skk->status[mode] = strdup(skk->status[mode]);
    }
  } else {
    input_mode_t mode;
    u_char buf[64]; /* enough for EUCJP(5bytes) -> Other CS */

    for (mode = 0; mode <= ALPHABET_FULL; mode++) {
      (*parser->init)(parser);
      (*parser->set_str)(parser, skk->status[mode], 5);
      (*skk->conv->init)(skk->conv);
      (*skk->conv->convert)(skk->conv, buf, sizeof(buf) - 1, parser);
      skk->status[mode] = strdup(buf);
    }

    (*parser->delete)(parser);
  }

  /*
   * set methods of ui_im_t
   */
  skk->im.delete = delete;
  skk->im.key_event = key_event;
  skk->im.switch_mode = switch_mode;
  skk->im.is_active = is_active;
  skk->im.focused = focused;
  skk->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_SKK_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)skk;

error:
  if (skk) {
    if (skk->parser_term) {
      (*skk->parser_term->delete)(skk->parser_term);
    }

    if (skk->conv) {
      (*skk->conv->delete)(skk->conv);
    }

    free(skk);
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_skk_get_info(char *locale, char *encoding) {
  im_info_t *result;

  if ((result = malloc(sizeof(im_info_t)))) {
    result->id = strdup("skk");
    result->name = strdup("Skk");
    result->num_args = 0;
    result->args = NULL;
    result->readable_args = NULL;
  }

  return result;
}
