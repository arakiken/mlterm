/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_selection_encoding.h"

#include <stdio.h> /* NULL */
#include <vt_char_encoding.h>
#include <mef/ef_xct_parser.h>
#include <mef/ef_xct_conv.h>

/* --- static varaiables --- */

static ef_parser_t *utf_parser; /* leaked */
static ef_parser_t *parser; /* leaked */
static ef_conv_t *utf_conv; /* leaked */
static ef_conv_t *conv; /* leaked */

#ifdef USE_XLIB
static int big5_buggy;
#endif

/* --- global functions --- */

#ifdef USE_XLIB
void ui_set_big5_selection_buggy(int buggy) {
  big5_buggy = buggy;
}
#endif

ef_parser_t *ui_get_selection_parser(int utf) {
  if (utf) {
    if (!utf_parser) {
      if (!(utf_parser = vt_char_encoding_parser_new(VT_UTF8))) {
        return NULL;
      }
    }

    return utf_parser;
  } else {
    if (!parser) {
      if (!(parser = ef_xct_parser_new())) {
        return NULL;
      }
    }

    return parser;
  }
}

ef_conv_t *ui_get_selection_conv(int utf) {
  if (utf) {
    if (!utf_conv) {
      if (!(utf_conv = vt_char_encoding_conv_new(VT_UTF8))) {
        return NULL;
      }
    }

    return utf_conv;
  } else {
    if (!conv) {
#ifdef USE_XLIB
      if (big5_buggy) {
        if (!(conv = ef_xct_big5_buggy_conv_new())) {
          return NULL;
        }
      } else
#endif
      {
        if (!(conv = ef_xct_conv_new())) {
          return NULL;
        }
      }
    }

    return conv;
  }
}

#if 0
void ui_selection_encoding_final(void) {
  if (utf_parser) {
    (*utf_parser->destroy)(utf_parser);
    utf_parser = NULL;
  }

  if (parser) {
    (*parser->destroy)(parser);
    parser = NULL;
  }

  if (utf_conv) {
    (*utf_conv->destroy)(utf_conv);
    utf_conv = NULL;
  }

  if (conv) {
    (*conv->destroy)(conv);
    conv = NULL;
  }
}
#endif
