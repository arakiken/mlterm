/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __IM_COMMON_H__
#define __IM_COMMON_H__

static inline u_int im_convert_encoding(ef_parser_t* parser, /* must be initialized */
                                        ef_conv_t* conv, u_char* from, u_char** to,
                                        u_int from_len) {
  u_int len;
  u_int filled_len;

  if (from == NULL || parser == NULL || conv == NULL) {
    return 0;
  }

  *to = NULL;

  len = 0;

  (*parser->set_str)(parser, from, from_len);
  (*conv->init)(conv);

#define UNIT__ 32

  while (1) {
    u_char* p;

    if (!(p = realloc(*to, len + UNIT__ + 1))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

      if (*to) {
        free(*to);
      }

      return 0;
    }

    *to = p;

    p += len;

    filled_len = (*conv->convert)(conv, p, UNIT__, parser);

    len += filled_len;

    if (filled_len == 0 && parser->is_eos) {
      /* finished converting */

      break;
    }
  }

#undef UNIT__

  if (len) {
    (*to)[len] = '\0';
  }

  return len;
}

#endif
