/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_termcap.h"

#include <string.h>      /* strchr */
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <pobl/bl_file.h>
#include <pobl/bl_conf_io.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

typedef enum str_field {
  TC_DELETE,
  TC_BACKSPACE,
  TC_HOME,
  TC_END,
  TC_F1,
  TC_F2,
  TC_F3,
  TC_F4,
  TC_F5,

  MAX_TERMCAP_STR_FIELDS

} str_field_t;

typedef enum bool_field {
  TC_BCE,

  MAX_TERMCAP_BOOL_FIELDS

} bool_field_t;

typedef struct str_field_table {
  char *name;
  str_field_t field;

} str_field_table_t;

typedef struct bool_field_table {
  char *name;
  bool_field_t field;

} bool_field_table_t;

typedef struct vt_termcap {
  char *name;

  char *str_fields[MAX_TERMCAP_STR_FIELDS];
  int8_t bool_fields[MAX_TERMCAP_BOOL_FIELDS];

} vt_termcap_t;

/* --- static variables --- */

static vt_termcap_t *entries;
static u_int num_entries;

static str_field_table_t str_field_table[] = {
  { "kD", TC_DELETE, },
  { "kb", TC_BACKSPACE, },
  { "kh", TC_HOME, },
  { "@7", TC_END, },
  /* "\x1bOP" in xterm(279), but doc/term/mlterm.ti defined "\x1b[11~" from before. */
  { "k1", TC_F1, },
  /* "\x1bOQ" in xterm(279), but doc/term/mlterm.ti defined "\x1b[12~" from before. */
  { "k2", TC_F2, },
  /* "\x1bOR" in xterm(279), but doc/term/mlterm.ti defined "\x1b[13~" from before. */
  { "k3", TC_F3, },
  /* "\x1bOS" in xterm(279), but doc/term/mlterm.ti defined "\x1b[14~" from before. */
  { "k4", TC_F4, },
  /* Requested by Andi Cristian Serbanescu (1 Nov 2012) */
  { "k5", TC_F5, },
};

static bool_field_table_t bool_field_table[] = {
  { "ut", TC_BCE, },
};

static char *tc_file = "mlterm/termcap";

/* --- static functions --- */

static int entry_init(vt_termcap_t *termcap, const char *name) {
  memset(termcap, 0, sizeof(vt_termcap_t));
  termcap->name = strdup(name);

  return 1;
}

static int entry_final(vt_termcap_t *termcap) {
  int count;

  free(termcap->name);

  for (count = 0; count < MAX_TERMCAP_STR_FIELDS; count++) {
    free(termcap->str_fields[count]);
  }

  return 1;
}

static int parse_termcap_db(vt_termcap_t *termcap, char *termcap_db) {
  char *field;
  int count;

  while ((field = bl_str_sep(&termcap_db, ":"))) {
    char *key;
    char *value;

    key = bl_str_sep(&field, "=");

    if ((value = field) == NULL) {
      for (count = 0; count < MAX_TERMCAP_BOOL_FIELDS; count++) {
        if (strcmp(key, bool_field_table[count].name) == 0) {
          termcap->bool_fields[bool_field_table[count].field] = 1;

          break;
        }
      }
    } else {
      for (count = 0; count < MAX_TERMCAP_STR_FIELDS; count++) {
        if (strcmp(key, str_field_table[count].name) == 0) {
          if ((value = bl_str_unescape(value))) {
            free(termcap->str_fields[str_field_table[count].field]);
            termcap->str_fields[str_field_table[count].field] = value;
          }

          break;
        }
      }
    }
  }

  return 1;
}

/*
 * If perfect_match is 0 and name is "xterm-256color", this function can
 * return "xterm" termcap.
 */
static vt_termcap_t *search_termcap(const char *name, int perfect_match) {
  int count;
  int partial_match_count;
  size_t partial_match_len = 0;

  for (count = 0; count < num_entries; count++) {
    const char *p1;
    const char *p2;

    p1 = entries[count].name;
    p2 = name;

    while (1) {
      if (*p1 == '\0') {
        if (*p2 == '\0') {
          return &entries[count];
        } else if (!perfect_match && partial_match_len < p2 - name) {
          partial_match_len = p2 - name;
          partial_match_count = count;
        }

        break;
      } else if (*p1 == *p2) {
        p1++;
        p2++;
      } else {
        break;
      }
    }
  }

  if (partial_match_len > 0) {
    return &entries[partial_match_count];
  }

  return NULL;
}

static int read_conf(char *filename) {
  bl_file_t *from;
  char *line;
  size_t len;
  char *termcap_db;
  size_t db_len;

  if (!(from = bl_file_open(filename, "r"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  termcap_db = NULL;
  db_len = 0;

  while ((line = bl_file_get_line(from, &len))) {
    void *p;

    if (len == 0 || *line == '#') {
      /* empty line or comment out */
      continue;
    }

    while (*line == ' ' || *line == '\t') {
      line++;
      len--;
    }

    /* + 1 is for NULL terminator */
    if ((p = realloc(termcap_db, db_len + len + 1)) == NULL) {
      free(termcap_db);
      bl_file_close(from);

      return 0;
    }

    termcap_db = p;

    strncpy(&termcap_db[db_len], line, len);
    db_len += len;

    if (termcap_db[db_len - 1] == '\\') {
      db_len--;
    } else {
      vt_termcap_t *termcap;
      char *field;
      char *db_p;

      termcap_db[db_len] = '\0';
      db_p = termcap_db;

      if ((field = bl_str_sep(&db_p, ":"))) {
        if ((termcap = search_termcap(field, 1))) {
#if 0
          entry_final(termcap);
          entry_init(termcap, field);
#endif
          parse_termcap_db(termcap, db_p);
        } else if ((p = realloc(entries, sizeof(vt_termcap_t) * (num_entries + 1)))) {
          entries = p;
          termcap = &entries[num_entries];

          if (entry_init(termcap, field) && parse_termcap_db(termcap, db_p)) {
            num_entries++;
          }
        }
      }

      db_len = 0;
    }
  }

  free(termcap_db);

  bl_file_close(from);

  return 1;
}

static int termcap_init(void) {
  char *rcpath;

  if ((entries = malloc(sizeof(vt_termcap_t))) == NULL) {
    return 0;
  }

  if (!entry_init(entries, "*")) {
    return 0;
  }

  entries[0].bool_fields[TC_BCE] = 1;
  num_entries = 1;

  if ((rcpath = bl_get_sys_rc_path(tc_file))) {
    if (!read_conf(rcpath)) {
#if defined(__ANDROID__)
#define MAX_DB_LEN_IDX 2
      const char *db[] = {
          "k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~", "ut",
          "kh=\E[7~:@7=\E[8~:k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~:ut",
          "kb=^H:kD=^?:k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~",
      };

      const char *names[] = {
          "mlterm", "xterm", "rxvt", "kterm",
      };

      void *p;
      char *buf;

      if ((p = realloc(entries, sizeof(vt_termcap_t) * (BL_ARRAY_SIZE(db) + 1))) &&
          (buf = alloca(strlen(db[MAX_DB_LEN_IDX]) + 1))) {
        size_t count;

        entries = p;
        num_entries = BL_ARRAY_SIZE(db) + 1;

        for (count = 0; count < BL_ARRAY_SIZE(db); count++) {
          entry_init(entries + count + 1, names[count]);
          strcpy(buf, db[count]);
          parse_termcap_db(entries + count + 1, buf);
        }
      }
#endif
    }

    free(rcpath);
  }

  if ((rcpath = bl_get_user_rc_path(tc_file))) {
    read_conf(rcpath);
    free(rcpath);
  }

  return 1;
}

/* --- global functions --- */

vt_termcap_t *vt_termcap_get(const char *name) {
  vt_termcap_t *termcap;

  if (entries == NULL) {
    if (!termcap_init()) {
      return NULL;
    }
  }

  if ((termcap = search_termcap(name, 0))) {
    return termcap;
  }

  /* '*' */
  return entries;
}

void vt_termcap_final(void) {
  int count;

  for (count = 0; count < num_entries; count++) {
    entry_final(&entries[count]);
  }

  free(entries);
}

int vt_termcap_set_key_seq(vt_termcap_t *termcap, vt_special_key_t key, const char *str) {
  str_field_t field;

  if (key == SPKEY_DELETE) {
    field = TC_DELETE;
  } else if (key == SPKEY_BACKSPACE) {
    field = TC_BACKSPACE;
  } else {
    return 0;
  }

  free(termcap->str_fields[field]);
  termcap->str_fields[field] = strdup(str);

  return 1;
}

int vt_termcap_bce_is_enabled(vt_termcap_t *termcap) { return termcap->bool_fields[TC_BCE]; }

char *vt_termcap_special_key_to_seq(vt_termcap_t *termcap, vt_special_key_t key, int modcode,
                                    int is_app_keypad, int is_app_cursor_keys, int is_app_escape,
                                    int modify_cursor_keys, int modify_function_keys) {
  static char escseq[10];
  char *seq;
  char intermed_ch;
  char final_ch;
  int param;

  switch (key) {
    case SPKEY_DELETE:
      if (modcode || !(seq = termcap->str_fields[TC_DELETE])) {
        intermed_ch = '[';
        param = 3;
        final_ch = '~';
        break;
      } else {
        return seq;
      }

    case SPKEY_BACKSPACE:
      if (!(seq = termcap->str_fields[TC_BACKSPACE])) {
        seq = "\x7f";
      }

      return seq;

    case SPKEY_ESCAPE:
      if (is_app_escape) {
        return "\x1bO[";
      } else {
        return "\x1b";
      }

    case SPKEY_END:
      if (modcode || !is_app_cursor_keys || !(seq = termcap->str_fields[TC_END])) {
        intermed_ch = (is_app_cursor_keys && !modcode) ? 'O' : '[';
        param = modcode ? 1 : 0;
        final_ch = 'F';
        break;
      } else {
        return seq;
      }

    case SPKEY_HOME:
      if (modcode || !is_app_cursor_keys || !(seq = termcap->str_fields[TC_HOME])) {
        intermed_ch = (is_app_cursor_keys && !modcode) ? 'O' : '[';
        param = modcode ? 1 : 0;
        final_ch = 'H';
        break;
      } else {
        return seq;
      }

    case SPKEY_BEGIN:
      intermed_ch = '[';
      param = modcode ? 1 : 0;
      final_ch = 'E';
      break;

    case SPKEY_ISO_LEFT_TAB:
      intermed_ch = '[';
      param = 0;
      final_ch = 'Z';
      modcode = 0;
      break;

    default:
      if (key <= SPKEY_KP_F4) {
        if (is_app_keypad) {
          const char final_chs[] = {
              'j', /* MULTIPLY */
              'k', /* ADD */
              'l', /* SEPARATOR */
              'm', /* SUBTRACT */
              'n', /* DELETE */
              'o', /* DIVIDE */
              'q', /* END */
              'w', /* HOME */
              'u', /* BEGIN */
              'x', /* UP */
              'r', /* DOWN */
              'v', /* RIGHT */
              't', /* LEFT */
              'p', /* INSERT */
              'y', /* PRIOR */
              's', /* NEXT */
              'P', /* F1 */
              'Q', /* F2 */
              'R', /* F3 */
              'S', /* F4 */
          };

          intermed_ch = 'O';
          param = 0;
          final_ch = final_chs[key - SPKEY_KP_MULTIPLY];
        } else {
          if (key <= SPKEY_KP_DIVIDE) {
            return NULL;
          } else if (key <= SPKEY_KP_BEGIN) {
            key += (SPKEY_END - SPKEY_KP_END);
          } else if (key <= SPKEY_KP_LEFT) {
            key += (SPKEY_UP - SPKEY_KP_UP);
          } else if (key == SPKEY_KP_INSERT) {
            key = SPKEY_INSERT;
          } else if (key <= SPKEY_KP_F4) {
            key += (SPKEY_PRIOR - SPKEY_KP_PRIOR);
          } else {
            return NULL;
          }

          return vt_termcap_special_key_to_seq(termcap, key, modcode, is_app_keypad,
                                               is_app_cursor_keys, is_app_escape,
                                               modify_cursor_keys, modify_function_keys);
        }
      } else if (key <= SPKEY_LEFT) {
        intermed_ch = (is_app_cursor_keys && !modcode) ? 'O' : '[';
        param = modcode ? 1 : 0;
        final_ch = (key - SPKEY_UP) + 'A';
      } else if (key <= SPKEY_NEXT) {
        intermed_ch = '[';
        param = (key - SPKEY_FIND) + 1;
        final_ch = '~';
      } else if (key <= SPKEY_F5) {
        if (modcode || !(seq = termcap->str_fields[TC_F1 + key - SPKEY_F1])) {
          if (key == SPKEY_F5) {
            intermed_ch = '[';
            param = 15;
            final_ch = '~';
          } else {
            /* PQRS */
            if (modcode) {
              intermed_ch = '[';
              param = 1;
            } else {
              intermed_ch = 'O';
              param = 0;
            }
            final_ch = (key - SPKEY_F1) + 'P';
          }
        } else {
          return seq;
        }
      } else /* if( key <= SPKEY_F37) */ {
        const char params[] = {
            /* F6 - F15 */
            17, 18, 19, 20, 21, 23, 24, 25, 26, 28,
            /* F16 - F25 */
            29, 31, 32, 33, 34, 42, 43, 44, 45, 46,
            /* F26 - F35 */
            47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
            /* F36 - F37 */
            57, 58,

        };
        intermed_ch = '[';
        final_ch = '~';

        param = params[key - SPKEY_F6];
      }
  }

  if (modcode) /* ESC <intermed> Ps ; Ps <final> */ {
    if ('A' <= final_ch && final_ch <= 'H') {
      /* cursor keys */
      if (0 <= modify_cursor_keys && modify_cursor_keys <= 1) {
        goto obsolete_format;
      } else if (modify_cursor_keys == 3) {
        goto private_format;
      }
    } else if (SPKEY_IS_FKEY(key)) {
      if (modify_function_keys == 3) {
        goto private_format;
      } else if ('P' <= final_ch && final_ch <= 'S') {
        /* F1-F4 keys */
        if (modify_function_keys == 0) {
          intermed_ch = 'O';
          goto obsolete_format;
        } else if (modify_function_keys == 1) {
          goto obsolete_format;
        }
      }
    }

    while (1) {
      bl_snprintf(escseq, sizeof(escseq), "\x1b%c%d;%d%c", intermed_ch, param, modcode, final_ch);
      break;

    obsolete_format:
      bl_snprintf(escseq, sizeof(escseq), "\x1b%c%d%c", intermed_ch, modcode, final_ch);
      break;

    private_format:
      bl_snprintf(escseq, sizeof(escseq), "\x1b%c>%d;%d%c", intermed_ch, param, modcode, final_ch);
      break;
    }
  } else if (param) /* ESC <intermed> Ps <final> */ {
    bl_snprintf(escseq, sizeof(escseq), "\x1b%c%d%c", intermed_ch, param, final_ch);
  } else /* ESC <intermed> <final> */ {
    bl_snprintf(escseq, sizeof(escseq), "\x1b%c%c", intermed_ch, final_ch);
  }

  return escseq;
}

#ifdef BL_DEBUG

#include <assert.h>

void TEST_vt_termcap(void) {
  vt_termcap_t *orig_entries = entries;
  u_int orig_num_entries = num_entries;
  vt_termcap_t new_entries[10];
  vt_termcap_t *termcap;
  char seq1[] = "ut:k4=\E[14~";
  char seq2[] = "kh=\E[7~";
  char seq3[] = "kh=\E[7~";

  entries = new_entries;
  num_entries = 3;
  entry_init(&entries[0], "xterm");
  entry_init(&entries[1], "kterm");
  entry_init(&entries[2], "mlterm");

  assert((termcap = search_termcap("xterm", 1)) != NULL);
  assert(strcmp(termcap->name, "xterm") == 0);

  parse_termcap_db(termcap, seq1);
  assert(vt_termcap_bce_is_enabled(termcap));
  assert(strcmp(vt_termcap_special_key_to_seq(termcap, SPKEY_F4, 0, 0, 0, 0, 0, 0),
                "\x1b[14~") == 0);
  assert(strcmp(vt_termcap_special_key_to_seq(termcap, SPKEY_HOME, 0, 0, 1, 0, 0, 0),
                "\x1bOH") == 0);

  parse_termcap_db(termcap, seq2);
  assert(strcmp(vt_termcap_special_key_to_seq(termcap, SPKEY_HOME, 0, 0, 1, 0, 0, 0),
                "\x1b[7~") == 0);

  assert((termcap = search_termcap("kterm", 1)) != NULL);
  assert(strcmp(termcap->name, "kterm") == 0);

  parse_termcap_db(termcap, seq3);
  assert(vt_termcap_bce_is_enabled(termcap) == 0);
  assert(strcmp(vt_termcap_special_key_to_seq(termcap, SPKEY_HOME, 0, 0, 1, 0, 0, 0),
                "\x1b[7~") == 0);

  assert((termcap = search_termcap("mlterm", 1)) != NULL);
  assert(strcmp(termcap->name, "mlterm") == 0);
  assert(vt_termcap_bce_is_enabled(termcap) == 0);

  assert(search_termcap("mlterm1", 1) == NULL);

  assert((termcap = search_termcap("kterm-256color", 0)) != NULL);
  assert(strcmp(termcap->name, "kterm") == 0);

  assert((termcap = search_termcap("xterm-16color", 0)) != NULL);
  assert(strcmp(termcap->name, "xterm") == 0);

  assert((termcap = search_termcap("xterm2", 0)) != NULL);
  assert(strcmp(termcap->name, "xterm") == 0);

  assert(search_termcap("xter", 0) == NULL);

  assert(search_termcap("term", 0) == NULL);

  entries = orig_entries;
  num_entries = orig_num_entries;

  bl_msg_printf("PASS vt_termcap test.\n");
}

#endif
