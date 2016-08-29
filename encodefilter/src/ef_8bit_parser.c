/*
 *	$Id$
 */

#include "ef_8bit_parser.h"

#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

typedef struct ef_iscii_parser {
  ef_parser_t parser;
  ef_charset_t cs;

} ef_iscii_parser_t;

/* --- static functions --- */

static int parser_next_char_intern(ef_parser_t* parser, ef_char_t* ch, ef_charset_t cs) {
  u_char c;

  if (parser->is_eos) {
    return 0;
  }

  ef_parser_mark(parser);

  ch->ch[0] = c = *parser->str;
  ch->size = 1;
  ch->property = 0;

  if (/* 0x0 <= c && */ c <= 0x7f && (cs != VISCII || (c != 0x02 && c != 0x05 && c != 0x06 &&
                                                       c != 0x14 && c != 0x19 && c != 0x1e))) {
    ch->cs = US_ASCII;
  } else {
    if (cs == CP874 && (c == 0xd1 || (0xd4 <= c && c <= 0xda) || (0xe7 <= c && c <= 0xee))) {
      ch->property = EF_COMBINING;
    }

    ch->cs = cs;
  }

  ef_parser_increment(parser);

  return 1;
}

static int koi8_r_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, KOI8_R);
}

static int koi8_u_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, KOI8_U);
}

static int koi8_t_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, KOI8_T);
}

static int georgian_ps_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, GEORGIAN_PS);
}

static int cp1250_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1250);
}

static int cp1251_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1251);
}

static int cp1252_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1252);
}

static int cp1253_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1253);
}

static int cp1254_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1254);
}

static int cp1255_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1255);
}

static int cp1256_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1256);
}

static int cp1257_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1257);
}

static int cp1258_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP1258);
}

static int cp874_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, CP874);
}

static int viscii_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, VISCII);
}

static int iscii_parser_next_char(ef_parser_t* parser, ef_char_t* ch) {
  return parser_next_char_intern(parser, ch, ((ef_iscii_parser_t*)parser)->cs);
}

static void parser_set_str(ef_parser_t* parser, u_char* str, size_t size) {
  parser->str = str;
  parser->left = size;
  parser->marked_left = 0;
  parser->is_eos = 0;
}

static void parser_delete(ef_parser_t* s) { free(s); }

static ef_parser_t* iscii_parser_new(ef_charset_t cs) {
  ef_iscii_parser_t* iscii_parser;

  if ((iscii_parser = malloc(sizeof(ef_iscii_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(&iscii_parser->parser);

  iscii_parser->parser.init = ef_parser_init;
  iscii_parser->parser.next_char = iscii_parser_next_char;
  iscii_parser->parser.set_str = parser_set_str;
  iscii_parser->parser.delete = parser_delete;
  iscii_parser->cs = cs;

  return &iscii_parser->parser;
}

/* --- global functions --- */

ef_parser_t* ef_koi8_r_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = koi8_r_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_koi8_u_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = koi8_u_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_koi8_t_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = koi8_t_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_georgian_ps_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = georgian_ps_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1250_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1250_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1251_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1251_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1252_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1252_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1253_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1253_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1254_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1254_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1255_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1255_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1256_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1256_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1257_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1257_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp1258_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp1258_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_cp874_parser_new(void) {
  ef_parser_t* parser;

  if ((parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(parser);

  parser->init = ef_parser_init;
  parser->next_char = cp874_parser_next_char;
  parser->set_str = parser_set_str;
  parser->delete = parser_delete;

  return parser;
}

ef_parser_t* ef_viscii_parser_new(void) {
  ef_parser_t* viscii_parser;

  if ((viscii_parser = malloc(sizeof(ef_parser_t))) == NULL) {
    return NULL;
  }

  ef_parser_init(viscii_parser);

  viscii_parser->init = ef_parser_init;
  viscii_parser->next_char = viscii_parser_next_char;
  viscii_parser->set_str = parser_set_str;
  viscii_parser->delete = parser_delete;

  return viscii_parser;
}

ef_parser_t* ef_iscii_assamese_parser_new(void) { return iscii_parser_new(ISCII_ASSAMESE); }

ef_parser_t* ef_iscii_bengali_parser_new(void) { return iscii_parser_new(ISCII_BENGALI); }

ef_parser_t* ef_iscii_gujarati_parser_new(void) { return iscii_parser_new(ISCII_GUJARATI); }

ef_parser_t* ef_iscii_hindi_parser_new(void) { return iscii_parser_new(ISCII_HINDI); }

ef_parser_t* ef_iscii_kannada_parser_new(void) { return iscii_parser_new(ISCII_KANNADA); }

ef_parser_t* ef_iscii_malayalam_parser_new(void) { return iscii_parser_new(ISCII_MALAYALAM); }

ef_parser_t* ef_iscii_oriya_parser_new(void) { return iscii_parser_new(ISCII_ORIYA); }

ef_parser_t* ef_iscii_punjabi_parser_new(void) { return iscii_parser_new(ISCII_PUNJABI); }

ef_parser_t* ef_iscii_tamil_parser_new(void) { return iscii_parser_new(ISCII_TAMIL); }

ef_parser_t* ef_iscii_telugu_parser_new(void) { return iscii_parser_new(ISCII_TELUGU); }
