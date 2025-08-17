/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_parser.h"

#include <stdio.h>    /* sprintf */
#include <strings.h>  /* strcasecmp */
#include <stdlib.h>   /* atoi */
#include <fcntl.h>    /* open */
#include <unistd.h>   /* write/getcwd */
#include <sys/time.h> /* gettimeofday */
#include <time.h>     /* clock */
#include <ctype.h>    /* iscntrl */
#ifdef DEBUG
#include <stdarg.h> /* va_list */
#endif

#if defined(USE_LIBSSH2) && !defined(USE_WIN32API)
/*
 * <wchar.h> has already been included without _XOPEN_SOURCE,
 * so #define _XOPEN_SOURCE here doesn't work.
 */
#if 0
#define _XOPEN_SOURCE
#include <wchar.h> /* wcwidth */
#else
int wcwidth(wchar_t c);
#endif
#endif

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>    /* malloc/free */
#include <pobl/bl_util.h>   /* DIGIT_STR_LEN */
#include <pobl/bl_conf_io.h>/* bl_get_user_rc_path */
#include <pobl/bl_str.h>    /* strdup */
#include <pobl/bl_args.h>
#include <pobl/bl_unistd.h>   /* bl_usleep */
#include <pobl/bl_locale.h>   /* bl_get_locale */
#include <mef/ef_ucs4_map.h> /* ef_map_to_ucs4 */
#include <mef/ef_ucs_property.h>
#include <mef/ef_locale_ucs4_map.h>
#include <mef/ef_ko_kr_map.h>

#include "vt_iscii.h"
#include "vt_str_parser.h"
#include "vt_transfer.h"
#include "vt_shape.h" /* vt_{set|get}_use_arabic_dynamic_comb */

#if defined(__CYGWIN__) || defined(__MSYS__)
#include "cygfile.h"
#endif

/*
 * kterm BUF_SIZE in ptyx.h is 4096.
 */
#define PTY_RD_BUFFER_SIZE 3072

#define CTL_BEL 0x07
#define CTL_BS 0x08
#define CTL_TAB 0x09
#define CTL_LF 0x0a
#define CTL_VT 0x0b
#define CTL_FF 0x0c
#define CTL_CR 0x0d
#define CTL_SO 0x0e
#define CTL_SI 0x0f
#define CTL_ESC 0x1b

#define CURRENT_STR_P(vt_parser) \
  ((vt_parser)->r_buf.chars + (vt_parser)->r_buf.filled_len - (vt_parser)->r_buf.left)

#define HAS_XTERM_LISTENER(vt_parser, method) \
  ((vt_parser)->xterm_listener && ((vt_parser)->xterm_listener->method))

#define HAS_CONFIG_LISTENER(vt_parser, method) \
  ((vt_parser)->config_listener && ((vt_parser)->config_listener->method))

#if 1
#define MAX_PS_DIGIT 0xffff
#endif

#if 0
#define EDIT_DEBUG
#endif

#if 0
#define EDIT_ROUGH_DEBUG
#endif

#if 0
#define INPUT_DEBUG
#endif

#if 0
#define ESCSEQ_DEBUG
#endif

#if 0
#define OUTPUT_DEBUG
#endif

#if 0
#define DUMP_HEX
#endif

#if 0
#define SUPPORT_VTE_CJK_WIDTH
#endif

#ifndef NO_IMAGE
#define SUPPORT_ITERM2_OSC1337
#endif

/* mode must be less than 64 */
#define MOD32(mode) ((mode) >= 32 ? (mode) - 32 : (mode))
#define DIV32(mode) ((mode) >= 32) /* 1 or 0 */
#define SHIFT_FLAG_0(mode) ((mode) >= 32 ? 0 : (1 << (mode)))
#define SHIFT_FLAG_1(mode) ((mode) >= 32 ? (1 << ((mode) - 32)) : 0)

/*
 * The default of DECMODE_1010 is 'set' (scroll to bottom on tty output) on xterm,
 * but is 'reset' on mlterm ("exit_backscroll_by_pty" option).
 */
#define INITIAL_VTMODE_FLAGS_0 \
  SHIFT_FLAG_0(DECMODE_2) | /* is_vt52_mode == 0 */ \
  SHIFT_FLAG_0(DECMODE_7) | /* auto_wrap == 1 (compatible with xterm, not with VT220) */ \
  SHIFT_FLAG_0(DECMODE_25) | /* is_visible_cursor == 1 */ \
  SHIFT_FLAG_0(DECMODE_1034) | /* mod_meta_mode = 8bit (compatible with xterm) */ \
  SHIFT_FLAG_0(VTMODE_12) | /* local echo is false */ \
  SHIFT_FLAG_0(VTMODE_33) /* Cursor mode is steady */
#define INITIAL_VTMODE_FLAGS_1 \
  SHIFT_FLAG_1(DECMODE_2) | /* is_vt52_mode == 0 */ \
  SHIFT_FLAG_1(DECMODE_7) | /* auto_wrap == 1 (compatible with xterm, not with VT220) */ \
  SHIFT_FLAG_1(DECMODE_25) | /* is_visible_cursor == 1 */ \
  SHIFT_FLAG_1(DECMODE_1034) | /* mod_meta_mode = 8bit (compatible with xterm) */ \
  SHIFT_FLAG_1(VTMODE_12) | /* local echo is false */ \
  SHIFT_FLAG_1(VTMODE_33) /* Cursor mode is steady */

#define SHIFT_FLAG(mode) (1 << MOD32(mode))
#define GET_VTMODE_FLAG(vt_parser, mode) \
  ((vt_parser)->vtmode_flags[DIV32(mode)] & SHIFT_FLAG(mode))
/* returns 1 or 0 */
#define GET_VTMODE_FLAG2(vt_parser, mode) \
  (((vt_parser)->vtmode_flags[DIV32(mode)] >> MOD32(mode)) & 1)
#define GET_SAVED_VTMODE_FLAG(vt_parser, mode) \
  ((vt_parser)->saved_vtmode_flags[DIV32(mode)] & SHIFT_FLAG(mode))
/* returns 1 or 0 */
#define GET_SAVED_VTMODE_FLAG2(vt_parser, mode) \
  (((vt_parser)->saved_vtmode_flags[DIV32(mode)] >> MOD32(mode)) & 1)

#define IS_APP_CURSOR_KEYS(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_1)
#define ALLOW_DECCOLM(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_40)
#define KEEP_SCREEN_ON_DECCOLM(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_95)
#define BOLD_AFFECTS_BG(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_116)
#define IS_APP_ESCAPE(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_7727)
#define CURSOR_TO_RIGHT_OF_SIXEL(vt_parser) GET_VTMODE_FLAG(vt_parser, DECMODE_8452)
#define SEND_RECV_MODE(vt_parser) GET_VTMODE_FLAG(vt_parser, VTMODE_12)
#define AUTO_CR(vt_parser) GET_VTMODE_FLAG(vt_parser, VTMODE_20)

#define VTMODE(mode) ((mode) + 10000)

#define destroy_drcs(drcs) \
  vt_drcs_destroy(drcs);   \
  (drcs) = NULL;

/*
 * If VTMODE_NUM >= 64, enlarge the size of vt_parser_t::vtmode_flags.
 * See get_initial_vtmode_flags() to check initial values of these modes.
 */
typedef enum {
  /* DECSET/DECRST */
  /* vtmode_flags[0] */
  DECMODE_1 = 0,
  DECMODE_2,
  DECMODE_3,
  DECMODE_5,
  DECMODE_6,
  DECMODE_7,
  DECMODE_25,
  DECMODE_40,
  DECMODE_47,
  DECMODE_66,
  DECMODE_67,
  DECMODE_69,
  DECMODE_80,
  DECMODE_95,
  DECMODE_116,
  DECMODE_117,
  DECMODE_1000,
  DECMODE_1002, /* Don't add an entry between 1000 and 1002 (see set_vtmode() idx - DECMODE_1000) */
  DECMODE_1003,
  DECMODE_1004,
  DECMODE_1005,
  DECMODE_1006,
  DECMODE_1015,
  DECMODE_1016, /* Don't add an entry between 1005 and 1016 (see set_vtmode() idx - DECMODE_1005) */
  DECMODE_1010,
  DECMODE_1034,
  DECMODE_1036,
  DECMODE_1042,
  DECMODE_1047,
  DECMODE_1048,
  DECMODE_1049,
  DECMODE_2004,

  /* vtmode_flags[1] */
  DECMODE_7727,
  DECMODE_8428,
  DECMODE_8452,
  DECMODE_8800,

  /* SM/RM */
  VTMODE_2,
  VTMODE_4,
  VTMODE_12,
  VTMODE_20,
  VTMODE_33,
  VTMODE_34,

  VTMODE_NUM,
} vtmode_t;

typedef struct area {
  u_int32_t min;
  u_int32_t max;

} area_t;

/* --- static variables --- */

static u_int16_t vtmodes[] = {
  /* DECSET/DECRST */
  1, 2, 3, 5, 6, 7, 25, 40, 47, 66, 67, 69, 80, 95, 116, 117,
  1000, 1002, /* Don't add an entry between 1000 and 1002 (see set_vtmode()) */
  1003, 1004,
  1005, 1006, 1015, 1016, /* Don't add an entry between 1005 and 1016 (see set_vtmode()) */
  1010, 1034, 1036,
  1042, 1047, 1048, 1049, 2004, 7727, 8428, 8452, 8800,

  /* SM/RM */
  VTMODE(2), VTMODE(4), VTMODE(12), VTMODE(20), VTMODE(33), VTMODE(34),
};

static int use_alt_buffer = 1;

static area_t *unicode_noconv_areas;
static u_int num_unicode_noconv_areas;

static area_t *full_width_areas;
static u_int num_full_width_areas;

static area_t *half_width_areas;
static u_int num_half_width_areas;

static char *auto_detect_encodings;
static struct {
  vt_char_encoding_t encoding;
  ef_parser_t *parser;

} * auto_detect;
static u_int num_auto_detect_encodings;

static int use_ttyrec_format;

static clock_t timeout_read_pty = CLOCKS_PER_SEC / 100; /* 0.01 sec */

static char *primary_da;
static char *secondary_da;

static int is_broadcasting;

static int old_drcs_sixel; /* Compatible behavior with RLogin 2.23.0 or before */

static u_int local_echo_wait_msec = 250;

#ifdef USE_LIBSSH2
static int use_scp_full;
#endif

static u_int8_t alt_color_idxs[] = { 0, 1, 2, 4, 8, 3, 5, 9, 6, 10, 12, 7, 11, 13, 14, 15, } ;

static char *send_file;
static char *recv_dir;

static int format_other_keys;

/* --- static functions --- */

#ifdef DEBUG
#if 0
#define debug_print_unknown bl_debug_printf
#else
static void debug_print_unknown(const char *format, ...) {
  va_list arg_list;

  va_start(arg_list, format);

  fprintf(stderr, BL_DEBUG_TAG " received unknown sequence ");
  vfprintf(stderr, format, arg_list);
}
#endif
#endif

/* XXX This function should be moved to pobl */
static void str_replace(char *str, int c1, int c2) {
  while (*str) {
    if (*str == c1) {
      *str = c2;
    }

    str++;
  }
}

static area_t *set_area_to_table(area_t *area_table, u_int *num, char *areas) {
  char *area;

#ifdef __DEBUG
  if (area_table == unicode_noconv_areas) {
    bl_debug_printf("Unicode noconv area:");
  } else if (area_table == full_width_areas) {
    bl_debug_printf("Unicode full width area:");
  } else {
    bl_debug_printf("Unicode half width area:");
  }
  bl_msg_printf(" parsing %s\n", areas);
#endif

  if (areas == NULL || *areas == '\0') {
    free(area_table);
    *num = 0;

    return NULL;
  } else {
    void *p;

    if (!(p = realloc(area_table, sizeof(*area_table) * (bl_count_char_in_str(areas, ',') + 2)))) {
      return area_table;
    }

    area_table = p;
  }

  *num = 0;

  while ((area = bl_str_sep(&areas, ","))) {
    u_int min;
    u_int max;

    if (vt_parse_unicode_area(area, &min, &max)) {
      u_int count = 0;

      while (1) {
        if (count == *num) {
          area_table[*num].min = min;
          area_table[(*num)++].max = max;
          break;
        }

        if (area_table[count].min <= min) {
          if (area_table[count].max + 1 >= min) {
            if (area_table[count].max < max) {
              area_table[count].max = max;
            }
            break;
          }
        } else {
          if (area_table[count].min <= max + 1) {
            if (area_table[count].min > min) {
              area_table[count].min = min;
            }
            if (area_table[count].max < max) {
              area_table[count].max = max;
            }
          } else {
            memmove(area_table + count + 1, area_table + count,
                    (*num - count) * sizeof(*area_table));
            area_table[count].max = max;
            area_table[count].min = min;
            (*num)++;
          }
          break;
        }

        count++;
      }
    }
  }

#ifdef __DEBUG
  {
    u_int count;

    for (count = 0; count < *num; count++) {
      bl_debug_printf("AREA %x-%x\n", area_table[count].min, area_table[count].max);
    }
  }
#endif

  return area_table;
}

static void response_area_table(vt_pty_t *pty, const u_char *key, area_t *area_table, u_int num,
                                int to_menu) {
  u_char *value;

  /* 20: U+FFFFFFFF-FFFFFFFF, */
  if (num > 0 && (value = alloca(20 * num))) {
    u_int count;
    u_char *p;

    p = value;
    count = 0;
    while (1) {
      sprintf(p, area_table[count].min == area_table[count].max ? "U+%x" : "U+%x-%x",
              area_table[count].min, area_table[count].max);
      p += strlen(p);
      if (++count < num) {
        *(p++) = ',';
      } else {
        break;
      }
    }
  } else {
    value = "";
  }

  vt_response_config(pty, key, value, to_menu);
}

static inline int hit_area(area_t *areas, u_int num, u_int code) {
  if (num > 0 && areas[0].min <= code && code <= areas[num - 1].max) {
    u_int count;

    if (num == 1) {
      return 1;
    }
    count = 0;
    do {
      if (areas[count].min <= code && code <= areas[count].max) {
        return 1;
      }
    } while (++count < num);
  }

  return 0;
}

static inline int is_noconv_unicode(u_char *ch) {
  if (unicode_noconv_areas || ch[2] == 0x20) {
    u_int32_t code = ef_bytes_to_int(ch, 4);

    if (hit_area(unicode_noconv_areas, num_unicode_noconv_areas, code)) {
      return 1;
    }

    /*
     * Don't convert these characters in order not to show them.
     * see vt_char_cols().
     */
    if ((0x200c <= code && code <= 0x200f) || (0x202a <= code && code <= 0x202e)) {
      return 1;
    }
  }

  return 0;
}

static inline ef_property_t modify_ucs_property(u_int32_t code, int col_size_of_width_a,
                                                ef_property_t prop) {
  if (prop & EF_AWIDTH) {
#ifdef SUPPORT_VTE_CJK_WIDTH
    char *env;
#endif

    if (col_size_of_width_a == 2) {
      prop |= EF_FULLWIDTH;
    }
#ifdef SUPPORT_VTE_CJK_WIDTH
    else if ((env = getenv("VTE_CJK_WIDTH")) &&
             (strcmp(env, "wide") == 0 || strcmp(env, "1") == 0)) {
      prop |= EF_FULLWIDTH;
    }
#endif
  }

  if (prop & EF_FULLWIDTH) {
    if (half_width_areas && hit_area(half_width_areas, num_half_width_areas, code)) {
      return prop & ~EF_FULLWIDTH;
    }
  } else {
    if (full_width_areas && hit_area(full_width_areas, num_full_width_areas, code)) {
      return prop | EF_FULLWIDTH;
    }
  }

  return prop;
}

static void start_vt100_cmd(vt_parser_t *vt_parser,
                            int trigger_xterm_event /* dispatch to x_screen or not. */
                            ) {
  vt_set_use_multi_col_char(vt_parser->use_multi_col_char);

  if (trigger_xterm_event && HAS_XTERM_LISTENER(vt_parser, start)) {
    /*
     * XXX Adhoc implementation.
     * Converting visual -> logical in xterm_listener->start.
     */
    (*vt_parser->xterm_listener->start)(vt_parser->xterm_listener->self);
  } else {
    vt_screen_logical(vt_parser->screen);
  }
}

static void stop_vt100_cmd(vt_parser_t *vt_parser,
                           int trigger_xterm_event /* dispatch to x_screen or not. */
                           ) {
  vt_screen_render(vt_parser->screen);
  vt_screen_visual(vt_parser->screen);

  if (trigger_xterm_event && HAS_XTERM_LISTENER(vt_parser, stop)) {
    (*vt_parser->xterm_listener->stop)(vt_parser->xterm_listener->self);
  }
}

static void interrupt_vt100_cmd(vt_parser_t *vt_parser) {
  if (HAS_XTERM_LISTENER(vt_parser, interrupt)) {
    vt_screen_render(vt_parser->screen);
    vt_screen_visual(vt_parser->screen);

    (*vt_parser->xterm_listener->interrupt)(vt_parser->xterm_listener->self);

    vt_screen_logical(vt_parser->screen);
  }
}

static int change_read_buffer_size(vt_read_buffer_t *r_buf, size_t len) {
  void *p;

  if (!(p = realloc(r_buf->chars, len))) {
    return 0;
  }

  r_buf->chars = p;
  r_buf->len = len;

  /*
   * Not check if r_buf->left and r_buf->filled_len is larger than r_buf->len.
   * It should be checked before calling this function.
   */

  return 1;
}

static char* get_now_suffix(char *now /* 16 bytes */) {
  time_t t;
  struct tm *tm;

  time(&t);
  tm = localtime(&t);

  sprintf(now, "-%04d%02d%02d%02d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          tm->tm_hour, tm->tm_min, tm->tm_sec);

  return now;
}

static char *get_home_file_path(const char *prefix, const char *name, const char *suffix) {
  char *file_name;

  if (!(file_name = alloca(7 + strlen(prefix) + 1 + strlen(name) + 1 + strlen(suffix) + 1))) {
    return NULL;
  }

  sprintf(file_name, "mlterm/%s%s.%s", prefix, name, suffix);
  str_replace(file_name + 7, '/', '_');

  return bl_get_user_rc_path(file_name);
}

/*
 * 0: Error
 * 1: No error
 * >=2: Probable
 */
static int parse_string(ef_parser_t *cc_parser, u_char *str, size_t len) {
  ef_char_t ch;
  int ret;
  u_int nfull;
  u_int nkana;

  ret = 1;
  nfull = 0;
  nkana = 0;
  (*cc_parser->init)(cc_parser);
  (*cc_parser->set_str)(cc_parser, str, len);

  while (1) {
    if (!(*cc_parser->next_char)(cc_parser, &ch)) {
      if (cc_parser->is_eos) {
        if (nkana * 8 > nfull) {
          /* kana is over 12.5%. */
          ret = 2;
        }

        return ret;
      } else {
        if (((str[len - cc_parser->left]) & 0x7f) <= 0x1f) {
          /* skip C0 or C1 */
          ef_parser_increment(cc_parser);
        } else {
          return 0;
        }
      }
    } else if (ch.size > 1) {
      if (ch.cs == ISO10646_UCS4_1) {
        if (ret == 1 && (ch.property & EF_FULLWIDTH)) {
          ret = 2;
        }
      } else {
        if (IS_CS94MB(ch.cs)) {
          if (ch.ch[0] <= 0x20 || ch.ch[0] == 0x7f || ch.ch[1] <= 0x20 || ch.ch[1] == 0x7f) {
            /* mef can return illegal character code. */
            return 0;
          } else if (ret == 1 && (ch.cs == JISX0208_1983 || ch.cs == JISC6226_1978 ||
                                  ch.cs == JISX0213_2000_1) &&
                     (ch.ch[0] == 0x24 || ch.ch[0] == 0x25) && 0x21 <= ch.ch[1] &&
                     ch.ch[1] <= 0x73) {
            /* Hiragana/Katakana */
            nkana++;
          }
        }

        nfull++;
      }
    }
  }
}

/* Check num_auto_detect_encodings > 0 before calling this function. */
static void detect_encoding(vt_parser_t *vt_parser) {
  u_char *str;
  size_t len;
  size_t count;
  u_int idx;
  int cur_idx;
  int cand_idx;
  int threshold;

  str = vt_parser->r_buf.chars;
  len = vt_parser->r_buf.filled_len;

  for (count = 0; count < len - 1; count++) {
    if (str[count] >= 0x80 && str[count + 1] >= 0x80) {
      goto detect;
    }
  }

  return;

detect:
  cur_idx = -1;
  threshold = 0;
  for (idx = 0; idx < num_auto_detect_encodings; idx++) {
    if (auto_detect[idx].encoding == vt_parser->encoding) {
      if ((threshold = parse_string(auto_detect[idx].parser, str, len)) > 1) {
        return;
      }

      cur_idx = idx;
      break;
    }
  }

  cand_idx = -1;
  for (idx = 0; idx < num_auto_detect_encodings; idx++) {
    int ret;

    if (idx != cur_idx && (ret = parse_string(auto_detect[idx].parser, str, len)) > threshold) {
      cand_idx = idx;

      if (ret > 1) {
        break;
      }
    }
  }

  if (cand_idx >= 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Character encoding is changed to %s.\n",
                    vt_get_char_encoding_name(auto_detect[cand_idx].encoding));
#endif

    vt_parser_change_encoding(vt_parser, auto_detect[cand_idx].encoding);
  }
}

inline static int is_dcs_or_osc(u_char *str /* The length should be 2 or more. */
                                ) {
  return *str == 0x90 || memcmp(str, "\x1bP", 2) == 0 || memcmp(str, "\x1b]", 2) == 0;
}

static void write_ttyrec_header(int fd, size_t len, int keep_time) {
  u_int32_t buf[3];

#ifdef HAVE_GETTIMEOFDAY
  if (!keep_time) {
    struct timeval tval;

    gettimeofday(&tval, NULL);
#ifdef WORDS_BIGENDIAN
    buf[0] = LE32DEC(((u_char *)&tval.tv_sec));
    buf[1] = LE32DEC(((u_char *)&tval.tv_usec));
    buf[2] = LE32DEC(((u_char *)&len));
#else
    buf[0] = tval.tv_sec;
    buf[1] = tval.tv_usec;
    buf[2] = len;
#endif

#if __DEBUG
    bl_debug_printf("write len %d at %d\n", len, lseek(fd, 0, SEEK_CUR));
#endif

    write(fd, buf, 12);
  } else
#endif
  {
    lseek(fd, 8, SEEK_CUR);

#ifdef WORDS_BIGENDIAN
    buf[0] = LE32DEC(((u_char *)&len));
#else
    buf[0] = len;
#endif

    write(fd, buf, 4);
  }
}

static int receive_bytes(vt_parser_t *vt_parser) {
  size_t len;

  if (vt_parser->r_buf.left == vt_parser->r_buf.len) {
    /* Buffer is full => Expand buffer */

    len = vt_parser->r_buf.len >= PTY_RD_BUFFER_SIZE * 5 ? PTY_RD_BUFFER_SIZE * 10
                                                            : PTY_RD_BUFFER_SIZE;

    if (!change_read_buffer_size(&vt_parser->r_buf, vt_parser->r_buf.len + len)) {
      return 0;
    }
  } else {
    if (0 < vt_parser->r_buf.left && vt_parser->r_buf.left < vt_parser->r_buf.filled_len) {
      memmove(vt_parser->r_buf.chars, CURRENT_STR_P(vt_parser),
              vt_parser->r_buf.left * sizeof(u_char));
    }

    /* vt_parser->r_buf.left must be always less than vt_parser->r_buf.len
     */
    if ((len = vt_parser->r_buf.len - vt_parser->r_buf.left) > PTY_RD_BUFFER_SIZE &&
        !is_dcs_or_osc(vt_parser->r_buf.chars)) {
      len = PTY_RD_BUFFER_SIZE;
    }
  }

  if ((vt_parser->r_buf.new_len = vt_read_pty(
           vt_parser->pty, vt_parser->r_buf.chars + vt_parser->r_buf.left, len)) == 0) {
    vt_parser->r_buf.filled_len = vt_parser->r_buf.left;

    return 0;
  }

  if (vt_parser->logging_vt_seq) {
    if (vt_parser->log_file == -1) {
      char *path;
      char buf[16];

      if (!(path = get_home_file_path(vt_pty_get_slave_name(vt_parser->pty) + 5,
                                      get_now_suffix(buf), "log"))) {
        goto end;
      }

      if ((vt_parser->log_file = open(path, O_CREAT | O_WRONLY, 0600)) == -1) {
        free(path);

        goto end;
      }

      free(path);

      /*
       * O_APPEND in open() forces lseek(0,SEEK_END) in write()
       * and disables lseek(pos,SEEK_SET) before calling write().
       * So don't specify O_APPEND in open() and call lseek(0,SEEK_END)
       * manually after open().
       */
      lseek(vt_parser->log_file, 0, SEEK_END);
      bl_file_set_cloexec(vt_parser->log_file);

      if (use_ttyrec_format) {
        char seq[6 + DIGIT_STR_LEN(int)*2 + 1];

        /* The height of "CSI 8 t" doesn't include status line. */
        sprintf(seq, "\x1b[8;%d;%dt", vt_screen_get_logical_rows(vt_parser->screen),
                vt_screen_get_logical_cols(vt_parser->screen));
        write_ttyrec_header(vt_parser->log_file, strlen(seq), 0);
        write(vt_parser->log_file, seq, strlen(seq));
      }
    }

    if (use_ttyrec_format) {
      if (vt_parser->r_buf.left > 0) {
        lseek(vt_parser->log_file,
              lseek(vt_parser->log_file, 0, SEEK_CUR) - vt_parser->r_buf.filled_len - 12,
              SEEK_SET);

        if (vt_parser->r_buf.left < vt_parser->r_buf.filled_len) {
          write_ttyrec_header(vt_parser->log_file,
                              vt_parser->r_buf.filled_len - vt_parser->r_buf.left, 1);

          lseek(vt_parser->log_file,
                lseek(vt_parser->log_file, 0, SEEK_CUR) + vt_parser->r_buf.filled_len -
                    vt_parser->r_buf.left,
                SEEK_SET);
        }
      }

      write_ttyrec_header(vt_parser->log_file,
                          vt_parser->r_buf.left + vt_parser->r_buf.new_len, 0);

      write(vt_parser->log_file, vt_parser->r_buf.chars,
            vt_parser->r_buf.left + vt_parser->r_buf.new_len);
    } else {
      write(vt_parser->log_file, vt_parser->r_buf.chars + vt_parser->r_buf.left,
            vt_parser->r_buf.new_len);
    }

#ifndef USE_WIN32API
    fsync(vt_parser->log_file);
#endif
  } else {
    if (vt_parser->log_file != -1) {
      close(vt_parser->log_file);
      vt_parser->log_file = -1;
    }
  }

end:
  vt_parser->r_buf.filled_len = (vt_parser->r_buf.left += vt_parser->r_buf.new_len);

  if (vt_parser->r_buf.filled_len <= PTY_RD_BUFFER_SIZE &&
      vt_parser->r_buf.len > PTY_RD_BUFFER_SIZE) {
    /* Shrink buffer */
    change_read_buffer_size(&vt_parser->r_buf, PTY_RD_BUFFER_SIZE);
  }

  if (vt_parser->use_auto_detect && num_auto_detect_encodings > 0) {
    detect_encoding(vt_parser);
  }

#ifdef INPUT_DEBUG
  {
    size_t count;

    bl_debug_printf(BL_DEBUG_TAG " pty msg (len %d) is received:", vt_parser->r_buf.left);

    for (count = 0; count < vt_parser->r_buf.left; count++) {
#ifdef DUMP_HEX
      if (isprint(vt_parser->r_buf.chars[count])) {
        bl_msg_printf("%c ", vt_parser->r_buf.chars[count]);
      } else {
        bl_msg_printf("%.2x ", vt_parser->r_buf.chars[count]);
      }
#else
      bl_msg_printf("%c", vt_parser->r_buf.chars[count]);
#endif
    }

    bl_msg_printf("[END]\n");
  }
#endif

  return 1;
}

/*
 * If buffer exists, vt_parser->w_buf.last_ch is cached.
 * If buffer doesn't exist, vt_parser->w_buf.last_ch is cleared.
 */
static int flush_buffer(vt_parser_t *vt_parser) {
  vt_write_buffer_t *buffer;

  buffer = &vt_parser->w_buf;

  if (buffer->filled_len == 0) {
    return 0;
  }

#ifdef OUTPUT_DEBUG
  {
    u_int count;

    bl_msg_printf("\nflushing chars(%d)...==>", buffer->filled_len);
    for (count = 0; count < buffer->filled_len; count++) {
      u_int code = vt_char_code(&buffer->chars[count]);

#ifdef DUMP_HEX
      bl_msg_printf("%x", code);
#else
      bl_msg_printf("%c", code);
#endif
    }

    bl_msg_printf("<===\n");
  }
#endif

  (*buffer->output_func)(vt_parser->screen, buffer->chars, buffer->filled_len);

  /* last_ch which will be used & cleared in REP sequence is cached. */
  buffer->last_ch = &buffer->chars[buffer->filled_len - 1];
  /* buffer is cleared. */
  buffer->filled_len = 0;

#ifdef EDIT_DEBUG
  vt_edit_dump(vt_parser->screen->edit);
#endif

  return 1;
}

static void put_char(vt_parser_t *vt_parser, u_int32_t ch, ef_charset_t cs,
                     ef_property_t prop) {
  vt_color_t fg_color;
  vt_color_t bg_color;
  int is_fullwidth;
  int is_awidth;
  int is_comb;
  int is_bold;
  int is_italic;
  int line_style;
  int is_blinking;
  int is_protected;
  int is_reversed;

  if (vt_parser->w_buf.filled_len == PTY_WR_BUFFER_SIZE) {
    flush_buffer(vt_parser);
  }

  /*
   * checking width property of the char.
   */

  if (prop & EF_FULLWIDTH) {
    is_fullwidth = 1;
  } else {
    is_fullwidth = 0;
  }

  if (prop & EF_AWIDTH) {
    is_awidth = 1;
  } else {
    is_awidth = 0;
  }

#ifdef __DEBUG
  bl_debug_printf("%x %d %x => %s\n", ch, len, cs, is_fullwidth ? "Fullwidth" : "Single");
#endif

  if ((prop & EF_COMBINING)
#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
      || (ch == '\xe9' && IS_ISCII(cs)) /* nukta is always combined. */
#endif
      ) {
    is_comb = 1;
  } else {
    is_comb = 0;
  }

  fg_color = vt_parser->fg_color;
  bg_color = vt_parser->bg_color;
  is_italic = vt_parser->is_italic ? 1 : 0;
  is_blinking = vt_parser->is_blinking ? 1 : 0;
  is_protected = vt_parser->is_protected ? 1 : 0;
  is_reversed = vt_parser->is_reversed ? 1 : 0;
  line_style = vt_parser->line_style;
  if (cs == ISO10646_UCS4_1 && 0x2580 <= ch && ch <= 0x259f) {
    /* prevent these block characters from being drawn doubly. */
    is_bold = 0;
  } else {
    is_bold = vt_parser->is_bold ? 1 : 0;
  }

  if (fg_color == VT_FG_COLOR) {
    if (is_italic && (vt_parser->alt_color_mode & ALT_COLOR_ITALIC)) {
      is_italic = 0;
      fg_color = VT_ITALIC_COLOR;
    }

    if ((line_style & LS_CROSSED_OUT) && (vt_parser->alt_color_mode & ALT_COLOR_CROSSED_OUT)) {
      line_style &= ~LS_CROSSED_OUT;
      fg_color = VT_CROSSED_OUT_COLOR;
    }

    if (is_blinking && (vt_parser->alt_color_mode & ALT_COLOR_BLINKING)) {
      is_blinking = 0;
      fg_color = VT_BLINKING_COLOR;
    }

    if ((line_style & LS_UNDERLINE) && (vt_parser->alt_color_mode & ALT_COLOR_UNDERLINE)) {
      line_style &= ~LS_UNDERLINE;
      fg_color = VT_UNDERLINE_COLOR;
    }

    if (is_bold && (vt_parser->alt_color_mode & ALT_COLOR_BOLD)) {
      is_bold = 0;
      fg_color = VT_BOLD_COLOR;
    }

    if (is_reversed && (vt_parser->alt_color_mode & ALT_COLOR_REVERSE)) {
      is_reversed = 0;
      fg_color = VT_REVERSE_COLOR;
    }
  } else {
    if (is_bold) {
      if (IS_VTSYS_BASE_COLOR(fg_color)) {
        fg_color |= VT_BOLD_COLOR_MASK;
      }
      if (BOLD_AFFECTS_BG(vt_parser) && IS_VTSYS_BASE_COLOR(bg_color)) {
        bg_color |= VT_BOLD_COLOR_MASK;
      }
    }
  }

  if (is_reversed) {
    vt_color_t tmp = bg_color;
    bg_color = fg_color;
    fg_color = tmp;
  }

  if (vt_parser->alt_colors.flags) {
    int idx = is_bold | ((vt_parser->is_reversed ? 1 : 0) << 1) |
              ((line_style & LS_UNDERLINE) ? 4 : 0) | (is_blinking << 3);

    if (/* idx < 16 && */ (vt_parser->alt_colors.flags & (1 << idx))) {
      fg_color = vt_parser->alt_colors.fg[idx];
      bg_color = vt_parser->alt_colors.bg[idx];
    }
  }

  /*
   * 2.9.2 Alternate Text Colors in VT520-VT525ProgrammerInformation.pdf
   * "The foreground color of text with the invisible attribute becomes the background color."
   */
  if (vt_parser->is_invisible) {
    fg_color = bg_color;
  }

  if (is_comb) {
#ifndef NO_IMAGE
    if ((ch & ~0x01) == 0xfe0e) {
      vt_char_t *prev_ch;

      if (vt_parser->w_buf.filled_len < 1) {
        prev_ch = vt_screen_get_n_prev_char(vt_parser->screen, 1);
      } else {
        prev_ch = &vt_parser->w_buf.chars[vt_parser->w_buf.filled_len - 1];
      }

      if (ch == 0xfe0f) {
        (*vt_parser->xterm_listener->get_emoji_data)(vt_parser->xterm_listener->self,
                                                     prev_ch, NULL);
      } else {
        /* 0xfe0e */
        vt_char_unset_picture(prev_ch);
      }

      if (vt_parser->w_buf.filled_len < 1) {
        vt_screen_unhighlight_cursor(vt_parser->screen);
      }
    }
#endif

    if (vt_parser->use_char_combining) {
      if (vt_parser->w_buf.filled_len == 0) {
        /*
         * vt_line_set_modified() is done in vt_screen_combine_with_prev_char()
         * internally.
         */
        if (vt_screen_combine_with_prev_char(vt_parser->screen, ch, cs, is_fullwidth, is_awidth,
                                             is_comb, fg_color, bg_color, is_bold, is_italic,
                                             line_style, is_blinking, is_protected)) {
          return;
        }
      } else {
        if (vt_char_combine(&vt_parser->w_buf.chars[vt_parser->w_buf.filled_len - 1], ch, cs,
                            is_fullwidth, is_awidth, is_comb, fg_color, bg_color, is_bold,
                            is_italic, line_style, is_blinking, is_protected)) {
          return;
        }
      }

      /*
       * if combining failed , char is normally appended.
       */
    }
  }

#ifndef NO_IMAGE
  {
    vt_drcs_font_t *font;
    int pic_id;
    int pic_pos;

    if (cs != US_ASCII &&
        (font = vt_drcs_get_font(vt_parser->drcs,
                                 cs == CS_REVISION_1(US_ASCII) ? US_ASCII : cs, 0)) &&
        /* XXX The width of pua is always regarded as 1. (is_fullwidth is ignored) */
        vt_drcs_get_picture(font, &pic_id, &pic_pos, ch)) {
      vt_char_copy(&vt_parser->w_buf.chars[vt_parser->w_buf.filled_len],
                   vt_screen_get_bce_ch(vt_parser->screen));
      vt_char_combine_picture(&vt_parser->w_buf.chars[vt_parser->w_buf.filled_len++],
                              pic_id, pic_pos);

      return;
    }
  }
#endif

  vt_char_set(&vt_parser->w_buf.chars[vt_parser->w_buf.filled_len++], ch, cs, is_fullwidth,
              is_awidth, is_comb, fg_color, bg_color, is_bold, is_italic, line_style,
              is_blinking, is_protected);

  if (cs != ISO10646_UCS4_1) {
    return;
  }

  if (0x1f000 <= ch && ch <= 0x1f9ff && (ch <= 0x1f6ff || 0x1f900 <= ch) &&
      HAS_XTERM_LISTENER(vt_parser, get_emoji_data)) {
    /*
     * Emoji pictures (mostly U+1F000-1F6FF) provided by
     * https://github.com/github/gemoji
     */

    vt_char_t *emoji1;
    vt_char_t *emoji2;
    int regional_indicator = 0;

    emoji1 = &vt_parser->w_buf.chars[vt_parser->w_buf.filled_len - 1];
    emoji2 = NULL;

    if (0x1f1e6 <= ch && ch <= 0x1f1ff /* REGIONAL INDICATOR SYMBOL LETTER A-Z */) {
      vt_char_t *prev_ch;

      if (vt_parser->w_buf.filled_len <= 1) {
        prev_ch = vt_screen_get_n_prev_char(vt_parser->screen, 1);
      } else {
        prev_ch = emoji1 - 1;
      }

      if (prev_ch && 0x1f1e6 <= vt_char_code(prev_ch) && vt_char_code(prev_ch) <= 0x1f1ff) {
        emoji2 = emoji1;
        emoji1 = prev_ch;

        /*
         * REGIONAL INDICATOR SYMBOL LETTER is half width by default,
         * but forcibly set to full width. (emoji2 is combined to emoji1)
         */
        vt_char_set_fullwidth(emoji1, 1);
        regional_indicator = 1;
      }
    } else if (0x1f3fb <= ch && ch <= 0x1f3ff) /* EMOJI MODIFIER FITZPATRIC TYPE 1-6 */ {
      vt_char_t *prev_ch;

      if (vt_parser->w_buf.filled_len <= 1) {
        prev_ch = vt_screen_get_n_prev_char(vt_parser->screen, 1);
      } else {
        prev_ch = emoji1 - 1;
      }

      if (prev_ch) {
        emoji2 = emoji1;
        emoji1 = prev_ch;
      }
    }

    if ((*vt_parser->xterm_listener->get_emoji_data)(vt_parser->xterm_listener->self,
                                                     emoji1, emoji2)) {
      if (emoji2) {
        if (regional_indicator) {
          /*
           * XXX
           * The property of emoji2 is not EF_COMBINING, but it is forcibly combined
           * to emoji1.
           *
           * Base char: emoji1, Comb1: picture, Comb2: emoji2
           */
          if (emoji2 == emoji1 + 1) {
            vt_char_combine(emoji1, ch, cs, is_fullwidth, is_awidth, is_comb, fg_color, bg_color,
                            is_bold, is_italic, line_style, is_blinking, is_protected);
          } else {
            /*
             * vt_line_set_modified() is done in
             * vt_screen_combine_with_prev_char() internally.
             */
            vt_screen_combine_with_prev_char(vt_parser->screen, ch, cs, is_fullwidth, is_awidth,
                                             is_comb, fg_color, bg_color, is_bold, is_italic,
                                             line_style, is_blinking, is_protected);
          }
          vt_parser->w_buf.filled_len--;
        } else {
          /*
           * The property of EMOJI MODIFIER FITZPATRIC TYPE 1-6 is EF_FULLWIDTH.
           * Don't draw emoji2 by setting is_zerowidth to 1.
           */
          vt_char_set_zerowidth(emoji2, 1);
        }
      }

      /* Flush buffer before searching and deleting unused pictures in x_picture.c. */
      flush_buffer(vt_parser);
    } else {
      if (emoji2) {
        if (regional_indicator) {
          /* revert to the original value. */
          vt_char_set_fullwidth(emoji1, 0);
        }

        if ((*vt_parser->xterm_listener->get_emoji_data)(vt_parser->xterm_listener->self,
                                                         emoji2, NULL)) {
          flush_buffer(vt_parser);
        }
      }
    }
  }
}

static void push_to_saved_names(vt_saved_names_t *saved, char *name) {
  void *p;

  if (!(p = realloc(saved->names, (saved->num + 1) * sizeof(name)))) {
    return;
  }

  saved->names = p;
  saved->names[saved->num++] = name ? strdup(name) : NULL;
}

static char *pop_from_saved_names(vt_saved_names_t *saved) {
  char *name;

  name = saved->names[--saved->num];

  if (saved->num == 0) {
    free(saved->names);
    saved->names = NULL;
  }

  return name;
}

/*
 * VT100_PARSER Escape Sequence Commands.
 */

static void save_cursor(vt_parser_t *vt_parser) {
  vt_storable_states_t *dest;

  dest = (vt_screen_is_alternative_edit(vt_parser->screen)) ? &(vt_parser->saved_alternate)
                                                               : &(vt_parser->saved_normal);
  dest->is_saved = 1;
  dest->fg_color = vt_parser->fg_color;
  dest->bg_color = vt_parser->bg_color;
  dest->is_bold = vt_parser->is_bold;
  dest->is_italic = vt_parser->is_italic;
  dest->line_style = vt_parser->line_style;
  dest->is_reversed = vt_parser->is_reversed;
  dest->is_blinking = vt_parser->is_blinking;
  dest->is_invisible = vt_parser->is_invisible;
  dest->is_protected = vt_parser->is_protected;
  dest->is_relative_origin = vt_screen_is_relative_origin(vt_parser->screen);
  dest->last_column_flag = vt_screen_get_last_column_flag(vt_parser->screen);
  dest->cs = vt_parser->cs;

  vt_screen_save_cursor(vt_parser->screen);
}

static void init_encoding_parser(vt_parser_t *vt_parser) {
  (*vt_parser->cc_parser->init)(vt_parser->cc_parser);
  vt_parser->gl = US_ASCII;
  vt_parser->g0 = US_ASCII;
  vt_parser->g1 = US_ASCII;
  vt_parser->is_so = 0;
}

static void set_vtmode(vt_parser_t *vt_parser, int mode, int flag);
static void change_char_attr(vt_parser_t *vt_parser, int flag);

static void restore_cursor(vt_parser_t *vt_parser) {
  vt_storable_states_t *src;

  src = (vt_screen_is_alternative_edit(vt_parser->screen)) ? &(vt_parser->saved_alternate)
                                                              : &(vt_parser->saved_normal);
  if (src->is_saved) {
    vt_parser->fg_color = src->fg_color;
    vt_screen_set_bce_fg_color(vt_parser->screen, src->fg_color);
    vt_parser->bg_color = src->bg_color;
    vt_screen_set_bce_bg_color(vt_parser->screen, src->bg_color);
    vt_parser->is_bold = src->is_bold;
    vt_parser->is_italic = src->is_italic;
    vt_parser->line_style = src->line_style;
    vt_parser->is_reversed = src->is_reversed;
    vt_parser->is_blinking = src->is_blinking;
    vt_parser->is_invisible = src->is_invisible;
    vt_parser->is_protected = src->is_protected;
    vt_screen_set_relative_origin(vt_parser->screen, src->is_relative_origin ? 1 : 0);
    /* See "Last Column Flag specifics" at https://github.com/mattiase/wraptest */
    vt_screen_set_last_column_flag(vt_parser->screen, src->last_column_flag ? 1 : 0);
    if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
      if ((src->cs == DEC_SPECIAL) && (src->cs != vt_parser->cs)) {
        /* force grapchics mode by sending \E(0 to current parser*/
        u_char DEC_SEQ[] = {CTL_ESC, '(', '0'};
        ef_char_t ch;
        ef_parser_t *parser;

        init_encoding_parser(vt_parser);
        parser = vt_parser->cc_parser;
        (*parser->set_str)(parser, DEC_SEQ, sizeof(DEC_SEQ));
        (*parser->next_char)(parser, &ch);
      }
    } else {
      /* XXX: what to do for g0/g1? */
      if (src->cs == DEC_SPECIAL) {
        vt_parser->gl = DEC_SPECIAL;
      } else {
        vt_parser->gl = US_ASCII;
      }
    }

    vt_screen_restore_cursor(vt_parser->screen);
  } else {
    /*
     * Moves the cursor to the home position (upper left of screen).
     * Resets origin mode (DECOM).
     * Turns all character attributes off (normal setting).
     * Maps the ASCII character set into GL, and the DEC Supplemental Graphic set into GR.
     * (see https://www.vt100.net/docs/vt510-rm/DECRC.html)
     */
    change_char_attr(vt_parser, 0);
    set_vtmode(vt_parser, 6, 0); /* goto(0, 0) internally */
  }
}

static void set_maximize(vt_parser_t *vt_parser, int flag) {
  if (HAS_XTERM_LISTENER(vt_parser, resize)) {
    stop_vt100_cmd(vt_parser, 0);
    (*vt_parser->xterm_listener->resize)(vt_parser->xterm_listener->self, 0, 0, flag, 0);
    start_vt100_cmd(vt_parser, 0);
  }
}

static int get_cell_size(vt_parser_t *vt_parser, u_int *col_width, u_int *line_height) {
  if (HAS_XTERM_LISTENER(vt_parser, get_window_size)) {
    u_int width;
    u_int height;

    (*vt_parser->xterm_listener->get_window_size)(vt_parser->xterm_listener->self, &width, &height);

    /* XXX This doesn't work on vertical mode. */
    if ((*col_width = width / vt_screen_get_logical_cols(vt_parser->screen)) == 0 ||
        (*line_height = height / vt_screen_get_logical_rows(vt_parser->screen)) == 0) {
      return 0;
    }

    return 1;
  } else {
    return 0;
  }
}

/*
 * width|height == 0: maximize
 * width|height == -1: current value
 */
static void resize(vt_parser_t *vt_parser, int width, int height, int by_char) {
  if (HAS_XTERM_LISTENER(vt_parser, resize)) {
    int flag = 0;

    if (by_char) {
      if (width == 0 || height == 0) {
        u_int col_width;
        u_int line_height;

        if (!HAS_XTERM_LISTENER(vt_parser, get_display_size) ||
            !get_cell_size(vt_parser, &col_width, &line_height)) {
          if (width == 0) {
            width = -1;
          }
          if (height == 0) {
            height = -1;
          }
        } else {
          u_int w;
          u_int h;

          (*vt_parser->xterm_listener->get_display_size)(vt_parser->xterm_listener->self, &w, &h);
          if (width == 0) {
            width = w / col_width;
          }
          if (height == 0) {
            height = h / line_height;

            goto do_resize;
          }
        }
      }

      if (width == -1) {
        width = vt_screen_get_logical_cols(vt_parser->screen);
      }
      if (height == -1) {
        height = vt_screen_get_logical_rows(vt_parser->screen);
      }

      /*
       * 'height' argument of this function should includes status line according to
       * the following document, but teraterm (4.95) excludes status line by CSI 8 t.
       * mlterm considers height of "CSI t", "OSC 5379;geometry", DECSNLS and DECSLPP
       * to be excluding status line.
       *
       * https://vt100.net/docs/vt510-rm/DECSNLS.html
       * The terminal supports three different font heights, which allows 26, 42, or 53 data
       * lines to be displayed on the screen or 25, 41, or 52 data lines to be displayed on
       * the screen, plus a status line.
       */
      if (vt_screen_has_status_line(vt_parser->screen)) {
        height ++;
      }

    do_resize:
      if (vt_screen_resize(vt_parser->screen, width, height) == 2) {
        flag = 1;
      }

      /*
       * xterm_listener::resize(0,0) means that screen should be
       * resized according to the size of pty.
       */
      width = 0;
      height = 0;
    } else {
      if ((width == 0 || height == 0) && HAS_XTERM_LISTENER(vt_parser, get_display_size)) {
        u_int w;
        u_int h;

        (*vt_parser->xterm_listener->get_display_size)(vt_parser->xterm_listener->self, &w, &h);
        if (width == 0) {
          width = w;
        }
        if (height == 0) {
          height = h;
        }
      }

      if (width == -1) {
        width = 0; /* xterm_listener->resize() doesn't resize width if width == 0. */
      }
      if (height == -1) {
        height = 0; /* xterm_listener->resize() doesn't resize height if height == 0. */
      }
    }

    stop_vt100_cmd(vt_parser, 0);
    (*vt_parser->xterm_listener->resize)(vt_parser->xterm_listener->self, width, height, 0, flag);
    start_vt100_cmd(vt_parser, 0);
  }
}

static void reverse_video(vt_parser_t *vt_parser, int flag) {
  if (HAS_XTERM_LISTENER(vt_parser, reverse_video)) {
    stop_vt100_cmd(vt_parser, 0);
    (*vt_parser->xterm_listener->reverse_video)(vt_parser->xterm_listener->self, flag);
    start_vt100_cmd(vt_parser, 0);
  }
}

static void set_mouse_report(vt_parser_t *vt_parser, vt_mouse_report_mode_t mode) {
  if (HAS_XTERM_LISTENER(vt_parser, set_mouse_report)) {
/*
 * If xterm_set_mouse_report() calls x_stop_selecting() etc,
 * remove #if 0 - #end.
 *
 * If #if 0 - #end is removed, ctl_render() is at least twice
 * in one sequence packet from w3m because w3m sends CSI?1000h every time.
 */
#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif

    if ((vt_parser->mouse_mode = mode) > 0 &&
        vt_parser->ext_mouse_mode == EXTENDED_MOUSE_REPORT_SGR_PIXEL) {
      vt_parser->mouse_mode |= MOUSE_REPORT_BY_PIXEL;
    }

    (*vt_parser->xterm_listener->set_mouse_report)(vt_parser->xterm_listener->self);

#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static void request_locator(vt_parser_t *vt_parser) {
  if (HAS_XTERM_LISTENER(vt_parser, request_locator)) {
    vt_mouse_report_mode_t orig;

#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif

    if (vt_parser->mouse_mode < LOCATOR_REPORT) {
      orig = vt_parser->mouse_mode;
      vt_parser->mouse_mode = LOCATOR_REPORT;
    } else {
      orig = 0;
    }

    vt_parser->locator_mode |= LOCATOR_REQUEST;

    (*vt_parser->xterm_listener->request_locator)(vt_parser->xterm_listener->self);

    vt_parser->locator_mode &= ~LOCATOR_REQUEST;

    if (orig) {
      vt_parser->mouse_mode = orig;
    }

#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static char *parse_title(vt_parser_t *vt_parser, char *src /* the caller should allocated */) {
  size_t len;
  ef_parser_t *parser;
  vt_char_encoding_t src_encoding;
  ef_char_t ch;
  u_int num_chars;
  vt_char_encoding_t dst_encoding;
  char *dst;

  if (src == NULL) {
    return NULL;
  }

  dst = NULL;

  len = strlen(src);
  if (vt_parser->set_title_using_hex) {
    if (!(len = bl_hex_decode(src, src, len))) {
#ifdef DEBUG
      bl_debug_printf("Failed to decode %s as hex string.\n", src);
#endif
      goto end;
    }
    src[len] = '\0';
  }

  if (vt_parser->set_title_using_utf8 ? vt_parser->encoding == VT_UTF8 : 1) {
    parser = vt_parser->cc_parser;
    src_encoding = vt_parser->encoding;
  } else {
    if (!(parser = vt_char_encoding_parser_new(VT_UTF8))) {
      goto end;
    }
    src_encoding = VT_UTF8;
  }

  (*parser->init)(parser);
  (*parser->set_str)(parser, src, len);
  num_chars = 0;
  while ((*parser->next_char)(parser, &ch)) {
    if ((ef_bytes_to_int(ch.ch, ch.size) & ~0x80) < 0x20) {
#ifdef DEBUG
      bl_debug_printf("%s which contains control characters is ignored for window title.\n",
                      src);
#endif
      goto end;
    }
    num_chars++;
  }

  /* locale encoding */
  dst_encoding = vt_get_char_encoding("auto");

  if (src_encoding == dst_encoding) {
    return src;
  }

  if ((dst = malloc(num_chars * UTF_MAX_SIZE + 1))) {
    (*parser->init)(parser);
    (*parser->set_str)(parser, src, len);
    len = vt_char_encoding_convert_with_parser(dst, num_chars * UTF_MAX_SIZE, dst_encoding,
                                               parser);
    dst[len] = '\0';

    if (parser != vt_parser->cc_parser) {
      (*parser->destroy)(parser);
    }
  }

end:
  free(src);

  return dst;
}

static void set_window_name(vt_parser_t *vt_parser,
                            u_char *name /* should be malloc'ed or NULL. */) {
  if (vt_parser->use_locked_title) {
    free(name);
    return;
  }

  free(vt_parser->win_name);
  vt_parser->win_name = name;

  if (HAS_XTERM_LISTENER(vt_parser, set_window_name)) {
#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif
    (*vt_parser->xterm_listener->set_window_name)(vt_parser->xterm_listener->self, name);
#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static void set_icon_name(vt_parser_t *vt_parser,
                          u_char *name /* should be malloc'ed or NULL. */) {
  if (vt_parser->use_locked_title) {
    free(name);
    return;
  }

  free(vt_parser->icon_name);
  vt_parser->icon_name = name;

  if (HAS_XTERM_LISTENER(vt_parser, set_icon_name)) {
#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif
    (*vt_parser->xterm_listener->set_icon_name)(vt_parser->xterm_listener->self, name);
#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static void switch_im_mode(vt_parser_t *vt_parser) {
  if (HAS_XTERM_LISTENER(vt_parser, switch_im_mode)) {
#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif
    (*vt_parser->xterm_listener->switch_im_mode)(vt_parser->xterm_listener->self);
#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static int im_is_active(vt_parser_t *vt_parser) {
  if (HAS_XTERM_LISTENER(vt_parser, im_is_active)) {
    return (*vt_parser->xterm_listener->im_is_active)(vt_parser->xterm_listener->self);
  } else {
    return 0;
  }
}

static void set_modkey_mode(vt_parser_t *vt_parser, int key, int mode) {
  if (key == 1) {
    if (-1 <= mode && mode <= 3) {
      vt_parser->modify_cursor_keys = mode;
    }
  } else if (key == 2) {
    if (-1 <= mode && mode <= 3) {
      vt_parser->modify_function_keys = mode;
    }
  } else if (key == 4) {
    if (0 <= mode && mode <= 2) {
      vt_parser->modify_other_keys = mode;
    }
  }
}

static void report_window_size(vt_parser_t *vt_parser, int by_char) {
  if (HAS_XTERM_LISTENER(vt_parser, get_window_size)) {
    int ps;
    u_int width;
    u_int height;
    char seq[5 + 1 /* ps */ + DIGIT_STR_LEN(u_int) * 2 + 1];

    if (by_char) {
      width = vt_screen_get_logical_cols(vt_parser->screen);
      height = vt_screen_get_logical_rows(vt_parser->screen);
      ps = 8;
    } else {
      (*vt_parser->xterm_listener->get_window_size)(vt_parser->xterm_listener->self,
                                                    &width, &height);
      ps = 4;
    }

    sprintf(seq, "\x1b[%d;%d;%dt", ps, height, width);
    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }
}

static void report_display_size(vt_parser_t *vt_parser, int by_char) {
  if (HAS_XTERM_LISTENER(vt_parser, get_display_size)) {
    int ps;
    u_int width;
    u_int height;
    char seq[5 + 1 /* ps */ + DIGIT_STR_LEN(u_int) * 2 + 1];

    (*vt_parser->xterm_listener->get_display_size)(vt_parser->xterm_listener->self,
                                                   &width, &height);

    if (by_char) {
      u_int col_width;
      u_int line_height;

      if (!get_cell_size(vt_parser, &col_width, &line_height) ||
          (width /= col_width) == 0 || (height /= line_height) == 0) {
        return;
      }

      ps = 9;
    } else {
      ps = 5;
    }

    sprintf(seq, "\x1b[%d;%d;%dt", ps, height, width);
    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }
}

static void report_cell_size(vt_parser_t *vt_parser) {
  u_int col_width;
  u_int line_height;

  if (get_cell_size(vt_parser, &col_width, &line_height)) {
    char seq[6 + DIGIT_STR_LEN(u_int) * 2 + 1];

    sprintf(seq, "\x1b[6;%d;%dt", line_height, col_width);
    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }
}

static void report_window_or_icon_name(vt_parser_t *vt_parser, int is_window) {
  char *seq;
  char *pre;
  char *title;
  size_t len;
  vt_char_encoding_t src_encoding;
  vt_char_encoding_t dst_encoding;

  if (is_window) {
    title = vt_parser->win_name;
    pre = "\x1b]l";
  } else {
    title = vt_parser->icon_name;
    pre = "\x1b]L";
  }

  if (!title) {
    title = "";
  }

  /* see parse_title() */
  src_encoding = vt_get_char_encoding("auto");

  if (vt_parser->get_title_using_utf8) {
    dst_encoding = VT_UTF8;
  } else {
    dst_encoding = vt_parser->encoding;
  }

  len = strlen(title);

  if (src_encoding != dst_encoding) {
    char *p;

    if (!(p = alloca(len * UTF_MAX_SIZE + 1))) {
      goto error;
    }

    len = vt_char_encoding_convert(p, len * UTF_MAX_SIZE, dst_encoding,
                                   title, len, src_encoding);
    title = p;
  }

  if (!(seq = alloca(3 + len * (vt_parser->get_title_using_hex ? 2 : 1) + 3))) {
    goto error;
  }

  strcpy(seq, pre);

  if (vt_parser->get_title_using_hex) {
    len = bl_hex_encode(seq + 3, title, len);
  } else {
    memcpy(seq + 3, title, len);
  }
  strcpy(seq + 3 + len, "\x1b\\");
  vt_write_to_pty(vt_parser->pty, seq, strlen(seq));

  return;

error:
  vt_write_to_pty(vt_parser->pty, pre, 3);
  vt_write_to_pty(vt_parser->pty, "\x1b\\", 2);
}

static void set_presentation_state(vt_parser_t *vt_parser, char *seq) {
  int row;
  int col;
  int page;
  char rend;
  char attr;
  char flag;

  if (sscanf(seq, "%d;%d;%d;%c;%c;%c;0;2;@;BB%%5%%5\x1b\\",
             &row, &col, &page, &rend, &attr, &flag) == 6) {
    vt_screen_goto(vt_parser->screen, col - 1, row - 1);

    if (rend & 0x20) {
      vt_parser->is_reversed = ((rend & 0x8) == 0x8);
      vt_parser->is_blinking = ((rend & 0x4) == 0x4);
      vt_parser->line_style &= ~LS_UNDERLINE;
      vt_parser->line_style |= ((rend & 0x2) ? LS_UNDERLINE_SINGLE : 0);
      vt_parser->is_bold = ((rend & 0x1) == 0x1);
    }

    if (attr & 0x20) {
      vt_parser->is_protected = ((attr & 0x1) == 0x1);
    }

    if (flag & 0x20) {
      vt_screen_set_auto_wrap(vt_parser->screen, ((flag & 0x8) == 0x8));
      vt_screen_set_relative_origin(vt_parser->screen, ((flag & 0x1) == 0x1));
    }
  }
}

static void report_presentation_state(vt_parser_t *vt_parser, int ps) {
  if (ps == 1) {
    /* DECCIR */
    char seq[DIGIT_STR_LEN(u_int) * 3 + 31];
    int rend = 0x40;
    int attr = 0x40;
    int flag = 0x40;

    if (vt_parser->is_reversed) {
      rend |= 0x8;
    }
    if (vt_parser->is_blinking) {
      rend |= 0x4;
    }
    if (vt_parser->line_style & LS_UNDERLINE) {
      rend |= 0x2;
    }
    if (vt_parser->is_bold) {
      rend |= 0x1;
    }

    if (vt_parser->is_protected) {
      attr |= 0x1;
    }

    if (vt_screen_is_auto_wrap(vt_parser->screen)) {
      flag |= 0x8;
    }
    if (vt_screen_is_relative_origin(vt_parser->screen)) {
      flag |= 0x1;
    }

    sprintf(seq, "\x1bP1$u%d;%d;%d;%c;%c;%c;0;2;@;BB%%5%%5\x1b\\",
            vt_screen_cursor_logical_row(vt_parser->screen) + 1,
            vt_screen_cursor_logical_col(vt_parser->screen) + 1,
            vt_screen_get_page_id(vt_parser->screen) + 1, rend, attr, flag);

    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  } else if (ps == 2) {
    /* DECTABSR */
    char *seq;
    u_int max_digit_len = 1;
    u_int cols = vt_screen_get_logical_cols(vt_parser->screen);
    u_int tmp = cols;

    while ((tmp /= 10) > 0) {
      max_digit_len++;
    }

    if ((seq = alloca(5 + (max_digit_len + 1) * cols + 3))) {
      u_int count;
      char *p;

      p = strcpy(seq, "\x1bP2$u") + 5;

      for (count = 0; count < cols; count++) {
        if (vt_screen_is_tab_stop(vt_parser->screen, count)) {
          sprintf(p, "%d/", count + 1);
          p += strlen(p);
        }
      }

      if (p != seq + 5) {
        p--;
      }

      strcpy(p, "\x1b\\");

      vt_write_to_pty(vt_parser->pty, seq, p + 2 - seq);
    }
  }
}

static void report_color_table(vt_parser_t *vt_parser, int pu) {
  int color;
  u_int8_t r, g, b;
  char seq[5+(3*5+4)*256+255+3];
  char *p;

  p = strcpy(seq, "\x1bP2$s") + 5;

  if (pu == 2) {
    /* RGB */
    for (color = 0; color < 256; color++) {
      vt_get_color_rgba(color, &r, &g, &b, NULL);
      sprintf(p, "%d;2;%d;%d;%d/", color, r * 100 / 255, g * 100 / 255, b * 100 / 255);
      p += strlen(p);
    }
  } else if (pu == 1) {
    /* HLS */
    int h, l, s;
    for (color = 0; color < 256; color++) {
      vt_get_color_rgba(color, &r, &g, &b, NULL);
      bl_rgb_to_hls(&h, &l, &s, r, g, b);
      sprintf(p, "%d;1;%d;%d;%d/", color, h, l, s);
      p += strlen(p);
    }
  } else {
    return;
  }

  strcpy(p - 1, "\x1b\\");

  vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
}

#ifndef NO_IMAGE
static int cursor_char_is_picture_and_modified(vt_screen_t *screen) {
  vt_line_t *line;
  vt_char_t *ch;

  if ((line = vt_screen_get_cursor_line(screen)) && vt_line_is_modified(line) &&
      (ch = vt_char_at(line, vt_screen_cursor_char_index(screen))) && vt_get_picture_char(ch)) {
    return 1;
  } else {
    return 0;
  }
}

/* Don't call this if SIXEL_NO_SCROLLING is false. */
static int check_sixel_anim(vt_screen_t *screen, u_char *str, size_t left) {
  vt_line_t *line = vt_screen_get_line(screen, 0); /* Always non-NULL */
  vt_char_t *ch;

  if ((ch = vt_char_at(line, 0)) && vt_get_picture_char(ch)) {
    while (--left > 0) {
      if (*(++str) == '\x1b') {
        if (--left == 0) {
          break;
        }

        if (*(++str) == 'P') {
          /* It seems animation sixel. */
          return 1;
        }
      }
    }
  }

  return 0;
}

static void show_picture(vt_parser_t *vt_parser, char *file_path, int clip_beg_col,
                         int clip_beg_row, int clip_cols, int clip_rows,
                         int img_cols, int img_rows, int keep_aspect,
                         int is_sixel /* 0: not sixel, 1: sixel, 2: sixel anim, 3: macro */
                         ) {
#ifndef DONT_OPTIMIZE_DRAWING_PICTURE
  if (is_sixel == 2) {
    (*vt_parser->xterm_listener->show_tmp_picture)(vt_parser->xterm_listener->self, file_path);

    /*
     * If only one window is shown on screen, it is not necessary to process
     * pending events for other windows.
     * But multiple windows can be shown even on framebuffer now, so it is
     * commented out.
     */
#if 0
    vt_parser->yield = 1;
#endif

    return;
  }
#endif /* DONT_OPTIMIZE_DRAWING_PICTURE */

  if (HAS_XTERM_LISTENER(vt_parser, get_picture_data)) {
    vt_char_t *data;
    int transparent;

#ifdef __DEBUG
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
#endif

    if ((data = (*vt_parser->xterm_listener->get_picture_data)(
                    vt_parser->xterm_listener->self, file_path, &img_cols, &img_rows, NULL, NULL,
                    is_sixel ? &vt_parser->sixel_palette : NULL, &transparent, keep_aspect, 0)) &&
        clip_beg_row < img_rows && clip_beg_col < img_cols) {
      vt_char_t *p;
      int row;
      int cursor_col;
      int orig_auto_wrap;
      int orig_visible_cursor;

      /* Flush buffer before vt_screen_overwrite_chars(picture data). */
      flush_buffer(vt_parser);

      if (transparent) {
        /*
         * Clear the cursor not to show it in the background of the transparent
         * (sixel P2=1) image.
         */
        interrupt_vt100_cmd(vt_parser);
      }

#ifdef __DEBUG
      gettimeofday(&tv2, NULL);
      bl_debug_printf("Processing sixel time (msec) %lu - %lu = %lu\n",
                      tv2.tv_sec * 1000 + tv2.tv_usec / 1000,
                      tv1.tv_sec * 1000 + tv1.tv_usec / 1000,
                      tv2.tv_sec * 1000 + tv2.tv_usec / 1000 -
                      tv1.tv_sec * 1000 - tv1.tv_usec / 1000);
#endif

      if (clip_cols == 0) {
        clip_cols = img_cols - clip_beg_col;
      }

      if (clip_rows == 0) {
        clip_rows = img_rows - clip_beg_row;
      }

      if (clip_beg_row + clip_rows > img_rows) {
        clip_rows = img_rows - clip_beg_row;
      }

      if (clip_beg_col + clip_cols > img_cols) {
        clip_cols = img_cols - clip_beg_col;
      }

      p = data + (img_cols * clip_beg_row);
      row = 0;

      if (is_sixel && !vt_parser->sixel_scrolling) {
        vt_screen_save_cursor(vt_parser->screen); /* XXX */
        orig_visible_cursor = vt_parser->is_visible_cursor;
        vt_parser->is_visible_cursor = 0;
        vt_screen_goto_home(vt_parser->screen);
        vt_screen_goto_beg_of_line(vt_parser->screen);
      }

      if (cursor_char_is_picture_and_modified(vt_parser->screen)) {
        /* Perhaps it is animation. */
        interrupt_vt100_cmd(vt_parser);
        /*
         * If only one window is shown on screen, it is not necessary to process
         * pending events for other windows.
         * But multiple windows can be shown even on framebuffer now, so it is
         * commented out.
         */
#if 0
        vt_parser->yield = 1;
#endif
      }

      orig_auto_wrap = vt_screen_is_auto_wrap(vt_parser->screen);
      vt_screen_set_auto_wrap(vt_parser->screen, 0);
      cursor_col = vt_screen_cursor_logical_col(vt_parser->screen);

      while (1) {
        vt_screen_overwrite_chars(vt_parser->screen, p + clip_beg_col, clip_cols);

        if (++row >= clip_rows) {
          break;
        }

        if (is_sixel && !vt_parser->sixel_scrolling) {
          if (!vt_screen_go_downward(vt_parser->screen, 1)) {
            break;
          }
        } else {
          if (vt_screen_line_feed(vt_parser->screen) == 2 /* scrolled */ && transparent) {
            /* Clear the background of the line scrolled in */
            interrupt_vt100_cmd(vt_parser);
          }
        }

        vt_screen_go_horizontally(vt_parser->screen, cursor_col);

        p += img_cols;
      }

      if (is_sixel) {
        if (vt_parser->sixel_scrolling) {
          if (!CURSOR_TO_RIGHT_OF_SIXEL(vt_parser) &&
              /* mlterm always enables DECSET 8452 in status line. */
              !vt_status_line_is_focused(vt_parser->screen)) {
            vt_screen_line_feed(vt_parser->screen);
            vt_screen_go_horizontally(vt_parser->screen, cursor_col);
          }
        } else {
          vt_screen_restore_cursor(vt_parser->screen); /* XXX */
          vt_parser->is_visible_cursor = orig_visible_cursor;
        }
      }

      vt_str_destroy(data, img_cols * img_rows);

      vt_screen_set_auto_wrap(vt_parser->screen, orig_auto_wrap);

      /*
       * If only one window is shown on screen, it is not necessary to process
       * pending events for other windows.
       * But multiple windows can be shown even on framebuffer now, so it is
       * commented out.
       */
#if 0
      if (strstr(file_path, "://")) {
        /* Showing remote image is very heavy. */
        vt_parser->yield = 1;
      }
#endif
    }
  }
}

static void define_drcs_picture(vt_parser_t *vt_parser, char *path, ef_charset_t cs, int idx,
                                u_int pix_width /* can be 0 */, u_int pix_height /* can be 0 */,
                                u_int col_width, u_int line_height) {
  if (HAS_XTERM_LISTENER(vt_parser, get_picture_data)) {
    vt_char_t *data;
    int cols = 0;
    int rows = 0;
    int cols_small = 0;
    int rows_small = 0;

    if (pix_width > 0 && pix_height > 0) {
      if (old_drcs_sixel) {
        cols = pix_width / col_width;
        rows = pix_height / line_height;
      } else {
        cols = (pix_width + col_width - 1) / col_width;
        rows = (pix_height + line_height - 1) / line_height;
      }
    }

    if (idx <= 0x5f &&
        /*
         * DRCS Sixel doesn't support P2=1 (transparent) for now.
         * (RLogin 2.28.9 also doesn't support it)
         */
        (data = (*vt_parser->xterm_listener->get_picture_data)(vt_parser->xterm_listener->self,
                                                               path, &cols, &rows, &cols_small,
                                                               &rows_small, NULL, NULL, 0, 1))) {
      u_int pages;
      u_int offset = 0;
      vt_drcs_font_t *font;

      if (!vt_parser->drcs) {
        vt_parser->drcs = vt_drcs_new();
      }

      if (!old_drcs_sixel) {
        cols_small = cols;
        rows_small = rows;
      }

      for (pages = (cols_small * rows_small + 95) / 96; pages > 0; pages--) {
        font = vt_drcs_get_font(vt_parser->drcs, cs, 1);

        vt_drcs_add_picture(font, vt_char_picture_id(vt_get_picture_char(data)),
                            offset, idx, cols, rows, cols_small, rows_small);

        offset += (96 - idx);
        idx = 0;

        if (cs == CS94SB_ID(0x7e)) {
          cs = CS96SB_ID(0x30);
        } else if (cs == CS96SB_ID(0x7e)) {
          cs = CS94SB_ID(0x30);
        } else {
          cs++;
        }
      }

      vt_str_destroy(data, cols * rows);
    }
  }
}

/*
 * The 2nd \x1bP...\x1b\\ in the following sequence (e.g. biplane.six)
 * overwrites the image of the 1st \x1bP...\x1b\\.
 * (\x1b[8k moves the cursor upward after drawing the 1st image.)
 * \x1bP...\x1b\\\x1b[8k\x1bP;1...\x1b\\
 *                            ^-> Pixel positions specified as 0 remain at their current color
 *
 * -> The 1st and 2nd sequences are saved in the same file and
 *    load_sixel_from_data() in c_sixel.c draws them at once.
 *
 * XXX
 * return -1 if len == 0, and wait for the next sequence.
 */
static int is_separated_sixel(u_char *str, size_t len) {
  if (len >= 3) {
    if (*str == '\x1d') {
      str++;
      len--;
    } else if (*str == '\x1b' && *(str + 1) == ']') {
      str += 2;
      len -= 2;
    } else {
      return 0;
    }

    while ('0' <= *str && *str <= '9') {
      str++;
      if (--len == 0) {
        return 0;
      }
    }

    if (*str == 'A' || *str == 'k') {
      u_int count = 0;

      str++;
      len--;
      while (len >= 3 /* \x90;1 => 3 */) {
        if (*str == '\x1b' && *(str + 1) == 'P') {
          str += 2;
          len -= 2;
        } else {
          len--;
          if (*(str++) != 0x90) {
            goto next_loop;
          }
        }

        if ('0' <= *str && *str <= '9') {
          str++;
          len--;
        }

        if (len >= 2 && *str == ';') {
          if (*(++str) == '1') {
            return 1;
          }
          len--;
        }

      next_loop:
        /*
         * It is assumed that the distance between 1st and 2nd sequence is
         * less than about 10 bytes.
         */
        if (++count >= 10) {
          break;
        }
      }
    }
  }

  return 0;
}

static int increment_str(u_char **str, size_t *left);

static int save_sixel_or_regis(vt_parser_t *vt_parser, char *path, u_char *dcs_beg,
                               u_char **body /* q ... */ , size_t *body_len) {
  u_char *str_p = *body;
  size_t left = *body_len;
  int is_end;
  FILE *fp;

  if (left > 2 && *(str_p + 1) == '\0') {
    fp = fopen(path, "a");
    is_end = *(str_p + 2);
    /*
     * dcs_beg will equal to str_p after str_p is
     * incremented by the following increment_str().
     */
    dcs_beg = (str_p += 2) + 1;
    left -= 2;
  } else {
    char *format;
    vt_color_t color;
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;

    fp = fopen(path, "w");
    is_end = 0;

    if (strcmp(path + strlen(path) - 4, ".rgs") == 0) {
      /* Clear background by VT_BG_COLOR */

      /* 13 + 3*3 + 1 = 23 */
      format = "S(I(R%dG%dB%d))S(E)";
      color = VT_BG_COLOR;
    } else if (strcmp(path + strlen(path) - 4, ".six") == 0) {
      /*
       * Set VT_FG_COLOR to the default value of the first entry of the sixel palette,
       * because some sixel graphics data has not palette definitions (#0;2;r;g;b).
       * '9' is a mark which means that this definition is added by mlterm itself.
       * (see c_sixel.c)
       */

      /* 7 + 3*3 + 1 = 17 */
      format = "#0;9;%d;%d;%d";
      color = VT_FG_COLOR;
    } else {
      format = NULL;
    }

    if (format && HAS_XTERM_LISTENER(vt_parser, get_rgb) &&
        (*vt_parser->xterm_listener->get_rgb)(vt_parser->xterm_listener->self, &red,
                                              &green, &blue, color)) {
      char color_seq[23];

      if (color == VT_FG_COLOR) {
        /* sixel */
        red = red * 100 / 255;
        green = green * 100 / 255;
        blue = blue * 100 / 255;
      }

      fwrite(dcs_beg, 1, str_p - dcs_beg + 1, fp);
      sprintf(color_seq, format, red, green, blue);
      fwrite(color_seq, 1, strlen(color_seq), fp);
      dcs_beg = str_p + 1;
    }

    /*
     * +1 in case str_p[left - vt_parser->r_buf.new_len]
     * points "\\" of "\x1b\\".
     */
    if (left > vt_parser->r_buf.new_len + 1) {
      str_p += (left - vt_parser->r_buf.new_len - 1);
      left = vt_parser->r_buf.new_len + 1;
    }
  }

  while (1) {
    if (!increment_str(&str_p, &left)) {
      if (is_end == 2) {
        left++;
        break;
      }

      if (vt_parser->logging_vt_seq && use_ttyrec_format) {
        fclose(fp);
        free(path);

        *body = str_p;
        *body_len = left;

        return 0;
      }

      fwrite(dcs_beg, 1, str_p - dcs_beg + 1, fp);

      vt_parser->r_buf.left = 0;
      if (!receive_bytes(vt_parser)) {
        fclose(fp);
        memcpy(vt_parser->r_buf.chars,
               strcmp(path + strlen(path) - 4, ".six") == 0 ? "\x1bPq\0" : "\x1bPp\0", 4);
        free(path);
        vt_parser->r_buf.chars[4] = is_end;
        vt_parser->r_buf.filled_len = vt_parser->r_buf.left = 5;

        /*
         * If only one window is shown on screen, it is not necessary to process
         * pending events for other windows.
         * But multiple windows can be shown even on framebuffer now, so it is
         * commented out.
         */
#if 0
        /* No more data in pty. */
        vt_parser->yield = 1;
#endif

        *body = str_p;
        *body_len = left;

        return 0;
      }

      dcs_beg = str_p = CURRENT_STR_P(vt_parser);
      left = vt_parser->r_buf.left;
    }

    if (is_end == 2) {
      if (is_separated_sixel(str_p, left)) {
        /* continued (XXX Hack for biplane.six) */
        is_end = 0;
      } else {
        str_p--;
        left++;
        break;
      }
    }
    /*
     * 0x9c is regarded as ST here, because sixel sequence
     * unuses it certainly.
     */
    else if (*str_p == 0x9c) {
      is_end = 2;
    } else if (*str_p == CTL_ESC) {
      is_end = 1;
    } else if (is_end == 1) {
      if (*str_p == '\\') {
        is_end = 2;
      } else {
        is_end = 0;
      }
    }
  }

  fwrite(dcs_beg, 1, str_p - dcs_beg + 1, fp);
  fclose(fp);

  *body = str_p;
  *body_len = left;

  return 1;
}

static int check_cell_size(vt_parser_t *vt_parser, u_int col_width, u_int line_height) {
  u_int cw;
  u_int lh;

  /*
   * XXX
   * This works except vertical mode, but no problem because images are not
   * supported on vertical mode.
   */
  if (get_cell_size(vt_parser, &cw, &lh) && cw == col_width && lh == line_height) {
    return 1;
  } else {
    return 0;
  }
}
#endif

static void snapshot(vt_parser_t *vt_parser, vt_char_encoding_t encoding,
                     char *file_name, vt_write_content_area_t area) {
  char buf[16];
  char *path;
  int fd;
  ef_conv_t *conv;

  if (!(path = get_home_file_path(file_name, get_now_suffix(buf), "snp"))) {
    return;
  }

  fd = open(path, O_WRONLY | O_CREAT, 0600);
  free(path);
  if (fd == -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed to open %s\n", file_name);
#endif

    return;
  }

  if (encoding == VT_UNKNOWN_ENCODING || (conv = vt_char_encoding_conv_new(encoding)) == NULL) {
    conv = vt_parser->cc_conv;
  }

  vt_screen_write_content(vt_parser->screen, fd, conv, 0, area);

  if (conv != vt_parser->cc_conv) {
    (*conv->destroy)(conv);
  }

  close(fd);
}

static void set_col_size_of_width_a(vt_parser_t *vt_parser, u_int col_size_a) {
  if (col_size_a == 1 || col_size_a == 2) {
#ifdef USE_LIBSSH2
    /* Don't change it after vt_parser_set_pty() on mosh. See vt_parser_set_pty(). */
    if (!vt_parser->pty || vt_pty_get_mode(vt_parser->pty) != PTY_MOSH)
#endif
    {
      vt_parser->col_size_of_width_a = col_size_a;
    }
  } else {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " col size should be 1 or 2. default value 1 is used.\n");
#endif

    vt_parser->col_size_of_width_a = 1;
  }
}

/*
 * This function will destroy the content of pt.
 */
static void config_protocol_set(vt_parser_t *vt_parser, char *pt, int save) {
  int ret;
  char *dev;

  ret = vt_parse_proto_prefix(&dev, &pt, save);
  if (ret <= 0) {
    /*
     * ret == -1: do_challenge failed.
     * ret ==  0: illegal format.
     * to_menu is necessarily 0 because it is pty that msg should be returned
     * to.
     */
    vt_response_config(vt_parser->pty, ret < 0 ? "forbidden" : "error", NULL, 0);

    return;
  }

  if (dev && strlen(dev) <= 7 && strstr(dev, "font")) {
    char *key;
    char *val;

    if (vt_parse_proto(NULL, &key, &val, &pt, 0, 0) && val &&
        HAS_CONFIG_LISTENER(vt_parser, set_font)) {
/*
 * Screen is redrawn not in vt_parser->config_listener->set_font
 * but in stop_vt100_cmd, so it is not necessary to hold
 * vt_parser->config_listener->set_font between stop_vt100_cmd and
 * start_vt100_cmd.
 */
#if 0
      stop_vt100_cmd(vt_parser, 0);
#endif

      (*vt_parser->config_listener->set_font)(vt_parser->config_listener->self, dev, key, val,
                                              save);

#if 0
      start_vt100_cmd(vt_parser, 0);
#endif
    }
  } else if (dev && strcmp(dev, "color") == 0) {
    char *key;
    char *val;

    if (vt_parse_proto(NULL, &key, &val, &pt, 0, 0) && val &&
        HAS_CONFIG_LISTENER(vt_parser, set_color)) {
/*
 * Screen is redrawn not in vt_parser->config_listener->set_color
 * but in stop_vt100_cmd, so it is not necessary to hold
 * vt_parser->config_listener->set_font between stop_vt100_cmd and
 * start_vt100_cmd.
 */
#if 0
      stop_vt100_cmd(vt_parser, 0);
#endif

      (*vt_parser->config_listener->set_color)(vt_parser->config_listener->self, dev, key,
                                               val, save);

#if 0
      start_vt100_cmd(vt_parser, 0);
#endif
    }
  } else {
    stop_vt100_cmd(vt_parser, 0);

    if ((!HAS_CONFIG_LISTENER(vt_parser, exec) ||
         !(*vt_parser->config_listener->exec)(vt_parser->config_listener->self, pt))) {
      bl_conf_write_t *conf;

      if (save) {
        char *path;

        /* XXX */
        if ((path = bl_get_user_rc_path("mlterm/main")) == NULL) {
          return;
        }

        conf = bl_conf_write_open(path);
        free(path);
      } else {
        conf = NULL;
      }

      /* accept multiple key=value pairs. */
      while (pt) {
        char *key;
        char *val;

        if (!vt_parse_proto(dev ? NULL : &dev, &key, &val, &pt, 0, 1)) {
          break;
        }

        if (conf) {
          /* XXX */
          if (strcmp(key, "xim") != 0) {
            bl_conf_io_write(conf, key, val);
          }
        }

        if (val == NULL) {
          val = "";
        }

        if (HAS_CONFIG_LISTENER(vt_parser, set) &&
            (*vt_parser->config_listener->set)(vt_parser->config_listener->self, dev, key,
                                                  val)) {
          if (!vt_parser->config_listener) {
            /* pty changed. */
            break;
          }
        }

        dev = NULL;
      }

      if (conf) {
        bl_conf_write_close(conf);

        if (HAS_CONFIG_LISTENER(vt_parser, saved)) {
          (*vt_parser->config_listener->saved)();
        }
      }
    }

    start_vt100_cmd(vt_parser, 0);
  }
}

static void config_protocol_set_simple(vt_parser_t *vt_parser, char *key, char *val,
                                       int visualize) {
  if (HAS_CONFIG_LISTENER(vt_parser, set)) {
    if (visualize) {
      stop_vt100_cmd(vt_parser, 0);
    }

    (*vt_parser->config_listener->set)(vt_parser->config_listener->self, NULL, key, val);

    if (visualize) {
      start_vt100_cmd(vt_parser, 0);
    }
  }
}

/*
 * This function will destroy the content of pt.
 */
static void config_protocol_get(vt_parser_t *vt_parser, char *pt, int to_menu, int *flag) {
  char *dev;
  char *key;
  int ret;

  if (to_menu == 0 && strchr(pt, ';') == NULL) {
    /* pt doesn't have challenge */
    to_menu = -1;

    stop_vt100_cmd(vt_parser, 0);
  }
#if 0
  else {
    /*
     * It is assumed that screen is not redrawn not in
     * vt_parser->config_listener->get, so vt_parser->config_listener->get
     * is not held between stop_vt100_cmd and start_vt100_cmd.
     */
    stop_vt100_cmd(vt_parser, 0);
  }
#endif

  ret = vt_parse_proto(&dev, &key, NULL, &pt, to_menu == 0, 0);
  if (ret <= 0) {
    /*
     * ret == -1: do_challenge failed.
     * ret ==  0: illegal format.
     * to_menu is necessarily 0 because it is pty that msg should be returned
     * to.
     */
    vt_response_config(vt_parser->pty, ret < 0 ? "forbidden" : "error", NULL, 0);

    goto end;
  }

  if (dev && strlen(dev) <= 7 && strstr(dev, "font")) {
    char *cs;

    /* Compat with old format (3.6.3 or before): <cs>,<fontsize> */
    cs = bl_str_sep(&key, ",");

    if (HAS_CONFIG_LISTENER(vt_parser, get_font)) {
      (*vt_parser->config_listener->get_font)(vt_parser->config_listener->self, dev, cs,
                                                 to_menu);
    }
  } else if (dev && strcmp(dev, "color") == 0) {
    if (HAS_CONFIG_LISTENER(vt_parser, get_color)) {
      (*vt_parser->config_listener->get_color)(vt_parser->config_listener->self, key,
                                                  to_menu);
    }
  } else if (HAS_CONFIG_LISTENER(vt_parser, get)) {
    (*vt_parser->config_listener->get)(vt_parser->config_listener->self, dev, key, to_menu);
  }

end:
  if (to_menu == -1) {
    start_vt100_cmd(vt_parser, 0);
  }
#if 0
  else {
    start_vt100_cmd(vt_parser, 0);
  }
#endif
}

#ifdef SUPPORT_ITERM2_OSC1337

#include <pobl/bl_path.h> /* bl_basename */

/*
 * This function will destroy the content of pt.
 */
static void iterm2_proprietary_set(vt_parser_t *vt_parser, char *pt) {
  if (strncmp(pt, "File=", 5) == 0) {
    /* See https://www.iterm2.com/images.html (2014/03/20) */

    char *args;
    char *encoded;
    char *decoded;
    size_t e_len;
    u_int width;
    u_int height;
    char *path;
    int keep_aspect;

    args = pt + 5;
    width = height = 0;
    keep_aspect = 1; /* default value is 1. */

    if ((encoded = strchr(args, ':'))) {
      char *beg;
      char *end;

      *(encoded++) = '\0';

      if ((beg = strstr(args, "name=")) &&
          ((end = strchr((beg += 5), ';')) || (end = beg + strlen(beg))) &&
          (path = malloc(7 + (end - beg) + 1))) {
        char *file;
        char *new_path;
        size_t d_len;

        strcpy(path, "mlterm/");

        d_len = bl_base64_decode(path + 7, beg, end - beg);
        path[7 + d_len] = '\0';
        file = bl_basename(path);
        memmove(path + 7, file, strlen(file) + 1);
        new_path = bl_get_user_rc_path(path);
        free(path);
        path = new_path;
      } else {
        path = get_home_file_path("", vt_pty_get_slave_name(vt_parser->pty) + 5, "img");
      }

      if ((beg = strstr(args, "width=")) &&
          ((end = strchr((beg += 6), ';')) || (end = beg + strlen(beg)))) {
        *(end--) = '\0';
        if ('0' <= *end && *end <= '9') {
          width = atoi(beg);
        } else if (*end == '%') {
          /* XXX vertical mode is not considered */
          width = atoi(beg) * vt_screen_get_logical_cols(vt_parser->screen) / 100;
        } else {
          /* XXX Npx is not supported */
        }
        *(end + 1) = ';'; /* For next strstr() */
      }

      if ((beg = strstr(args, "height=")) &&
          ((end = strchr((beg += 7), ';')) || (end = beg + strlen(beg)))) {
        *(end--) = '\0';
        if ('0' <= *end && *end <= '9') {
          height = atoi(beg);
        } else if (*end == '%') {
          /* XXX vertical mode is not considered */
          height = atoi(beg) * vt_screen_get_logical_rows(vt_parser->screen) / 100;
        } else {
          /* XXX Npx is not supported */
        }
        /* *(end + 1) = ';'; */
      }

      if ((beg = strstr(args, "preserveAspectRatio=")) && beg[20] == '0') {
        keep_aspect = 0;
      }
    } else {
      encoded = args;
      path = get_home_file_path("", vt_pty_get_slave_name(vt_parser->pty) + 5, "img");
    }

    if (!path) {
      return;
    }
#ifdef DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " OSC 1337 file is stored at %s.\n", path);
    }
#endif

    if ((e_len = strlen(encoded)) > 0 && (decoded = malloc(e_len))) {
      size_t d_len;
      FILE *fp;

      if ((d_len = bl_base64_decode(decoded, encoded, e_len)) > 0 && (fp = fopen(path, "w"))) {
        fwrite(decoded, 1, d_len, fp);
        fclose(fp);

        show_picture(vt_parser, path, 0, 0, 0, 0, width, height, keep_aspect, 0);

        remove(path);
      }

      free(decoded);
    }

    free(path);
  }
}
#endif

static int change_char_fine_color(vt_parser_t *vt_parser, int *ps, int num) {
  int proceed;
  vt_color_t color;

  if (ps[0] != 38 && ps[0] != 48) {
    return 0;
  }

  if (num >= 3 && ps[1] == 5) {
    proceed = 3;
    color = (ps[2] <= 0 ? 0 : ps[2]);
  } else if (num >= 5 && ps[1] == 2) {
    proceed = 5;
    color = vt_get_closest_color(ps[2] <= 0 ? 0 : ps[2], ps[3] <= 0 ? 0 : ps[3],
                                 ps[4] <= 0 ? 0 : ps[4]);
#ifdef USE_COMPACT_TRUECOLOR
    vt_parser->yield = 1;
#endif
  } else {
    return 1;
  }

  if (vt_parser->use_ansi_colors) {
    if (ps[0] == 38) {
      vt_parser->fg_color = color;
      vt_screen_set_bce_fg_color(vt_parser->screen, color);
    } else /* if( ps[0] == 48) */
    {
      vt_parser->bg_color = color;
      vt_screen_set_bce_bg_color(vt_parser->screen, color);
    }
  }

  return proceed;
}

static void change_char_attr(vt_parser_t *vt_parser, int flag) {
  vt_color_t fg_color;
  vt_color_t bg_color;

  fg_color = vt_parser->fg_color;
  bg_color = vt_parser->bg_color;

  if (flag == 0) {
    /* Normal */
    fg_color = VT_FG_COLOR;
    bg_color = VT_BG_COLOR;
    vt_parser->is_bold = 0;
    vt_parser->is_italic = 0;
    vt_parser->line_style = 0;
    vt_parser->is_reversed = 0;
    vt_parser->is_blinking = 0;
    vt_parser->is_invisible = 0;
  } else if (flag == 1) {
    /* Bold */
    vt_parser->is_bold = 1;
  } else if (flag == 2) {
    /* XXX Faint */
    vt_parser->is_bold = 0;
  } else if (flag == 3) {
    /* Italic */
    vt_parser->is_italic = 1;
  } else if (flag == 4) {
    /* Underscore */
    vt_parser->line_style = (vt_parser->line_style & ~LS_UNDERLINE) | LS_UNDERLINE_SINGLE;
  } else if (flag == 5 || flag == 6) {
    /* Blink (6 is repidly blinking) */
    vt_parser->is_blinking = 1;
    vt_screen_enable_blinking(vt_parser->screen);
  } else if (flag == 7) {
    /* Inverse */
    vt_parser->is_reversed = 1;
  } else if (flag == 8) {
    vt_parser->is_invisible = 1;
  } else if (flag == 9) {
    vt_parser->line_style |= LS_CROSSED_OUT;
  } else if (flag == 21) {
    /* Double underscore */
    vt_parser->line_style = (vt_parser->line_style & ~LS_UNDERLINE) | LS_UNDERLINE_DOUBLE;
  } else if (flag == 22) {
    /* Bold */
    vt_parser->is_bold = 0;
  } else if (flag == 23) {
    /* Italic */
    vt_parser->is_italic = 0;
  } else if (flag == 24) {
    /* Underline */
    vt_parser->line_style &= ~LS_UNDERLINE;
  } else if (flag == 25) {
    /* blink */
    vt_parser->is_blinking = 0;
  } else if (flag == 27) {
    vt_parser->is_reversed = 0;
  } else if (flag == 28) {
    vt_parser->is_invisible = 0;
  } else if (flag == 29) {
    vt_parser->line_style &= ~LS_CROSSED_OUT;
  } else if (flag == 39) {
    /* default fg */
    fg_color = VT_FG_COLOR;
  } else if (flag == 49) {
    bg_color = VT_BG_COLOR;
  } else if (flag == 53) {
    vt_parser->line_style |= LS_OVERLINE;
  } else if (flag == 55) {
    vt_parser->line_style &= ~LS_OVERLINE;
  } else if (vt_parser->use_ansi_colors) {
    /* Color attributes */

    if (30 <= flag && flag <= 37) {
      /* 30=VT_BLACK(0) ... 37=VT_WHITE(7) */
      fg_color = flag - 30;
    } else if (40 <= flag && flag <= 47) {
      /* 40=VT_BLACK(0) ... 47=VT_WHITE(7) */
      bg_color = flag - 40;
    } else if (90 <= flag && flag <= 97) {
      fg_color = (flag - 90) | VT_BOLD_COLOR_MASK;
    } else if (100 <= flag && flag <= 107) {
      bg_color = (flag - 100) | VT_BOLD_COLOR_MASK;
    }
#ifdef DEBUG
    else {
      bl_warn_printf(BL_DEBUG_TAG " unknown char attr flag(%d).\n", flag);
    }
#endif
  }

  if (fg_color != vt_parser->fg_color) {
    vt_parser->fg_color = fg_color;
    vt_screen_set_bce_fg_color(vt_parser->screen, fg_color);
  }

  if (bg_color != vt_parser->bg_color) {
    vt_parser->bg_color = bg_color;
    vt_screen_set_bce_bg_color(vt_parser->screen, bg_color);
  }
}

static void get_rgb(vt_parser_t *vt_parser, vt_color_t color) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  int ret;

  if (IS_ALT_COLOR(color)) {
    ret = (HAS_XTERM_LISTENER(vt_parser, get_rgb) &&
           (*vt_parser->xterm_listener->get_rgb)(vt_parser->xterm_listener->self, &red, &green,
                                                 &blue, color));
    color = color + 0x100 - VT_BOLD_COLOR;
  } else {
    ret = vt_get_color_rgba(color, &red, &green, &blue, NULL);
  }

  if (ret) {
    char seq[4 + DIGIT_STR_LEN(int) + 1 + 18 + 2 + 1];

    sprintf(seq, "\x1b]4;%d;rgb:%.2x%.2x/%.2x%.2x/%.2x%.2x\x1b\\",
            color, red, red, green, green, blue, blue);
    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }
}

static void get_fgbg_rgb(vt_parser_t *vt_parser, int ps /* 10, 11 */) {
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;

  if (HAS_XTERM_LISTENER(vt_parser, get_rgb) &&
      (*vt_parser->xterm_listener->get_rgb)(vt_parser->xterm_listener->self, &red, &green,
                                            &blue, ps == 10 ? VT_FG_COLOR : VT_BG_COLOR)) {
    char seq[2 + DIGIT_STR_LEN(int) + 1 + 18 + 2 + 1];

    sprintf(seq, "\x1b]%d;rgb:%.2x%.2x/%.2x%.2x/%.2x%.2x\x1b\\",
            ps, red, red, green, green, blue, blue);

    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }
}

/*
 * This function will destroy the content of pt.
 */
static void change_color_rgb(vt_parser_t *vt_parser, u_char *pt) {
  char *p;

  while ((p = strchr(pt, ';'))) {
    char *next;

    if ((next = strchr(p + 1, ';'))) {
      *(next++) = '\0';
    }

    if (strcmp(p + 1, "?") == 0) {
      vt_color_t color;

      *p = '\0';

      if ((color = vt_get_color(pt)) != VT_UNKNOWN_COLOR) {
        get_rgb(vt_parser, color);
      }
    } else {
      *p = '=';

      if ((p = alloca(6 + strlen(pt) + 1))) {
        sprintf(p, "color:%s", pt);
        config_protocol_set(vt_parser, p, 0);
      }
    }

    if (!next) {
      break;
    }

    pt = next;
  }
}

static char *get_special_color_name(const char c /* '0' - '4' */) {
  if (c == '0') {
    return "bd_color";
  } else if (c == '1') {
    return "ul_color";
  } else if (c == '2') {
    return "bl_color";
  } else if (c == '3') {
    return "rv_color"; /* XXX Not supported for now */
  } else /* if (c == '4') */ {
    return "it_color";
  }
}

static void change_special_color(vt_parser_t *vt_parser, u_char *pt) {
  char *p;

  while ((p = strchr(pt, ';'))) {
    char *next;

    if ((next = strchr(p + 1, ';'))) {
      *(next++) = '\0';
    }

    if ('0' <= *pt && *pt <= '4') {
      if (strcmp(p + 1, "?") == 0) {
        get_rgb(vt_parser, *pt - '0' + VT_BOLD_COLOR);
      } else {
        config_protocol_set_simple(vt_parser, get_special_color_name(*pt), p + 1, 0);
      }
    }

    if (!next) {
      break;
    }

    pt = next;
  }
}

static void change_fgbg_color(vt_parser_t *vt_parser, int ps /* 10, 11 */, u_char *pt) {
  while (1) {
    char *next;

    if ((next = strchr(pt, ';'))) {
      *(next++) = '\0';
    }

    if (10 <= ps && ps <= 11) {
      if (strcmp(pt, "?") == 0) {
        get_fgbg_rgb(vt_parser, ps);
      } else {
        char *key;

        if (ps == 10) {
          key = "fg_color";
        } else /* if (ps == 11) */ {
          key = "bg_color";
        }

        config_protocol_set_simple(vt_parser, key, pt, 0);
      }
    }

    if (!next) {
      break;
    }

    pt = next;
    ps ++;
  }
}

static void reset_color_rgb(vt_parser_t *vt_parser, u_char *pt, int is_spcolor) {
  u_char *p;
  char seq[11];
  int color;

  if (*pt == '\0') {
    if (is_spcolor) {
      for (color = '0'; color <= '4'; color++) {
        config_protocol_set_simple(vt_parser, get_special_color_name(color), "", 0);
      }
    } else {
      for (color = 0; color < 256; color++) {
        sprintf(seq, "color:%d=", color);
        config_protocol_set(vt_parser, seq, 0);
      }
    }
  } else {
    while ((p = bl_str_sep((char**)&pt, ";"))) {
      if (is_spcolor) {
        if ('0' <= *p && *p <= '4') {
          config_protocol_set_simple(vt_parser, get_special_color_name(*p), "", 0);
        }
      } else {
        color = atoi(p);
        sprintf(seq, "color:%d=", color);
        config_protocol_set(vt_parser, seq, 0);
      }
    }
  }
}

static void set_selection(vt_parser_t *vt_parser, u_char *encoded) {
  if (HAS_XTERM_LISTENER(vt_parser, set_selection)) {
    u_char *p;
    u_char *targets;
    size_t e_len;
    size_t d_len;
    u_char *decoded;
    ef_char_t ch;
    vt_char_t *str;
    u_int str_len;

    if ((p = strchr(encoded, ';'))) {
      *p = '\0';
      targets = encoded;
      encoded = p + 1;
    } else {
      targets = "s0";
    }

    if ((e_len = strlen(encoded)) < 4 || !(decoded = alloca(e_len)) ||
        (d_len = bl_base64_decode(decoded, encoded, e_len)) == 0 || !(str = vt_str_new(d_len))) {
      return;
    }

    str_len = 0;
    (*vt_parser->cc_parser->set_str)(vt_parser->cc_parser, decoded, d_len);
    while ((*vt_parser->cc_parser->next_char)(vt_parser->cc_parser, &ch)) {
      vt_char_set(&str[str_len++], ef_char_to_int(&ch), ch.cs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

/*
 * It is assumed that screen is not redrawn not in
 * vt_parser->config_listener->get, so vt_parser->config_listener->get
 * is not held between stop_vt100_cmd and start_vt100_cmd.
 */
#if 0
    stop_vt100_cmd(vt_parser, 0);
#endif

    (*vt_parser->xterm_listener->set_selection)(vt_parser->xterm_listener->self, str, str_len,
                                                   targets);

#if 0
    start_vt100_cmd(vt_parser, 0);
#endif
  }
}

static void destroy_macro(vt_parser_t *vt_parser,
                         int id /* should be less than vt_parser->num_macros */
                         ) {
  if (vt_parser->macros[id].is_sixel) {
    unlink(vt_parser->macros[id].str);
    vt_parser->macros[id].is_sixel = 0;
    vt_parser->macros[id].sixel_num++;
  }

  free(vt_parser->macros[id].str);
  vt_parser->macros[id].str = NULL;
}

static void destroy_all_macros(vt_parser_t *vt_parser) {
  u_int count;

  for (count = 0; count < vt_parser->num_macros; count++) {
    destroy_macro(vt_parser, count);
  }

  free(vt_parser->macros);
  vt_parser->macros = NULL;
  vt_parser->num_macros = 0;
}

static u_char *hex_to_text(u_char *hex) {
  u_char *text;
  u_char *p;
  size_t len;
  size_t count;
  int rep_count;
  u_char *rep_beg;
  int d[2];
  int sub_count;

  len = strlen(hex) / 2 + 1;
  if (!(text = malloc(len))) {
    return NULL;
  }

  count = 0;
  rep_count = 1;
  rep_beg = NULL;
  sub_count = 0;

  /* Don't use sscanf() here because it works too slow. */
  while (1) {
    if ('0' <= *hex && *hex <= '9') {
      d[sub_count++] = *hex - '0';
    } else {
      u_char masked_hex;

      masked_hex = (*hex) & 0xcf;
      if ('A' <= masked_hex && masked_hex <= 'F') {
        d[sub_count++] = masked_hex - 'A' + 10;
      } else {
        sub_count = 0;

        if (*hex == '!') {
          rep_beg = NULL;

          if ((p = strchr(hex + 1, ';'))) {
            *p = '\0';
            if ((rep_count = atoi(hex + 1)) > 1) {
              rep_beg = text + count;
            }
            hex = p;
          }
        } else if (*hex == ';' || *hex == '\0') {
          if (rep_beg) {
            size_t rep_len;

            if ((rep_len = text + count - rep_beg) > 0) {
              len += rep_len * (rep_count - 1);
              if (!(p = realloc(text, len))) {
                free(text);

                return NULL;
              }
              rep_beg += (p - text);
              text = p;

              while (--rep_count > 0) {
                strncpy(text + count, rep_beg, rep_len);
                count += rep_len;
              }
            }

            rep_beg = NULL;
          }

          if (*hex == '\0') {
            goto end;
          }
        }
      }
    }

    if (sub_count == 2) {
      text[count++] = (d[0] << 4) + d[1];
      sub_count = 0;
    }

    hex++;
  }

end:
  text[count] = '\0';

  return text;
}

#define MAX_NUM_OF_MACRO 1024
#define MAX_DIGIT_OF_MACRO 4

static void define_macro(vt_parser_t *vt_parser, u_char *param, u_char *data) {
  u_char *p;
  int ps[3] = {0, 0, 0};
  int num = 0;

  for (p = param; *p != 'z'; p++) {
    if ((*p == ';' || *p == '!') && num < 3) {
      *p = '\0';
      ps[num++] = atoi(param);
      param = p + 1;
    }
  }

  if (ps[0] >= MAX_NUM_OF_MACRO) {
    return;
  }

  if (ps[1] == 1) {
    destroy_all_macros(vt_parser);
  }

  if (ps[0] >= vt_parser->num_macros) {
    void *p;

    if (*data == '\0' ||
        !(p = realloc(vt_parser->macros, (ps[0] + 1) * sizeof(*vt_parser->macros)))) {
      return;
    }

    memset((vt_parser->macros = p) + vt_parser->num_macros, 0,
           (ps[0] + 1 - vt_parser->num_macros) * sizeof(*vt_parser->macros));
    vt_parser->num_macros = ps[0] + 1;
  } else {
    destroy_macro(vt_parser, ps[0]);

    if (*data == '\0') {
      return;
    }
  }

  if (ps[2] == 1) {
    p = vt_parser->macros[ps[0]].str = hex_to_text(data);

#ifndef NO_IMAGE
    if (p && (*p == 0x90 || (*(p++) == '\x1b' && *p == 'P'))) {
      for (p++; *p == ';' || ('0' <= *p && *p <= '9'); p++)
        ;

      if (*p == 'q' && (strrchr(p, 0x9c) || ((p = strrchr(p, '\\')) && *(p - 1) == '\x1b'))) {
        char prefix[5 + MAX_DIGIT_OF_MACRO + 1 + DIGIT_STR_LEN(vt_parser->macros[0].sixel_num) +
                    2];
        char *path;

        sprintf(prefix, "macro%d_%d_", ps[0], vt_parser->macros[ps[0]].sixel_num);

        if ((path =
                 get_home_file_path(prefix, vt_pty_get_slave_name(vt_parser->pty) + 5, "six"))) {
          FILE *fp;

          if ((fp = fopen(path, "w"))) {
            fwrite(vt_parser->macros[ps[0]].str, 1, strlen(vt_parser->macros[ps[0]].str), fp);
            fclose(fp);

            free(vt_parser->macros[ps[0]].str);
            vt_parser->macros[ps[0]].str = path;
            vt_parser->macros[ps[0]].is_sixel = 1;

#ifdef DEBUG
            bl_debug_printf(BL_DEBUG_TAG " Register %s to macro %d\n", path, ps[0]);
#endif
          }
        }
      }
    }
#endif
  } else {
    vt_parser->macros[ps[0]].str = strdup(data);
  }
}

static int write_loopback(vt_parser_t *vt_parser, const u_char *buf, size_t len,
                          int enable_local_echo, int is_visual);

static void invoke_macro(vt_parser_t *vt_parser, int id) {
  if (id < vt_parser->num_macros && vt_parser->macros[id].str) {
#ifndef NO_IMAGE
    if (vt_parser->macros[id].is_sixel) {
      show_picture(vt_parser, vt_parser->macros[id].str, 0, 0, 0, 0, 0, 0, 0, 3);
    } else
#endif
    {
      write_loopback(vt_parser, vt_parser->macros[id].str,
                     strlen(vt_parser->macros[id].str), 0, 0);
    }
  }
}

static int response_termcap(vt_pty_t *pty, u_char *key, u_char *value) {
  u_char *response;

  if ((response = alloca(5 + strlen(key) + 1 + strlen(value) * 2 + 3))) {
    u_char *dst;

    sprintf(response, "\x1bP1+r%s=", key);
    for (dst = response + strlen(response); *value; value++, dst += 2) {
      dst[0] = (value[0] >> 4) & 0xf;
      dst[0] = (dst[0] > 9) ? (dst[0] + 'A' - 10) : (dst[0] + '0');
      dst[1] = value[0] & 0xf;
      dst[1] = (dst[1] > 9) ? (dst[1] + 'A' - 10) : (dst[1] + '0');
    }
    strcpy(dst, "\x1b\\");

    vt_write_to_pty(pty, response, strlen(response));

    return 1;
  } else {
    return 0;
  }
}

#define TO_INT(a) (((a) | 0x20) - ((a) <= '9' ? '0' : ('a' - 10)))

static void report_termcap(vt_parser_t *vt_parser, u_char *key) {
  u_char *deckey;
  u_char *src;
  u_char *dst;

  if ((deckey = alloca(strlen(key) / 2 + 1))) {
    struct {
      u_char *tckey;
      u_char *tikey;
      int16_t spkey; /* vt_special_key_t */
      int16_t modcode;

    } db[] = {
        {"%1", "khlp", SPKEY_F15, 0},
        {"#1", "kHLP", SPKEY_F15, 2},
        {"@0", "kfnd", SPKEY_FIND, 0},
        {"*0", "kFND", SPKEY_FIND, 2},
        {"*6", "kslt", SPKEY_SELECT, 0},
        {"#6", "kSLT", SPKEY_SELECT, 2},

        {"kh", "khome", SPKEY_HOME, 0},
        {"#2", "kHOM", SPKEY_HOME, 2},
        {"@7", "kend", SPKEY_END, 0},
        {"*7", "kEND", SPKEY_END, 2},

        {"kl", "kcub1", SPKEY_LEFT, 0},
        {"kr", "kcuf1", SPKEY_RIGHT, 0},
        {"ku", "kcuu1", SPKEY_UP, 0},
        {"kd", "kcud1", SPKEY_DOWN, 0},

        {"#4", "kLFT", SPKEY_LEFT, 2},
        {"%i", "kRIT", SPKEY_RIGHT, 2},
        {"kF", "kind", SPKEY_DOWN, 2},
        {"kR", "kri", SPKEY_UP, 2},

        {"k1", "kf1", SPKEY_F1, 0},
        {"k2", "kf2", SPKEY_F2, 0},
        {"k3", "kf3", SPKEY_F3, 0},
        {"k4", "kf4", SPKEY_F4, 0},
        {"k5", "kf5", SPKEY_F5, 0},
        {"k6", "kf6", SPKEY_F6, 0},
        {"k7", "kf7", SPKEY_F7, 0},
        {"k8", "kf8", SPKEY_F8, 0},
        {"k9", "kf9", SPKEY_F9, 0},
        {"k;", "kf10", SPKEY_F10, 0},

        {"F1", "kf11", SPKEY_F11, 0},
        {"F2", "kf12", SPKEY_F12, 0},
        {"F3", "kf13", SPKEY_F13, 0},
        {"F4", "kf14", SPKEY_F14, 0},
        {"F5", "kf15", SPKEY_F15, 0},
        {"F6", "kf16", SPKEY_F16, 0},
        {"F7", "kf17", SPKEY_F17, 0},
        {"F8", "kf18", SPKEY_F18, 0},
        {"F9", "kf19", SPKEY_F19, 0},
        {"FA", "kf20", SPKEY_F20, 0},
        {"FB", "kf21", SPKEY_F21, 0},
        {"FC", "kf22", SPKEY_F22, 0},
        {"FD", "kf23", SPKEY_F23, 0},
        {"FE", "kf24", SPKEY_F24, 0},
        {"FF", "kf25", SPKEY_F25, 0},
        {"FG", "kf26", SPKEY_F26, 0},
        {"FH", "kf27", SPKEY_F27, 0},
        {"FI", "kf28", SPKEY_F28, 0},
        {"FJ", "kf29", SPKEY_F29, 0},
        {"FK", "kf30", SPKEY_F30, 0},
        {"FL", "kf31", SPKEY_F31, 0},
        {"FM", "kf32", SPKEY_F32, 0},
        {"FN", "kf33", SPKEY_F33, 0},
        {"FO", "kf34", SPKEY_F34, 0},
        {"FP", "kf35", SPKEY_F35, 0},
        {"FQ", "kf36", SPKEY_F36, 0},
        {"FR", "kf37", SPKEY_F37, 0},

        {"K1", "ka1", SPKEY_KP_HOME, 0},
        {"K4", "kc1", SPKEY_KP_END, 0},
        {"K3", "ka3", SPKEY_KP_PRIOR, 0},
        {"K5", "kc3", SPKEY_KP_NEXT, 0},
        {"kB", "kcbt", SPKEY_ISO_LEFT_TAB, 0},
        /* { "kC" , "kclr" , SPKEY_CLEAR , 0 } , */
        {"kD", "kdch1", SPKEY_DELETE, 0},
        {"kI", "kich1", SPKEY_INSERT, 0},

        {"kN", "knp", SPKEY_NEXT, 0},
        {"kP", "kpp", SPKEY_PRIOR, 0},
        {"%c", "kNXT", SPKEY_NEXT, 2},
        {"%e", "kPRV", SPKEY_PRIOR, 2},

        /* { "&8" , "kund" , SPKEY_UNDO , 0 } , */
        {"kb", "kbs", SPKEY_BACKSPACE, 0},

        {"Co", "colors", -1, 0},
        {"TN", "name", -2, 0},
    };
    int idx;
    u_char *value;

    for (src = key, dst = deckey; src[0] && src[1]; src += 2) {
      *(dst++) = TO_INT(src[0]) * 16 + TO_INT(src[1]);
    }
    *dst = '\0';

    value = NULL;

    for (idx = 0; idx < BL_ARRAY_SIZE(db); idx++) {
      if (strcmp(deckey, db[idx].tckey) == 0 || strcmp(deckey, db[idx].tikey) == 0) {
        if (db[idx].spkey == -1) {
          value = "256";
        } else if (db[idx].spkey == -2) {
          value = "mlterm";
        } else {
          value = vt_termcap_special_key_to_seq(
              vt_parser->termcap, db[idx].spkey, db[idx].modcode,
              /* vt_parser->is_app_keypad */ 0, IS_APP_CURSOR_KEYS(vt_parser),
              /* IS_APP_ESCAPE(vt_parser) */ 0,
              vt_parser->modify_cursor_keys, vt_parser->modify_function_keys);
        }

        break;
      }
    }

    if (value && response_termcap(vt_parser->pty, key, value)) {
      return;
    }
  }

  vt_write_to_pty(vt_parser->pty, "\x1bP0+r\x1b\\", 7);
}

static void report_char_attr_status(vt_parser_t *vt_parser) {
  char color[10]; /* ";38;5;XXX" (256 color) */

  vt_write_to_pty(vt_parser->pty, "\x1bP1$r0", 6);

  if (vt_parser->is_bold) {
    vt_write_to_pty(vt_parser->pty, ";1", 2);
  }

  if (vt_parser->is_italic) {
    vt_write_to_pty(vt_parser->pty, ";3", 2);
  }

  if (vt_parser->line_style & LS_UNDERLINE_SINGLE) {
    vt_write_to_pty(vt_parser->pty, ";4", 2);
  }

  if (vt_parser->is_blinking) {
    vt_write_to_pty(vt_parser->pty, ";5", 2);
  }

  if (vt_parser->is_reversed) {
    vt_write_to_pty(vt_parser->pty, ";7", 2);
  }

  if (vt_parser->is_invisible) {
    vt_write_to_pty(vt_parser->pty, ":8", 2);
  }

  if (vt_parser->line_style & LS_CROSSED_OUT) {
    vt_write_to_pty(vt_parser->pty, ";9", 2);
  }

  if (vt_parser->line_style & LS_UNDERLINE_DOUBLE) {
    vt_write_to_pty(vt_parser->pty, ";21", 3);
  }

  if (vt_parser->line_style & LS_OVERLINE) {
    vt_write_to_pty(vt_parser->pty, ":53", 3);
  }

  color[0] = ';';
  if (IS_VTSYS_BASE_COLOR(vt_parser->fg_color)) {
    color[1] = '3';
    color[2] = '0' + vt_parser->fg_color;
    color[3] = '\0';
  } else if (IS_VTSYS_COLOR(vt_parser->fg_color)) {
    color[1] = '9';
    color[2] = '0' + (vt_parser->fg_color - 8);
    color[3] = '\0';
  } else if (IS_VTSYS256_COLOR(vt_parser->fg_color)) {
    sprintf(color + 1, "38;5;%d", vt_parser->fg_color);
  } else {
    goto bg_color;
  }
  vt_write_to_pty(vt_parser->pty, color, strlen(color));

bg_color:
  if (IS_VTSYS_BASE_COLOR(vt_parser->bg_color)) {
    color[1] = '4';
    color[2] = '0' + vt_parser->bg_color;
    color[3] = '\0';
  } else if (IS_VTSYS_COLOR(vt_parser->bg_color)) {
    color[1] = '1';
    color[2] = '0';
    color[3] = '0' + (vt_parser->bg_color - 8);
    color[4] = '\0';
  } else if (IS_VTSYS256_COLOR(vt_parser->bg_color)) {
    sprintf(color + 1, "48;5;%d", vt_parser->bg_color);
  } else {
    goto end;
  }
  vt_write_to_pty(vt_parser->pty, color, strlen(color));

end:
  vt_write_to_pty(vt_parser->pty, "m\x1b\\", 3);
}

static void report_status(vt_parser_t *vt_parser, u_char *key) {
  u_char *val;
  u_char digits[DIGIT_STR_LEN(int)*3 + 3];
  u_char *seq;

  if (strcmp(key, "\"q") == 0) {
    /* DECSCA */
    val = vt_parser->is_protected ? "1" : "0"; /* Both 0 and 2 => is_protected = 0 */
  } else if (strcmp(key, "\"p") == 0) {
    /* DECSCL */
    val = "63;1";
  } else if (strcmp(key, "r") == 0) {
    /* DECSTBM */
    val = digits;
    sprintf(val, "%d;%d", vt_parser->screen->edit->vmargin_beg + 1,
            vt_parser->screen->edit->vmargin_end + 1);
  } else if (strcmp(key, "s") == 0) {
    /* DECSLRM */
    val = digits;
    sprintf(val, "%d;%d", vt_parser->screen->edit->hmargin_beg + 1,
            vt_parser->screen->edit->hmargin_end + 1);
  } else if (strcmp(key, " q") == 0) {
    /* DECSCUR */
    const char vals[] = {'2', '4', '6', '\0', '1', '3', '5'};
    digits[0] = vals[vt_parser->cursor_style];
    digits[1] = '\0';
    val = digits;
  } else if (strcmp(key, "$}") == 0) {
    /* DECSASD */
    val = vt_status_line_is_focused(vt_parser->screen) ? "1" : "0";
  } else if (strcmp(key, "$~") == 0) {
    /* DECSSDT */
    val = vt_screen_has_status_line(vt_parser->screen) ? "2" : "0";
  } else if (strcmp(key, "*x") == 0) {
    /* DECSACE */
    val = vt_screen_is_using_rect_attr_select(vt_parser->screen) ? "2" : "1";
  } else if (strcmp(key, "){") == 0) {
    /* DECSTGLT */
    if (vt_parser->use_ansi_colors) {
      val = "0";
    } else if (vt_parser->alt_colors.flags) {
      val = "1";
    } else {
      val = "2";
    }
  } else if (strcmp(key, "$|") == 0) {
    /* DECSCPP */
    val = digits;
    sprintf(val, "%d", vt_screen_get_logical_cols(vt_parser->screen));
  } else if (strcmp(key, "t") == 0 || strcmp(key, "*|") == 0) {
    /* DECSLPP/DECSNLS */
    val = digits;
    sprintf(val, "%d", vt_screen_get_logical_rows(vt_parser->screen));
  } else if (strcmp(key, "m") == 0) {
    /* SGR */
    report_char_attr_status(vt_parser);
    return;
  } else {
    u_int ps = atoi(key);

    key += (strlen(key) - 2);

    if (strcmp(key, ",|") == 0) {
      /* DECAC */
      u_int8_t red;
      u_int8_t green;
      u_int8_t blue;
      vt_color_t colors[] = { VT_FG_COLOR, VT_BG_COLOR } ;
      u_int count;

      if (ps == 2) {
        /* Window Frame */
        goto error_validreq;
      }

      vt_color_force_linear_search(1);
      for (count = 0; count < BL_ARRAY_SIZE(colors); count++) {
        if (!HAS_XTERM_LISTENER(vt_parser, get_rgb) ||
            !(*vt_parser->xterm_listener->get_rgb)(vt_parser->xterm_listener->self, &red, &green,
                                                   &blue, colors[count])) {
          goto error_validreq;
        }
        colors[count] = vt_get_closest_color(red, green, blue);
      }
      vt_color_force_linear_search(0);

      val = digits;
      sprintf(val, "1;%d;%d", colors[0], colors[1]);
    } else if (strcmp(key, ",}") == 0) {
      /* DECATC */
      int idx;
      if (ps <= 15 && (vt_parser->alt_colors.flags & (1 << (idx = alt_color_idxs[ps])))) {
        val = digits;
        sprintf(val, "%d;%d;%d", ps, vt_parser->alt_colors.fg[idx], vt_parser->alt_colors.bg[idx]);
      } else {
        goto error_validreq;
      }
    } else {
      goto error_invalidreq;
    }
  }

  /*
   * XXX
   * xterm returns 1 for vaild request while 0 for invalid request.
   * VT520/525 manual says 0 for vaild request while 1 for invalid request,
   * but it's wrong. (https://twitter.com/ttdoda/status/911125737242992645)
   */

  if ((seq = alloca(7 + strlen(val) + strlen(key) + 1))) {
    sprintf(seq, "\x1bP1$r%s%s\x1b\\", val, key);
    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
  }

  return;

error_validreq:
  vt_write_to_pty(vt_parser->pty, "\x1bP1$r\x1b\\", 7);
  return;

error_invalidreq:
  vt_write_to_pty(vt_parser->pty, "\x1bP0$r\x1b\\", 7);
}

static void clear_line_all(vt_parser_t *vt_parser) {
  vt_screen_clear_line_to_left(vt_parser->screen);
  vt_screen_clear_line_to_right(vt_parser->screen);
}

static void clear_display_all(vt_parser_t *vt_parser) {
  vt_screen_clear_below(vt_parser->screen);
  vt_screen_clear_above(vt_parser->screen);
  vt_screen_clear_size_attr(vt_parser->screen);
}

static int vtmode_num_to_idx(int mode) {
  int idx;

  for (idx = 0; idx < VTMODE_NUM; idx++) {
    if (vtmodes[idx] == mode) {
      return idx;
    }
  }

  return -1;
}

static int get_vtmode(vt_parser_t *vt_parser, int mode) {
  int idx;

  /* XXX These can be changed via OSC 5379 instead of DEC Private Mode Set. */
  if (mode == 8428) {
    return (vt_parser->col_size_of_width_a == 1) ? 1 : 2;
  } else if (mode == 117) {
    return vt_screen_is_using_bce(vt_parser->screen) ? 1 : 2;
  } else if (mode == VTMODE(33)) {
    return (vt_parser->cursor_style & CS_BLINK) ? 2 : 1;
  } else
  /* XXX DECBKM affects *all* terms sharing vt_termcap, so vtmode_flags is not reliable. */
  if (mode == 67) {
    return strcmp(vt_termcap_special_key_to_seq(vt_parser->termcap, SPKEY_BACKSPACE,
                                                0, 0, 0, 0, 0, 0), "\x08") == 0 ? 1 : 2;
  }

  if ((idx = vtmode_num_to_idx(mode)) != -1) {
    if (GET_VTMODE_FLAG(vt_parser, idx)) {
      return 1; /* set */
    } else {
      return 2; /* reset */
    }
  }

  if (mode == 8 /* DECARM (auto repeat) */ ||
      mode == 64 /* DECPCCM (page cursor-coupling mode) */ ||
      mode == 98 /* DECARSM (change lines per screen automatically by DECSLPP) */ ||
      mode == 102 /* DECNULM (always dicard NUL character) */ ||
      mode == 114 /* DECATCUM (alternate text color underline mode) */ ||
      mode == 115 /* DECATCBM (alternate text color blink mode) */ ||
      mode == 19 /* DECPEX (Ignore DECSTBM in MC and DECMC) */ ||
      mode == 1007 /* Alternate Scroll Mode */) {
    return 3; /* permanently set */
  } else if (mode == 4 /* DECSCLM (smooth scroll) */ || mode == 9 /* X10 mouse report */ ||
             mode == 38 /* Tek */ || mode == 45 /* reverse wraparound */ ||
             mode == 1001 /* highlight mouse tracking */ ||
             mode == 1011 /* scroll to bottom on key press */ ||
             /*
              * GATM, SRTM, VEM, HEM, PUM, FEAM, FETM, MATM, TTM, SATM, TSM, EBM are
              * permanently reset on VT510.
              * (https://vt100.net/docs/vt510-rm/DECRQM.html)
              *
              * ERM => http://nanno.dip.jp/softlib/man/rlogin/ctrlcode.html#ERM
              */
             mode == VTMODE(1) /* GATM */ ||
             (VTMODE(5) <= mode && mode <= VTMODE(7)) /* SRTM, ERM, VEM */||
             mode == VTMODE(10) /* HEM */ ||
             mode == VTMODE(11) /* PUM */ ||
             /* FEAM, FETM, MATM, TTM, SATM, TSM, EBM */
             (VTMODE(13) <= mode && mode <= VTMODE(19)) ||
             mode == VTMODE(18) /* ISM */) {
    return 4; /* permanently reset */
  }

  return 0; /* not recognized */
}

static void set_vtmode(vt_parser_t *vt_parser, int mode, int flag) {
  int idx;

#ifdef DEBUG
  if (VTMODE_NUM != BL_ARRAY_SIZE(vtmodes)) {
    bl_debug_printf(BL_DEBUG_TAG "vtmode table is broken.\n");
  }
#endif

  if ((idx = vtmode_num_to_idx(mode)) == -1) {
#ifdef DEBUG
    debug_print_unknown("ESC [%s %d l\n", mode >= VTMODE(0) ? "" : " ?",
                        mode >= VTMODE(0) ? mode - VTMODE(0) : mode);
#endif

    return;
  }

  if (flag) {
    vt_parser->vtmode_flags[DIV32(idx)] |= (SHIFT_FLAG(idx));
  } else {
    vt_parser->vtmode_flags[DIV32(idx)] &= ~(SHIFT_FLAG(idx));
  }

  switch (idx) {
  case DECMODE_1:
    /* DECCKM */
    break;

  case DECMODE_2:
#ifdef USE_VT52
    if (!(vt_parser->is_vt52_mode = (flag ? 0 : 1))) {
      /*
       * USASCII should be designated for G0-G3 here, but it is temporized by
       * using init_encoding_parser() etc for now.
       */
      init_encoding_parser(vt_parser);
    }
#endif
    break;

  case DECMODE_3:
    /* DECCOLM */
    if (ALLOW_DECCOLM(vt_parser)) {
      /* XTERM compatibility [#1048321] */
      if (!KEEP_SCREEN_ON_DECCOLM(vt_parser)) {
        clear_display_all(vt_parser);
        vt_screen_goto(vt_parser->screen, 0, 0);
      }
      resize(vt_parser, flag ? 132 : 80, -1, 1);
    }
    break;

#if 0
  case DECMODE_4:
    /* DECSCLM smooth scrolling */
    break;
#endif

  case DECMODE_5:
    /* DECSCNM */
    reverse_video(vt_parser, flag);
    break;

  case DECMODE_6:
    /* DECOM */
    if (flag) {
      vt_screen_set_relative_origin(vt_parser->screen, 1);
    } else {
      vt_screen_set_relative_origin(vt_parser->screen, 0);
    }

    /*
     * cursor position is reset
     * (the same behavior of xterm 4.0.3, kterm 6.2.0 or so)
     */
    vt_screen_goto(vt_parser->screen, 0, 0);
    break;

  case DECMODE_7:
    /* DECAWM */
    vt_screen_set_auto_wrap(vt_parser->screen, flag);
    break;

#if 0
  case DECMODE_8:
    /* DECARM auto repeat */
    break;

  case DECMODE_9:
    /* X10 mouse reporting */
    break;

  case DECMODE_12:
    /*
     * XT_CBLINK
     * If cursor blinking is enabled, restart blinking.
     */
    break;
#endif

  case DECMODE_25:
    /* DECTCEM */
    vt_parser->is_visible_cursor = flag;
    break;

#if 0
  case DECMODE_35:
    /* shift keys */
    break;
#endif

  case DECMODE_40:
    if (flag) {
      /* Compatible behavior with rlogin. (Not compatible with xterm) */
      set_vtmode(vt_parser, 3, GET_VTMODE_FLAG2(vt_parser, DECMODE_3));
    }
    break;

  case DECMODE_47:
    if (use_alt_buffer) {
      if (flag) {
        vt_screen_use_alternative_edit(vt_parser->screen);
      } else {
        vt_screen_use_normal_edit(vt_parser->screen);
      }
    }
    break;

  case DECMODE_66:
    /* DECNKM */
    vt_parser->is_app_keypad = flag;
    break;

  case DECMODE_67:
    /* DECBKM have back space */
    /* XXX Affects all terms sharing vt_termcap */
    vt_termcap_set_key_seq(vt_parser->termcap, SPKEY_BACKSPACE, flag ? "\x08" : "\x7f");
    break;

  case DECMODE_69:
    /* DECLRMM */
    vt_screen_set_use_hmargin(vt_parser->screen, flag);
    break;

  case DECMODE_80:
    /* DECSDM */
    /*
     * See https://github.com/arakiken/mlterm/pull/23
     *     https://github.com/hackerb9/lsix/issues/41
     */
    vt_parser->sixel_scrolling = (flag ? 0 : 1);
    break;

  case DECMODE_95:
    /* DECNCSM */
    break;

  case DECMODE_116:
    /* DECBBSM */
    break;

  case DECMODE_117:
    /* DECECM */
    vt_screen_set_use_bce(vt_parser->screen, (flag ? 0 : 1));
    break;

  case DECMODE_1000: /* MOUSE_REPORT */
  case DECMODE_1002: /* BUTTON_EVENT */
  case DECMODE_1003: /* ANY_EVENT */
    set_mouse_report(vt_parser, flag ? MOUSE_REPORT + ((idx - DECMODE_1000) << 4) : 0);
    break;

#if 0
  case DECMODE_1001:
    /* X11 mouse highlighting */
    break;
#endif

  case DECMODE_1004:
    vt_parser->want_focus_event = flag;
    break;

  case DECMODE_1005: /* UTF8 */
  case DECMODE_1006: /* SGR */
  case DECMODE_1015: /* URXVT */
  case DECMODE_1016: /* SGR-PIXEL */
    if (flag) {
      vt_parser->ext_mouse_mode = EXTENDED_MOUSE_REPORT_UTF8 + idx - DECMODE_1005;
    } else {
      vt_parser->ext_mouse_mode = 0;
    }
    break;

  case DECMODE_1010:
    /* scroll to bottom on tty output */
    config_protocol_set_simple(vt_parser, "exit_backscroll_by_pty", flag ? "true" : "false", 0);
    break;

#if 0
  case DECMODE_1011:
    /* scroll to bottom on key press (always enabled) */
    break;
#endif

  case DECMODE_1034:
    if (get_vtmode(vt_parser, 1036) == 2 /* reset */) {
      config_protocol_set_simple(vt_parser, "mod_meta_mode", flag ? "8bit" : "none", 0);
    }
    break;

  case DECMODE_1036:
    config_protocol_set_simple(vt_parser, "mod_meta_mode",
                               flag ? "esc" :
                                      (get_vtmode(vt_parser, 1034) == 1 ? "8bit" : "none"),
                               0);
    break;

  case DECMODE_1042:
    config_protocol_set_simple(vt_parser, "use_urgent_bell", flag ? "true" : "false", 0);
    break;

  case DECMODE_1047:
    if (use_alt_buffer) {
      if (flag) {
        vt_screen_use_alternative_edit(vt_parser->screen);
      } else {
        if (vt_screen_is_alternative_edit(vt_parser->screen)) {
          clear_display_all(vt_parser);
          vt_screen_use_normal_edit(vt_parser->screen);
        }
      }
    }
    break;

  case DECMODE_1048:
    if (use_alt_buffer) {
      if (flag) {
        save_cursor(vt_parser);
      } else {
        restore_cursor(vt_parser);
      }
    }
    break;

  case DECMODE_1049:
    if (use_alt_buffer) {
      if (flag) {
        if (!vt_screen_is_alternative_edit(vt_parser->screen)) {
          save_cursor(vt_parser);
          vt_screen_use_alternative_edit(vt_parser->screen);
          clear_display_all(vt_parser);
        }
      } else {
        if (vt_screen_is_alternative_edit(vt_parser->screen)) {
          vt_screen_use_normal_edit(vt_parser->screen);
          restore_cursor(vt_parser);
        }
      }
    }
    break;

  case DECMODE_2004:
    vt_parser->is_bracketed_paste_mode = flag;
    break;

  case DECMODE_7727:
    /* Do nothing because IS_APP_ESCAPE() macro is used to check app escape mode. */
    break;

  case DECMODE_8428:
    /* RLogin original */
    vt_parser->col_size_of_width_a = (flag ? 1 : 2);
    break;

  case DECMODE_8452:
    /* RLogin original */
    break;

  case DECMODE_8800:
    if (flag) {
      vt_parser->unicode_policy |= USE_UNICODE_DRCS;
    } else {
      vt_parser->unicode_policy &= ~USE_UNICODE_DRCS;
    }
    break;

  case VTMODE_2:
    /* KAM */
    if (!HAS_XTERM_LISTENER(vt_parser, lock_keyboard)) {
      if (flag) {
        vt_parser->vtmode_flags[DIV32(VTMODE_2)] &= ~(SHIFT_FLAG(VTMODE_2)); /* unset flag */
      }
    } else {
      (*vt_parser->xterm_listener->lock_keyboard)(vt_parser->xterm_listener->self, flag);
    }
    break;

  case VTMODE_4:
    /* IRM */
    if (flag) {
      /* insert mode */
      vt_parser->w_buf.output_func = vt_screen_insert_chars;
    } else {
      /* replace mode */
      vt_parser->w_buf.output_func = vt_screen_overwrite_chars;
    }
    break;

  case VTMODE_12:
    /* SRM */
    break;

  case VTMODE_20:
    /* LNM */
    break;

  case VTMODE_33:
    /* WYSTCURM */
    if (flag) {
      vt_parser->cursor_style &= ~CS_BLINK;
    } else {
      vt_parser->cursor_style |= CS_BLINK;
    }
    break;

  case VTMODE_34:
    /* WYULCURM */
    if (flag) {
      vt_parser->cursor_style |= CS_UNDERLINE;
    } else {
      vt_parser->cursor_style &= ~CS_UNDERLINE;
    }
    break;
  }
}

/* Options configurable by ~/.mlterm/main are not reset. */
static void soft_reset(vt_parser_t *vt_parser) {
  /*
   * XXX
   * Following options is not reset for now.
   * DECNRCM, DECAUPSS, DECKPM, DECRLM, DECPCTERM
   * (see https://vt100.net/docs/vt510-rm/DECSTR.html)
   */

  /* "CSI m" (SGR) */
  change_char_attr(vt_parser, 0);

  init_encoding_parser(vt_parser);

  /* DECSC */
  (vt_screen_is_alternative_edit(vt_parser->screen) ? &vt_parser->saved_alternate
                                                       : &vt_parser->saved_normal)->is_saved = 0;

  /* DECSCA */
  vt_parser->is_protected = 0;

  /* CSI < r, CSI < s, CSI < t (compatible with TeraTerm) */
  vt_parser->im_is_active = 0;

  set_vtmode(vt_parser, 1, 0); /* DECCKM */
  /* auto_wrap == 1 (compatible with xterm, not with VT220) */
  set_vtmode(vt_parser, 7, 1); /* DECAWM */
  set_vtmode(vt_parser, 25, 1); /* DECTCEM */
  set_vtmode(vt_parser, 66, 0); /* DECNKM */
  set_vtmode(vt_parser, VTMODE(2), 0); /* KAM */
  set_vtmode(vt_parser, VTMODE(4), 0); /* IRM */

  /* DECOM */
#if 0
  set_vtmode(vt_parser, 6, 0);
#else
  vt_screen_set_relative_origin(vt_parser->screen, 0);
  vt_parser->vtmode_flags[DIV32(DECMODE_6)] &= ~(SHIFT_FLAG(DECMODE_6));
#endif

  /*
   * DECSLRM is reset but DECLRMM is as it is.
   * (Compatible with xterm, not described in vt510 manual.)
   */
  vt_screen_set_hmargin(vt_parser->screen, -1, -1);

  /* "CSI r" (DECSTBM) */
  vt_screen_set_vmargin(vt_parser->screen, -1, -1);

  /* DECSC => Home position */
  vt_screen_saved_cursor_to_home(vt_parser->screen);

  /* DECSASD */
  vt_focus_main_screen(vt_parser->screen);
}

static void full_reset(vt_parser_t *vt_parser) {
  int mode;

  vt_screen_use_normal_edit(vt_parser->screen);
  vt_screen_goto_page(vt_parser->screen, 0);
  /* RIS doesn't reset DECMODE_40 in xterm (see test_RIS_ResetDECCOLM in ris.py in esctest) */
  vt_parser->saved_vtmode_flags[0] = vt_parser->vtmode_flags[0] =
    INITIAL_VTMODE_FLAGS_0 | (DIV32(DECMODE_40) == 0 ? GET_VTMODE_FLAG(vt_parser, DECMODE_40) : 0);
  vt_parser->saved_vtmode_flags[1] = vt_parser->vtmode_flags[1] =
    INITIAL_VTMODE_FLAGS_1 | (DIV32(DECMODE_40) == 1 ? GET_VTMODE_FLAG(vt_parser, DECMODE_40) : 0);
  for (mode = 0; mode < VTMODE_NUM; mode++) {
    set_vtmode(vt_parser, vtmodes[mode], GET_VTMODE_FLAG2(vt_parser, mode));
  }
  soft_reset(vt_parser); /* XXX insufficient */
  vt_screen_goto(vt_parser->screen, 0, 0);
  vt_screen_set_tab_size(vt_parser->screen, 8);
  clear_display_all(vt_parser); /* XXX off-screen pages are not cleared */
  destroy_all_macros(vt_parser);
  destroy_drcs(vt_parser->drcs);

  /* Compatible with xterm (CSI > T / CSI > t) */
  vt_parser->set_title_using_hex = vt_parser->get_title_using_hex =
    vt_parser->set_title_using_utf8 = vt_parser->get_title_using_utf8 = 0;
}

static void send_device_status(vt_parser_t *vt_parser, int num, int id) {
  char *seq;
  char amb[] = "\x1b[?884Xn";

  if (num == 6) {
    /* XCPR */
    if ((seq = alloca(6 + DIGIT_STR_LEN(int)+1))) {
      sprintf(seq, "\x1b[?%d;%d;%dR", vt_screen_cursor_logical_row(vt_parser->screen) + 1,
              vt_screen_cursor_logical_col(vt_parser->screen) + 1,
              vt_screen_get_page_id(vt_parser->screen) + 1);
    } else {
      return;
    }
  } else if (num == 15) {
    seq = "\x1b[?13n"; /* No printer */
  } else if (num == 25) {
    seq = "\x1b[?21n"; /* UDKs are locked */
  } else if (num == 26) {
    seq = "\x1b[?27;1;0;0n"; /* North American, Keyboard ready */
  } else if (num == 53 || num == 55) {
    seq = "\x1b[?50n"; /* Locator ready */
  } else if (num == 56) {
    seq = "\x1b[?57;1n"; /* Locator exists */
  } else if (num == 62) {
    seq = "\x1b[0*{"; /* Macro Space (Pn = [num of bytes] / 16 (rounded down) */
  } else if (num == 63) {
    /* checksum = 0 */
    if ((seq = alloca(10+DIGIT_STR_LEN(int)+1))) {
      sprintf(seq, "\x1bP%d!~0000\x1b\\", id);
    } else {
      seq = "\x1bP0!~0000\x1b\\";
    }
  } else if (num == 75) {
    seq = "\x1b[?70n"; /* Ready */
  } else if (num == 85) {
    seq = "\x1b[?83n"; /* Not multiple-session */
  } else if (num == 8840) {
    /* "CSI ? 8840 n" (TNREPTAMB) */

    amb[6] = vt_parser->col_size_of_width_a + 0x30;
    seq = amb;
  } else {
    return;
  }

  vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
}

static void send_device_attributes(vt_parser_t *vt_parser, int rank) {
  char *seq;

  if (rank == 1) {
    if (primary_da && (seq = alloca(4 + strlen(primary_da) + 1))) {
      sprintf(seq, "\x1b[?%sc", primary_da);
    } else {
      /*
       * 1  132-columns
       * 2  Printer
       * 3  ReGIS graphics
       * 4  Sixel graphics
       * 6  Selective erase
       * 7  Soft character set (DRCS)
       * 15 Technical character set
       * 18 Windowing capability
       * 22 Color
       * 29 ANSI text locator (i.e., DEC Locator mode)
       * 52 OSC 52 (https://github.com/arakiken/mlterm/issues/144)
       */
#ifndef NO_IMAGE
      if (HAS_XTERM_LISTENER(vt_parser, set_selection)) {
        seq = "\x1b[?63;1;2;3;4;6;7;15;18;22;29;52c";
      } else {
        seq = "\x1b[?63;1;2;3;4;6;7;15;18;22;29c";
      }
#else
      if (HAS_XTERM_LISTENER(vt_parser, set_selection)) {
        seq = "\x1b[?63;1;2;6;7;15;18;22;29;52c";
      } else {
        seq = "\x1b[?63;1;2;6;7;15;18;22;29c";
      }
#endif
    }
  } else if (rank == 2) {
    if (secondary_da && (seq = alloca(4 + strlen(secondary_da) + 1))) {
      sprintf(seq, "\x1b[>%sc", secondary_da);
    } else {
      /*
       * >=96: vim sets ttymouse=xterm2
       * >=141: vim uses tcap-query.
       * >=277: vim uses sgr mouse tracking.
       * >=279: xterm supports DECSLRM/DECLRMM.
       */
      seq = "\x1b[>24;279;0c";
    }
  } else if (rank == 3) {
    seq = "\x1bP!|000000\x1b\\";
  } else {
    return;
  }

  vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
}

static void send_display_extent(vt_pty_t *pty, u_int cols, u_int rows, int vmargin,
                                int hmargin, int page) {
  char seq[DIGIT_STR_LEN(int) * 5 + 1];

  sprintf(seq, "\x1b[%d;%d;%d;%d;%d\"w", rows, cols, hmargin, vmargin, page);

  vt_write_to_pty(pty, seq, strlen(seq));
}

static void set_use_status_line(vt_parser_t *vt_parser, int ps) {
  u_int width;
  u_int height;

  if (ps <= 1) {
    vt_screen_set_use_status_line(vt_parser->screen, 0);
  } else if (ps == 2) {
    vt_screen_set_use_status_line(vt_parser->screen, 1);
  } else {
    return;
  }

  if (!HAS_XTERM_LISTENER(vt_parser, get_window_size)) {
    width = height = 0;
  } else {
    (*vt_parser->xterm_listener->get_window_size)(vt_parser->xterm_listener->self,
                                                  &width, &height);
  }

  vt_set_pty_winsize(vt_parser->pty, vt_screen_get_logical_cols(vt_parser->screen),
                     vt_screen_get_logical_rows(vt_parser->screen), width, height);
}

/*
 * For string outside escape sequences.
 */
static int increment_str(u_char **str, size_t *left) {
  if (--(*left) == 0) {
    return 0;
  }

  (*str)++;

  return 1;
}

/*
 * For string inside escape sequences.
 * Don't use in OSC and DSC which are terminated by ST including \x1b.
 */
static int inc_str_in_esc_seq(vt_screen_t *screen, u_char **str_p, size_t *left) {
  while (1) {
    if (increment_str(str_p, left) == 0) {
      return 0;
    }

    if (**str_p < 0x20 || 0x7e < **str_p) {
      /*
       * cursor-control characters inside ESC sequences
       */
      if (CTL_LF <= **str_p && **str_p <= CTL_FF) {
        vt_screen_line_feed(screen);
        /* XXX AUTO_CR(vt_parser) is ignored for now. */
      } else if (**str_p == CTL_CR) {
        vt_screen_goto_beg_of_line(screen);
      } else if (**str_p == CTL_BS) {
        vt_screen_go_back(screen, 1, 0);
      } else {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " Ignored 0x%x inside escape sequences.\n", **str_p);
#endif
      }
    } else {
      return 1;
    }
  }
}

/*
 * Set use_c1=0 for 0x9c not to be regarded as ST if str can be encoded by utf8.
 * (UTF-8 uses 0x80-0x9f as printable characters.)
 */
static char *get_pt_in_esc_seq(
    u_char **str, size_t *left,
    int use_c1,       /* OSC is terminated by not only ST(ESC \) but also 0x9c. */
    int bel_terminate /* OSC is terminated by not only ST(ESC \) but also BEL. */
    ) {
  u_char *pt;

  pt = *str;

  while (1) {
    if ((bel_terminate && **str == CTL_BEL) || (use_c1 && **str == 0x9c)) {
      **str = '\0';

      break;
    }

    if (**str == CTL_ESC) {
      if (!increment_str(str, left)) {
        return NULL;
      }

      if (**str == '\\') {
        *((*str) - 1) = '\0';

        break;
      }

      /*
       * Reset position ahead of unprintable character for compat with xterm.
       * e.g.) "\x1bP\x1b[A" is parsed as "\x1b[A"
       */
      (*str) -= 2;
      (*left) += 2;

      return NULL;
    }

    if (!increment_str(str, left)) {
      return NULL;
    }
  }

  return pt;
}

#ifdef USE_VT52
inline static int parse_vt52_escape_sequence(
    vt_parser_t *vt_parser /* vt_parser->r_buf.left must be more than 0 */
    ) {
  u_char *str_p;
  size_t left;

  str_p = CURRENT_STR_P(vt_parser);
  left = vt_parser->r_buf.left;

  if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
    return 0;
  }

  if (*str_p == 'A') {
    vt_screen_go_upward(vt_parser->screen, 1);
  } else if (*str_p == 'B') {
    vt_screen_go_downward(vt_parser->screen, 1);
  } else if (*str_p == 'C') {
    vt_screen_go_forward(vt_parser->screen, 1, 0);
  } else if (*str_p == 'D') {
    vt_screen_go_back(vt_parser->screen, 1, 0);
  } else if (*str_p == 'F') {
    /* Enter graphics mode */
  } else if (*str_p == 'G') {
    /* Exit graphics mode */
  } else if (*str_p == 'H') {
    vt_screen_goto(vt_parser->screen, 0, 0);
  } else if (*str_p == 'I') {
    if (vt_screen_cursor_logical_row(vt_parser->screen) == 0) {
      vt_screen_scroll_downward(vt_parser->screen, 1);
    } else {
      vt_screen_go_upward(vt_parser->screen, 1);
    }
  } else if (*str_p == 'J') {
    vt_screen_clear_below(vt_parser->screen);
  } else if (*str_p == 'K') {
    vt_screen_clear_line_to_right(vt_parser->screen);
  } else if (*str_p == 'Y') {
    int row;
    int col;

    if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
      return 0;
    }

    if (*str_p < ' ') {
      goto end;
    }

    row = *str_p - ' ';

    if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
      return 0;
    }

    if (*str_p < ' ') {
      goto end;
    }

    col = *str_p - ' ';

    vt_screen_goto(vt_parser->screen, col, row);
  } else if (*str_p == 'Z') {
    const char msg[] = "\x1b/Z";

    vt_write_to_pty(vt_parser->pty, msg, sizeof(msg) - 1);
  } else if (*str_p == '=') {
    /* Enter altenate keypad mode */
  } else if (*str_p == '>') {
    /* Exit altenate keypad mode */
  } else if (*str_p == '<') {
    vt_parser->is_vt52_mode = 0;
  } else {
    /* not VT52 control sequence */

    return 1;
  }

end:
  vt_parser->r_buf.left = left - 1;

  return 1;
}
#endif

/*
 * Parse escape/control sequence. Note that *any* valid format sequence
 * is parsed even if mlterm doesn't support it.
 *
 * Return value:
 * 0 => vt_parser->r_buf.left == 0
 * 1 => Finished parsing. (If current vt_parser->r_buf.chars is not escape
 *sequence,
 *      return without doing anthing.)
 */
inline static int parse_vt100_escape_sequence(
    vt_parser_t *vt_parser /* vt_parser->r_buf.left must be more than 0 */
    ) {
  u_char *str_p;
  size_t left;

#if 0
  if (vt_parser->r_buf.left == 0) {
    /* end of string */

    return 1;
  }
#endif

  str_p = CURRENT_STR_P(vt_parser);
  left = vt_parser->r_buf.left;

  if (*str_p == CTL_ESC) {
#ifdef ESCSEQ_DEBUG
    bl_msg_printf("RECEIVED ESCAPE SEQUENCE(current left = %d: ESC", left);
#endif

#ifdef USE_VT52
    if (vt_parser->is_vt52_mode) {
      return parse_vt52_escape_sequence(vt_parser);
    }
#endif

    if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
      return 0;
    }

    if (*str_p == '2') {
      /* "ESC 2" DECCAHT */
      vt_screen_clear_all_tab_stops(vt_parser->screen);
    } else if (*str_p == '6') {
      /* "ESC 6" DECBI */

      vt_screen_go_back(vt_parser->screen, 1, 1);
    } else if (*str_p == '7') {
      /* "ESC 7" save cursor (DECSC) */

      save_cursor(vt_parser);
    } else if (*str_p == '8') {
      /* "ESC 8" restore cursor (DECRC) */

      restore_cursor(vt_parser);
    } else if (*str_p == '9') {
      /* "ESC 9" DECFI */

      vt_screen_go_forward(vt_parser->screen, 1, 1);
    } else if (*str_p == '=') {
      /* "ESC =" application keypad (DECKPAM) */

      vt_parser->is_app_keypad = 1;
    } else if (*str_p == '>') {
      /* "ESC >" normal keypad (DECKPNM) */

      vt_parser->is_app_keypad = 0;
    } else if (*str_p == 'D') {
      /* "ESC D" index(scroll up) (IND) */

      vt_screen_index(vt_parser->screen);
    } else if (*str_p == 'E') {
      /* "ESC E" next line (NEL) */

      vt_screen_line_feed(vt_parser->screen);
      vt_screen_goto_beg_of_line(vt_parser->screen);
    }
#if 0
    else if (*str_p == 'F') {
      /* "ESC F" cursor to lower left corner of screen */
    }
#endif
    else if (*str_p == 'H' || *str_p == '1') {
      /* "ESC H" (HTS) / "ESC 1" (DECHTS) set tab */

      vt_screen_set_tab_stop(vt_parser->screen);
    } else if (*str_p == 'I') {
      /* "ESC I" (HTJ) */
      vt_screen_forward_tabs(vt_parser->screen, 1);
    }
#if 0
    else if (*str_p == 'K') {
      /* "ESC K" (PLD) Partial Line Down */
    } else if (*str_p == 'L') {
      /* "ESC L" (PLU) Partial Line Up */
    }
#endif
    else if (*str_p == 'M') {
      /* "ESC M" reverse index(scroll down) */

      vt_screen_reverse_index(vt_parser->screen);
    } else if (*str_p == 'Z') {
      /* "ESC Z" return terminal id (Obsolete form of CSI c) */

      send_device_attributes(vt_parser, 1);
    } else if (*str_p == 'c') {
      /* "ESC c" full reset (RIS) */

      full_reset(vt_parser);
    }
#if 0
    else if (*str_p == 'l') {
      /* "ESC l" memory lock */
    }
#endif
#if 0
    else if (*str_p == 'm') {
      /* "ESC m" memory unlock */
    }
#endif
    else if (*str_p == '[') {
/*
 * "ESC [" (CSI)
 * CSI P.....P I.....I F
 *     060-077 040-057 100-176
 */

#define MAX_NUM_OF_PS 10

      u_char para_ch = '\0';
      u_char intmed_ch = '\0';
      int ps[MAX_NUM_OF_PS];
      int num;

      num = 0;
      while (1) {
        if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
          return 0;
        }

        if (*str_p == ';') {
          /*
           * "CSI ; n" is regarded as "CSI -1 ; n"
           */
          if (num < MAX_NUM_OF_PS) {
            ps[num++] = -1;
          }
        } else if (0x3c <= *str_p && *str_p <= 0x3f) {
          /* parameter character except numeric, ':' and ';' (< = > ?). */
          if (para_ch == '\0') {
            para_ch = *str_p;
          }
        } else {
          if ('0' <= *str_p && *str_p <= '9') {
            int _ps;

            if (*str_p == '0') {
              /* 000000001 -> 01 */
              while (left > 1 && *(str_p + 1) == '0') {
                str_p++;
                left--;
              }
            }

            _ps = *str_p - '0';
            while (1) {
              if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
                return 0;
              }

              if (*str_p < '0' || '9' < *str_p) {
                break;
              }

              _ps = _ps * 10 + (*str_p - '0');
            }

            if (num < MAX_NUM_OF_PS) {
#ifdef MAX_PS_DIGIT
              if (_ps > MAX_PS_DIGIT) {
                _ps = MAX_PS_DIGIT;
              }
#endif
              ps[num++] = _ps;
            }
          }

          if (*str_p < 0x30 || 0x3f < *str_p) {
            /* Is not a parameter character(0x30-0x3f). */

            /*
             * "CSI 0 n" is *not* regarded as "CSI 0 ; -1 n"
             * "CSI n" is regarded as "CSI -1 n"
             * "CSI 0 ; n" is regarded as "CSI 0 ; -1 n"
             * "CSI ; n" which has been already regarded as
             * "CSI -1 ; n" is regarded as "CSI -1 ; -1 n"
             */
            if (num == 0 || (*(str_p - 1) == ';' && num < MAX_NUM_OF_PS)) {
              ps[num++] = -1;
            }

            /* num is always greater than 0 */

            break;
          }
        }
      }

      /*
       * Intermediate(0x20-0x2f) characters.
       * (Unexpected paremeter(0x30-0x3f) characters are also ignored.)
       */
      while (0x20 <= *str_p && *str_p <= 0x3f) {
        if (*str_p <= 0x2f && intmed_ch == '\0') {
          intmed_ch = *str_p;
        }

        if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
          return 0;
        }
      }

      if (para_ch == '?') {
        if (intmed_ch == '$') {
          if (*str_p == 'p' && num > 0) {
            /* "CSI ? $ p" DECRQM */

            char seq[3 + DIGIT_STR_LEN(u_int) + 5];
            sprintf(seq, "\x1b[?%d;%d$y", ps[0],
                    (ps[0] >= VTMODE(0)) ? 0 : get_vtmode(vt_parser, ps[0]));
            vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
          }
        } else if (*str_p == 'h') {
          /* "CSI ? h" DEC Private Mode Set (DECSET) */
          int count;

          for (count = 0; count < num; count++) {
            if (ps[count] < VTMODE(0)) {
              set_vtmode(vt_parser, ps[count], 1);
            }
          }
        } else if (*str_p == 'i') {
          /* "CSI ? i" (DECMC) */

          if (ps[0] <= 0 || ps[0] == 10) {
            snapshot(vt_parser, VT_UTF8, vt_pty_get_slave_name(vt_parser->pty) + 5 /* skip /dev/ */,
                     WCA_SCREEN);
          } else if (ps[0] == 1) {
            snapshot(vt_parser, VT_UTF8, vt_pty_get_slave_name(vt_parser->pty) + 5 /* skip /dev/ */,
                     WCA_CURSOR_LINE);
          } else if (ps[0] == 11) {
            snapshot(vt_parser, VT_UTF8, vt_pty_get_slave_name(vt_parser->pty) + 5 /* skip /dev/ */,
                     WCA_ALL);
          }
        } else if (*str_p == 'l') {
          /* "CSI ? l" DEC Private Mode Reset (DECRST) */
          int count;

          for (count = 0; count < num; count++) {
            if (ps[count] < VTMODE(0)) {
              set_vtmode(vt_parser, ps[count], 0);
            }
          }
        } else if (*str_p == 'r') {
          /* "CSI ? r" Restore DEC Private Mode (xterm) */
          int count;

          for (count = 0; count < num; count++) {
            if (ps[count] < VTMODE(0)) {
              int idx;
              if ((idx = vtmode_num_to_idx(ps[count])) != -1) {
                set_vtmode(vt_parser, ps[count], GET_SAVED_VTMODE_FLAG2(vt_parser, idx));
              }
            }
          }
        } else if (*str_p == 's') {
          /* "CSI ? s" Save DEC Private Mode (xterm) */
          int count;

          for (count = 0; count < num; count++) {
            if (ps[count] < VTMODE(0)) {
              int idx;
              if ((idx = vtmode_num_to_idx(ps[count])) != -1) {
                if (GET_VTMODE_FLAG(vt_parser, idx)) {
                  vt_parser->saved_vtmode_flags[DIV32(idx)] |= (SHIFT_FLAG(idx));
                } else {
                  vt_parser->saved_vtmode_flags[DIV32(idx)] &= ~(SHIFT_FLAG(idx));
                }
              }
            }
          }
        } else if (*str_p == 'n') {
          /* "CSI ? n" Device Status Report (DSR) */

          send_device_status(vt_parser, ps[0], num == 2 ? ps[1] : 0);
        } else if (*str_p == 'J') {
          /* "CSI ? J" DECSED (Selective Erase Display) */
          vt_protect_store_t *save;

          if (ps[0] <= 0) {
            save = vt_screen_save_protected_chars(vt_parser->screen,
                                                  -1 /* cursor row */,
                                                  vt_screen_get_logical_rows(vt_parser->screen) - 1,
                                                  0);
            vt_screen_clear_below(vt_parser->screen);
            vt_screen_restore_protected_chars(vt_parser->screen, save);
          } else if (ps[0] == 1) {
            save = vt_screen_save_protected_chars(vt_parser->screen, 0,
                                                  -1 /* cursor row */, 0);
            vt_screen_clear_above(vt_parser->screen);
            vt_screen_restore_protected_chars(vt_parser->screen, save);
          } else if (ps[0] == 2) {
            save = vt_screen_save_protected_chars(vt_parser->screen, 0,
                                                  vt_screen_get_logical_rows(vt_parser->screen) - 1,
                                                  0);
            clear_display_all(vt_parser);
            vt_screen_restore_protected_chars(vt_parser->screen, save);
          }
        } else if (*str_p == 'K') {
          /* "CSI ? K" DECSEL (Selective Erase Line) */
          vt_protect_store_t *save;

          save = vt_screen_save_protected_chars(vt_parser->screen,
                                                -1 /* cursor row */,
                                                -1 /* cursor row */, 0);
          if (ps[0] <= 0) {
            vt_screen_clear_line_to_right(vt_parser->screen);
          } else if (ps[0] == 1) {
            vt_screen_clear_line_to_left(vt_parser->screen);
          } else if (ps[0] == 2) {
            clear_line_all(vt_parser);
          }
          vt_screen_restore_protected_chars(vt_parser->screen, save);
        } else if (*str_p == 'S') {
          /*
           * "CSI ? Pi;Pa;Pv S" (Xterm original)
           * The number of palettes mlterm supports is 1024 alone.
           */
          char *seq;

          if (num == 3 && ps[0] == 1 &&
              (ps[1] == 1 /* read */ || ps[1] == 2 /* reset to default */ ||
               ps[1] == 3 /* set (ps[2] is ignored) */ ||
               ps[1] == 4 /* read the max allowd value */)) {
            seq = "\x1b[?1;0;1024S";
          } else {
            seq = "\x1b[?1;3;0S";
          }
          vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
        } else if (*str_p == 'W') {
          /* "CSI ? W"  DECST8C */

          vt_screen_set_tab_size(vt_parser->screen, 8);
        }
#if 0
        else if (*str_p == 'r') {
          /* "CSI ? r"  Restore DEC Private Mode */
        }
#endif
#if 0
        else if (*str_p == 's') {
          /* "CSI ? s" Save DEC Private Mode */
        }
#endif
        /* Other final character */
        else if (0x40 <= *str_p && *str_p <= 0x7e) {
#ifdef DEBUG
          debug_print_unknown("ESC [ ? %c\n", *str_p);
#endif
        } else {
/* not VT100 control sequence. */

#ifdef ESCSEQ_DEBUG
          bl_msg_printf("=> not VT100 control sequence.\n");
#endif

          return 1;
        }
      } else if (para_ch == '<') {
        /* Teraterm compatible IME control sequence */

        if (*str_p == 'r') {
          /* Restore IME state */
          if (vt_parser->im_is_active != im_is_active(vt_parser)) {
            switch_im_mode(vt_parser);
          }
        } else if (*str_p == 's') {
          /* Save IME state */

          vt_parser->im_is_active = im_is_active(vt_parser);
        } else if (*str_p == 't') {
          /* ps[0] = 0, -1 (Close), ps[0] = 1 (Open) */

          if (ps[0] == -1)
            ps[0] = 0;
          if (ps[0] != im_is_active(vt_parser)) {
            switch_im_mode(vt_parser);
          }
        }
      } else if (para_ch == '>') {
        if (*str_p == 'c') {
          /* "CSI > c" Secondary DA (DA2) */

          send_device_attributes(vt_parser, 2);
        } else if (*str_p == 'm') {
          /* "CSI > m" */

          if (ps[0] == -1) {
            /* reset to initial value. */
            set_modkey_mode(vt_parser, 1, 2);
            set_modkey_mode(vt_parser, 2, 2);
            set_modkey_mode(vt_parser, 4, 0);
          } else {
            if (num == 1 || ps[1] == -1) {
              if (ps[0] == 1 || /* modifyCursorKeys */
                  ps[0] == 2)   /* modifyFunctionKeys */
              {
                /* The default is 2. */
                ps[1] = 2;
              } else /* if( ps[0] == 4) */
              {
                /*
                 * modifyOtherKeys
                 * The default is 0.
                 */
                ps[1] = 0;
              }
            }

            set_modkey_mode(vt_parser, ps[0], ps[1]);
          }
        } else if (*str_p == 'n') {
          /* "CSI > n" */

          if (num == 1) {
            if (ps[0] == -1) {
              ps[0] = 2;
            }

            /* -1: don't send modifier key code. */
            set_modkey_mode(vt_parser, ps[0], -1);
          }
        } else if (*str_p == 'p') {
          /* "CSI > p" pointer mode */

          vt_parser->hide_pointer_mode = ps[0];
        } else if (*str_p == 'q') {
          /* "CSI > q" XTVERSION */

          const char xtversion[] = "\x1bP>|mlterm(3.9.4)\x1b\\";

          vt_write_to_pty(vt_parser->pty, xtversion, sizeof(xtversion) - 1);
        } else if (*str_p == 't') {
          /* "CSI > t" */

          int count;
          for (count = 0; count < num; count++) {
            if (ps[count] == 0) {
              vt_parser->set_title_using_hex = 1;
            } else if (ps[count] == 1) {
              vt_parser->get_title_using_hex = 1;
            } else if (ps[count] == 2) {
              vt_parser->set_title_using_utf8 = 1;
            } else if (ps[count] == 3) {
              vt_parser->get_title_using_utf8 = 1;
            }
          }
        } else if (*str_p == 'T') {
          /* "CSI > T" */

          int count;
          for (count = 0; count < num; count++) {
            if (ps[count] == 0) {
              vt_parser->set_title_using_hex = 0;
            } else if (ps[count] == 1) {
              vt_parser->get_title_using_hex = 0;
            } else if (ps[count] == 2) {
              vt_parser->set_title_using_utf8 = 0;
            } else if (ps[count] == 3) {
              vt_parser->get_title_using_utf8 = 0;
            }
          }
        } else {
          /*
           * "CSI > c"
           */
        }
      } else if (para_ch == '=') {
        if (*str_p == 'c') {
          /* "CSI = c" Tertiary DA (DA3) */

          send_device_attributes(vt_parser, 3);
        }
      } else if (intmed_ch == '!') {
        if (*str_p == 'p') {
          /* "CSI ! p" Soft Terminal Reset (DECSTR) */

          soft_reset(vt_parser);
        }
      } else if (intmed_ch == '$') {
        if (*str_p == 'p') {
          /* "CSI $ p" DECRQM */

          if (num > 0) {
            char seq[2 + DIGIT_STR_LEN(u_int) + 5];
            sprintf(seq, "\x1b[%d;%d$y", ps[0], get_vtmode(vt_parser, VTMODE(ps[0])));
            vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
          }
        } else if (*str_p == '|') {
          /* "CSI $ |" DECSCPP */

          /*
           * DECSCPP doesns't clear screen, reset scroll regions or moves the cursor
           * as does DECCOLM.
           */

          if (ps[0] <= 0 || ps[0] == 80) {
            resize(vt_parser, 80, -1, 1);
          } else if (ps[0] == 132) {
            resize(vt_parser, 132, -1, 1);
          }
        } else if (*str_p == '}') {
          /* "CSI $ }" DECSASD */

          if (ps[0] <= 0) {
            vt_focus_main_screen(vt_parser->screen);
          } else if (ps[0] == 1) {
            vt_focus_status_line(vt_parser->screen);
          }
        } else if (*str_p == '~') {
          /* "CSI $ ~" DECSSDT */

          set_use_status_line(vt_parser, ps[0]);
        } else if (*str_p == 'u') {
          if (ps[0] == 1) {
            /* "CSI 1 $ u" DECRQTSR */
            vt_write_to_pty(vt_parser->pty, "\x1bP0$s\x1b\\", 7);
          } else if (ps[0] == 2) {
            if (num == 2) {
              /* "CSI 2;Pu $ u" DECCTR */
              report_color_table(vt_parser, ps[1]);
            }
          }
        } else if (*str_p == 'w') {
          /* "CSI $ w" DECRQPSR */
          if (num == 1) {
            report_presentation_state(vt_parser, ps[0]);
          }
        } else {
          int count;

          if (*str_p == 'x') {
            /* DECFRA: Move Pc to the end */
            int tmp;

            tmp = ps[0];
            memmove(ps, ps + 1, sizeof(ps[0]) * 4);
            ps[4] = tmp;
            num--;
          }

          /* Set the default values to the 0-3 parameters. */
          for (count = 0; count < 4; count++) {
            if (count >= num || ps[count] <= 0) {
              if (count == 2) {
                ps[count] = vt_screen_get_logical_rows(vt_parser->screen);
              } else if (count == 3) {
                ps[count] = vt_screen_get_logical_cols(vt_parser->screen);
              } else {
                ps[count] = 1;
              }
            }
          }

          if (ps[3] >= ps[1] && ps[2] >= ps[0]) {
            if (*str_p == 'z') {
              /* "CSI ... $ z" DECERA */
              vt_screen_erase_area(vt_parser->screen, ps[1] - 1, ps[0] - 1, ps[3] - ps[1] + 1,
                                   ps[2] - ps[0] + 1);
            } else if (*str_p == '{') {
              /* "CSI ... $ {" DECSERA */
              vt_protect_store_t *save;
              save = vt_screen_save_protected_chars(vt_parser->screen, ps[0] - 1, ps[2] - 1, 1);
              vt_screen_erase_area(vt_parser->screen, ps[1] - 1, ps[0] - 1, ps[3] - ps[1] + 1,
                                   ps[2] - ps[0] + 1);
              vt_screen_restore_protected_chars(vt_parser->screen, save);
            } else if (*str_p == 'v' && (num == 5 || num >= 8)) {
              /* "CSI ... $ v" DECCRA */
              if (num == 5) {
                ps[5] = ps[6] = ps[7] = 1; /* default values of xterm */
              } else {
                for (count = 4; count < 8; count++) {
                  if (ps[count] <= 0) {
                    ps[count] = 1;
                  }
                }
              }
              vt_screen_copy_area(vt_parser->screen, ps[1] - 1, ps[0] - 1, ps[3] - ps[1] + 1,
                                  ps[2] - ps[0] + 1, ps[4] - 1, ps[6] - 1, ps[5] - 1, ps[7] - 1);
            } else if (*str_p == 'x') {
              /* "CSI ... $ x" DECFRA */
              if (!iscntrl(ps[4])) {
                vt_screen_fill_area(vt_parser->screen, ps[4], vt_parser->is_protected,
                                    ps[1] - 1, ps[0] - 1, ps[3] - ps[1] + 1, ps[2] - ps[0] + 1);
              }
            } else if (*str_p == 'r') {
              /* "CSI Pt;Pl;Pb;Pr;...$r" DECCARA */

              for (count = 4; count < num; count++) {
                vt_screen_change_attr_area(vt_parser->screen, ps[1] - 1, ps[0] - 1,
                                           ps[3] - ps[1] + 1, ps[2] - ps[0] + 1, ps[count]);
              }
            } else if (*str_p == 't') {
              /* "CSI Pt;Pl;Pb;Pr;...$t" DECRARA */

              for (count = 4; count < num; count++) {
                vt_screen_reverse_attr_area(vt_parser->screen, ps[1] - 1, ps[0] - 1,
                                            ps[3] - ps[1] + 1, ps[2] - ps[0] + 1, ps[count]);
              }
            }
          }
        }
      } else if (intmed_ch == ' ') {
        if (*str_p == 'q') {
          /* "CSI SP q" DECSCUSR */

          if (ps[0] < 2) {
            vt_parser->cursor_style = CS_BLOCK|CS_BLINK;
          } else if (ps[0] == 2) {
            vt_parser->cursor_style = CS_BLOCK;
          } else if (ps[0] == 3) {
            vt_parser->cursor_style = CS_UNDERLINE|CS_BLINK;
          } else if (ps[0] == 4) {
            vt_parser->cursor_style = CS_UNDERLINE;
          } else if (ps[0] == 5) {
            vt_parser->cursor_style = CS_BAR|CS_BLINK;
          } else if (ps[0] == 6) {
            vt_parser->cursor_style = CS_BAR;
          }
        } else {
          if (ps[0] <= 0) {
            ps[0] = 1;
          }

          if (*str_p == '@') {
            /* CSI SP @ (SL) */
            vt_screen_scroll_leftward(vt_parser->screen, ps[0]);
          } else if (*str_p == 'A') {
            /* CSI SP A (SR) */
            vt_screen_scroll_rightward(vt_parser->screen, ps[0]);
          } else if (*str_p == 'P') {
            /* CSI SP P (PPA) */
            vt_screen_goto_page(vt_parser->screen, ps[0] - 1);
          } else if (*str_p == 'Q') {
            /* CSI SP Q (PPR) */
            vt_screen_goto_next_page(vt_parser->screen, ps[0]);
          } else if (*str_p == 'R') {
            /* CSI SP R (PPB) */
            vt_screen_goto_prev_page(vt_parser->screen, ps[0]);
          } else {
            /*
             * "CSI SP t"(DECSWBV), "CSI SP u"(DECSMBV)
             */
          }
        }
      } else if (intmed_ch == '*') {
        if (ps[0] == -1) {
          ps[0] = 0;
        }

        if (*str_p == 'z') {
          /* "CSI Pn * z" DECINVM */

          invoke_macro(vt_parser, ps[0]);
        } else if (*str_p == 'x') {
          /* "CSI Pn * x " DECSACE */

          vt_screen_set_use_rect_attr_select(vt_parser->screen, ps[0] == 2);
        } else if (*str_p == '|') {
          /* "CSI Pn * |" DECSNLS */
          resize(vt_parser, -1, ps[0], 1);
        } else if (*str_p == 'y') {
          /* "CSI Pn * y" DECRQCRA */
          if (num >= 1) {
            u_int16_t checksum;
            char seq[6+DIGIT_STR_LEN(ps[0])+4+1];

            if (ps[1] == 0) {
#if 0
              /* Ignore following parameters (https://vt100.net/docs/vt510-rm/DECRQCRA.html) */
              num = 1;
#else
              /* Compatible with xterm */
              ps[1] = 1;
#endif
            }

            switch(num) {
            case 1:
            case 2:
              ps[2] = 1;

            case 3:
              ps[3] = 1;

            case 4:
              ps[4] = vt_screen_get_rows(vt_parser->screen);

            case 5:
              ps[5] = vt_screen_get_cols(vt_parser->screen);
            }

            if (num == 1) {
              int page;

              checksum = 0;
              for (page = 0; page <= MAX_PAGE_ID; page++) {
                int i = vt_screen_get_checksum(vt_parser->screen, ps[3] - 1, ps[2] - 1,
                                               ps[5] - ps[3] + 1, ps[4] - ps[2] + 1, page);
                checksum += i;
              }
            } else {
              checksum = vt_screen_get_checksum(vt_parser->screen, ps[3] - 1, ps[2] - 1,
                                                ps[5] - ps[3] + 1, ps[4] - ps[2] + 1, ps[1] - 1);
            }

            sprintf(seq, "\x1bP%d!~%.4x\x1b\\", ps[0], checksum);
            vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
          }
        }
      } else if (intmed_ch == '\'') {
        if (*str_p == '|') {
          /* "CSI ' |" DECRQLP */

          request_locator(vt_parser);
        } else if (*str_p == '{') {
          /* "CSI Pn ' {" DECSLE */
          int count;

          for (count = 0; count < num; count++) {
            if (ps[count] == 1) {
              vt_parser->locator_mode |= LOCATOR_BUTTON_DOWN;
            } else if (ps[count] == 2) {
              vt_parser->locator_mode &= ~LOCATOR_BUTTON_DOWN;
            } else if (ps[count] == 3) {
              vt_parser->locator_mode |= LOCATOR_BUTTON_UP;
            } else if (ps[count] == 4) {
              vt_parser->locator_mode &= ~LOCATOR_BUTTON_UP;
            } else {
              vt_parser->locator_mode &= ~(LOCATOR_BUTTON_UP | LOCATOR_BUTTON_DOWN);
            }
          }
        } else if (*str_p == '}') {
          /* "CSI ' }" DECIC */

          if (ps[0] <= 0) {
            ps[0] = 1;
          }

          vt_screen_scroll_rightward_from_cursor(vt_parser->screen, ps[0]);
        } else if (*str_p == '~') {
          /* "CSI ' ~" DECDC */

          if (ps[0] <= 0) {
            ps[0] = 1;
          }

          vt_screen_scroll_leftward_from_cursor(vt_parser->screen, ps[0]);
        } else if (*str_p == 'w') {
          /* "CSI ' w" DECEFR Filter Rectangle */

          if (num == 4) {
#if 0
            if (top > ps[0] || left > ps[1] || bottom < ps[2] || right < ps[3]) {
              /*
               * XXX
               * An outside rectangle event should
               * be sent immediately.
               */
            }
#endif

            vt_parser->loc_filter.top = ps[0];
            vt_parser->loc_filter.left = ps[1];
            vt_parser->loc_filter.bottom = ps[2];
            vt_parser->loc_filter.right = ps[3];
          }

          vt_parser->locator_mode |= LOCATOR_FILTER_RECT;
        } else if (*str_p == 'z') {
          /* "CSI ' z" DECELR */

          vt_parser->locator_mode &= ~LOCATOR_FILTER_RECT;
          memset(&vt_parser->loc_filter, 0, sizeof(vt_parser->loc_filter));
          if (ps[0] <= 0) {
            if (vt_parser->mouse_mode >= LOCATOR_REPORT) {
              set_mouse_report(vt_parser, 0);
            }
          } else {
            vt_parser->locator_mode |= ps[0] == 2 ? LOCATOR_ONESHOT : 0;

            set_mouse_report(vt_parser, (num == 1 || ps[1] <= 0 || ps[1] == 2)
                                               ? LOCATOR_REPORT
                                               : (LOCATOR_REPORT|MOUSE_REPORT_BY_PIXEL));
          }
        }
      } else if (intmed_ch == '\"') {
        if (*str_p == 'v') {
          /* "CSI " v" DECRQDE */

          send_display_extent(vt_parser->pty, vt_screen_get_logical_cols(vt_parser->screen),
                              vt_screen_get_logical_rows(vt_parser->screen),
                              vt_parser->screen->edit->hmargin_beg + 1,
                              vt_parser->screen->edit->vmargin_beg + 1,
                              vt_screen_get_page_id(vt_parser->screen) + 1);
        } else if (*str_p == 'q') {
          /* "CSI " q" DECSCA */

          if (ps[0] <= 0 || ps[0] == 2) {
            vt_parser->is_protected = 0;
          } else if (ps[0] == 1) {
            vt_parser->is_protected = 1;
          }
        } else {
          /* "CSI " p"(DECSCL) etc */
        }
      } else if (intmed_ch == ',') {
        if (*str_p == '|') {
          /* "CSI Ps1;Ps2;Ps3,|" (DECAC) */

          if (num == 3 && ps[0] == 1 && ((u_int)ps[1]) <= 255 && ((u_int)ps[2]) <= 255) {
            config_protocol_set_simple(vt_parser, "fg_color", vt_get_color_name(ps[1]), 0);
            config_protocol_set_simple(vt_parser, "bg_color", vt_get_color_name(ps[2]), 1);
          }
        } else if (*str_p == '}') {
          /* "CSI Ps1;Ps2;Ps3,}" (DECATC) */

          if (num == 3 && ((u_int)ps[0]) <= 15 && ((u_int)ps[1]) <= 255 && ((u_int)ps[2] <= 255)) {
            int idx = alt_color_idxs[ps[0]];
            vt_parser->alt_colors.flags |= (1 << idx);
            vt_parser->alt_colors.fg[idx] = ps[1];
            vt_parser->alt_colors.bg[idx] = ps[2];
          }
        }
      } else if (intmed_ch == ')') {
        if (*str_p == '{') {
          /* "CSI ) {" DECSTGLT */
          if (ps[0] <= 0) {
            vt_parser->use_ansi_colors = 0;
          } else if (ps[0] <= 3) {
            /*
             * ps[0] == 1 or 2 enables "Alternate color", but mlterm always enables it
             * if DECATC specifies alternate colors.
             */
            vt_parser->use_ansi_colors = 1;
          }
        }
      } else if (intmed_ch == '+') {
        if (*str_p == 'p') {
          /* "CSI + p" DECSR */

          full_reset(vt_parser);

          if (ps[0] >= 0) {
            char seq[2+DIGIT_STR_LEN(ps[0])+3];

            sprintf(seq, "\x1b[%d*q", ps[0]);
            vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
          }
        }
      }
      /* Other para_ch(0x3a-0x3f) or intmed_ch(0x20-0x2f) */
      else if (para_ch || intmed_ch) {
#ifdef DEBUG
        debug_print_unknown("ESC [ %c %c\n", para_ch, intmed_ch, *str_p);
#endif
      } else if (*str_p == '@') {
        /* "CSI @" insert blank chars (ICH) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        /* inserting ps[0] blank characters. */
        vt_screen_insert_blank_chars(vt_parser->screen, ps[0]);
      } else if (*str_p == 'A' || *str_p == 'k') {
        /* "CSI A" = CUU , "CSI k" = VPB */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_upward(vt_parser->screen, ps[0]);
      } else if (*str_p == 'B' || *str_p == 'e') {
        /* "CSI B" = CUD , "CSI e" = VPR */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_downward(vt_parser->screen, ps[0]);
      } else if (*str_p == 'C' || *str_p == 'a') {
        /* "CSI C" = CUF , "CSI a" = HPR */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_forward(vt_parser->screen, ps[0], 0);
      } else if (*str_p == 'D' || *str_p == 'j') {
        /* "CSI D" = CUB , "CSI j" = HPB */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_back(vt_parser->screen, ps[0], 0);
      } else if (*str_p == 'E') {
        /* "CSI E" down and goto first column (CNL) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_downward(vt_parser->screen, ps[0]);
        vt_screen_goto_beg_of_line(vt_parser->screen);
      } else if (*str_p == 'F') {
        /* "CSI F" up and goto first column (CPL) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_upward(vt_parser->screen, ps[0]);
        vt_screen_goto_beg_of_line(vt_parser->screen);
      } else if (*str_p == 'G' || *str_p == '`') {
        /*
         * "CSI G" (CHA), "CSI `" (HPA)
         * cursor position absolute.
         */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_horizontally(vt_parser->screen, ps[0] - 1);
      } else if (*str_p == 'H' || *str_p == 'f') {
        /* "CSI H" (CUP) "CSI f" (HVP) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        if (num <= 1 || ps[1] <= 0) {
          ps[1] = 1;
        }

        vt_screen_goto(vt_parser->screen, ps[1] - 1, ps[0] - 1);
      } else if (*str_p == 'I') {
        /* "CSI I" cursor forward tabulation (CHT) */

        if (ps[0] == -1) {
          /*
           * "CSI 0 I" => No tabulation.
           * "CSI I" => 1 taburation.
           */
          ps[0] = 1;
        }

        vt_screen_forward_tabs(vt_parser->screen, ps[0]);
      } else if (*str_p == 'J') {
        /* "CSI J" Erase in Display (ED) */

        if (ps[0] <= 0) {
          vt_screen_clear_below(vt_parser->screen);
        } else if (ps[0] == 1) {
          vt_screen_clear_above(vt_parser->screen);
        } else if (ps[0] == 2) {
          clear_display_all(vt_parser);
        }
#if 1
        /* xterm compatible (Tera Term 4.105 or later also supports) */
        else if (ps[0] == 3) {
          u_int size = vt_screen_get_log_size(vt_parser->screen);
          int unlimited = vt_screen_log_size_is_unlimited(vt_parser->screen);

          vt_screen_change_log_size(vt_parser->screen, 0);
          if (unlimited) {
            vt_screen_unlimit_log_size(vt_parser->screen);
          } else {
            vt_screen_change_log_size(vt_parser->screen, size);
          }
        }
#endif
      } else if (*str_p == 'K') {
        /* "CSI K" Erase in Line (EL) */

        if (ps[0] <= 0) {
          vt_screen_clear_line_to_right(vt_parser->screen);
        } else if (ps[0] == 1) {
          vt_screen_clear_line_to_left(vt_parser->screen);
        } else if (ps[0] == 2) {
          clear_line_all(vt_parser);
        }
      } else if (*str_p == 'L') {
        /* "CSI L" IL */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_insert_new_lines(vt_parser->screen, ps[0]);
      } else if (*str_p == 'M') {
        /* "CSI M" DL */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_delete_lines(vt_parser->screen, ps[0]);
      } else if (*str_p == 'P') {
        /* "CSI P" delete chars (DCH) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_delete_cols(vt_parser->screen, ps[0]);
      } else if (*str_p == 'S') {
        /* "CSI S" scroll up (SU) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_scroll_upward(vt_parser->screen, ps[0]);
      } else if (*str_p == 'T') {
        /* "CSI T" scroll down (SD) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_scroll_downward(vt_parser->screen, ps[0]);
      } else if (*str_p == 'U') {
        /* "CSI U" (NP) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_goto_next_page(vt_parser->screen, ps[0]);
        vt_screen_goto_home(vt_parser->screen);
      } else if (*str_p == 'V') {
        /* "CSI V" (PP) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_goto_prev_page(vt_parser->screen, ps[0]);
        vt_screen_goto_home(vt_parser->screen);
      } else if (*str_p == 'W') {
        /* "CSI W" Cursor Tabluation Control (CTC) */

        if (ps[0] <= 0) {
          vt_screen_set_tab_stop(vt_parser->screen);
        } else if (ps[0] == 2) {
          vt_screen_clear_tab_stop(vt_parser->screen);
        } else if (ps[0] == 4 || ps[0] == 5) {
          vt_screen_clear_all_tab_stops(vt_parser->screen);
        }
      } else if (*str_p == 'X') {
        /* "CSI X" erase characters (ECH) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_clear_cols(vt_parser->screen, ps[0]);
      } else if (*str_p == 'Z') {
        /* "CSI Z" cursor backward tabulation (CBT) */

        if (ps[0] == -1) {
          /*
           * "CSI 0 Z" => No tabulation.
           * "CSI Z" => 1 taburation.
           */
          ps[0] = 1;
        }

        vt_screen_backward_tabs(vt_parser->screen, ps[0]);
      } else if (*str_p == 'b') {
        /* "CSI b" repeat the preceding graphic character(REP) */

        if (vt_parser->w_buf.last_ch) {
          int count;

          if (ps[0] <= 0) {
            ps[0] = 1;
          }

          for (count = 0; count < ps[0]; count++) {
            (*vt_parser->w_buf.output_func)(vt_parser->screen, vt_parser->w_buf.last_ch,
                                               1);
          }

          vt_parser->w_buf.last_ch = NULL;
        }
      } else if (*str_p == 'c') {
        /* "CSI c" Primary DA (DA1) */

        send_device_attributes(vt_parser, 1);
      } else if (*str_p == 'd') {
        /* "CSI d" line position absolute(VPA) */

        if (ps[0] <= 0) {
          ps[0] = 1;
        }

        vt_screen_go_vertically(vt_parser->screen, ps[0] - 1);
      } else if (*str_p == 'g') {
        /* "CSI g" tab clear (TBC) */

        if (ps[0] <= 0) {
          vt_screen_clear_tab_stop(vt_parser->screen);
        } else if (ps[0] == 3) {
          vt_screen_clear_all_tab_stops(vt_parser->screen);
        }
      } else if (*str_p == 'h') {
        /* "CSI h" (SM) */
        int count;

        for (count = 0; count < num; count++) {
          set_vtmode(vt_parser, VTMODE(ps[count]), 1);
        }
      } else if (*str_p == 'i') {
        /* "CSI i" (MC) */

        if (ps[0] <= 0) {
          snapshot(vt_parser, VT_UTF8, vt_pty_get_slave_name(vt_parser->pty) + 5 /* skip /dev/ */,
                   WCA_SCREEN);
        } else if (ps[0] == 1) {
          snapshot(vt_parser, VT_UTF8, vt_pty_get_slave_name(vt_parser->pty) + 5 /* skip /dev/ */,
                   WCA_CURSOR_LINE);
        }
      } else if (*str_p == 'l') {
        /* "CSI l" (RM) */
        int count;

        for (count = 0; count < num; count++) {
          set_vtmode(vt_parser, VTMODE(ps[count]), 0);
        }
      } else if (*str_p == 'm') {
        /* "CSI m" (SGR) */
        int count;

        for (count = 0; count < num;) {
          int proceed;

          if ((proceed = change_char_fine_color(vt_parser, ps + count, num - count))) {
            count += proceed;
          } else {
            if (ps[count] < 0) {
              ps[count] = 0;
            }

            change_char_attr(vt_parser, ps[count++]);
          }
        }
      } else if (*str_p == 'n') {
        /* "CSI n" Device Status Report (DSR) */

        if (ps[0] == 5) {
          /* Operating Status */
          vt_write_to_pty(vt_parser->pty, "\x1b[0n", 4);
        } else if (ps[0] == 6) {
          /* CPR */
          char seq[4 + DIGIT_STR_LEN(u_int) * 2 + 1];

          sprintf(seq, "\x1b[%d;%dR", vt_screen_cursor_logical_row(vt_parser->screen) + 1,
                  vt_screen_cursor_logical_col(vt_parser->screen) + 1);

          vt_write_to_pty(vt_parser->pty, seq, strlen(seq));
        }
      } else if (*str_p == 'r') {
        /* "CSI r" set scroll region (DECSTBM) */

        if (ps[0] < 0) {
          ps[0] = 0;
        }

        if (num <= 1 || ps[1] < 0) {
          ps[1] = 0;
        }

        vt_screen_set_vmargin(vt_parser->screen, ps[0] - 1, ps[1] - 1);
        vt_screen_goto(vt_parser->screen, 0, 0);
      } else if (*str_p == 's') {
        /* "CSI s" SCOSC or DECSLRM */

        if (num <= 1 || ps[1] <= 0) {
          ps[1] = vt_screen_get_logical_cols(vt_parser->screen);
        }

        if (vt_screen_set_hmargin(vt_parser->screen, ps[0] <= 0 ? 0 : ps[0] - 1, ps[1] - 1)) {
          vt_screen_goto(vt_parser->screen, 0, 0);
        } else if (num == 1 && ps[0] == -1) {
          save_cursor(vt_parser);
        }
      } else if (*str_p == 't') {
        /* "CSI t" */

        if (ps[0] == 4 || ps[0] == 8) {
          if (num == 2) {
            ps[2] = -1;
            num = 3;
          }

          if (num == 3) {
            resize(vt_parser, ps[2], ps[1], ps[0] == 8);
          }
        } else if (ps[0] == 9) {
          if (num == 2 && 0 <= ps[1] && ps[1] <= 3) {
            int flag;

            if (ps[1] >= 2) {
              flag = ps[1]; /* MAXIMIZE VERTICALLY or HORIZONTALLY */
            } else {
              flag = (ps[1] == 0 ? 1 /* UNMAXIMIZE */ : 4 /* MAXIMIZE FULL */);
            }

            set_maximize(vt_parser, flag);
          }
        } else if (ps[0] == 10) {
          /* XXX full screen is not supported for now. */
        } else if (ps[0] == 7) {
          char cmd[] = "update_all";
          config_protocol_set(vt_parser, cmd, 0);
        } else if (ps[0] == 11) {
          vt_write_to_pty(vt_parser->pty, "\x1b[1t", 4); /* XXX always non-iconified */
        } else if (ps[0] == 13) {
          vt_write_to_pty(vt_parser->pty, "\x1b[3;0;0t", 8);
        } else if (ps[0] == 14) {
          report_window_size(vt_parser, 0);
        } else if (ps[0] == 15) {
          report_display_size(vt_parser, 0);
        } else if (ps[0] == 16) {
          report_cell_size(vt_parser);
        } else if (ps[0] == 18) {
          report_window_size(vt_parser, 1);
        } else if (ps[0] == 19) {
          report_display_size(vt_parser, 1);
        } else if (ps[0] == 20) {
          report_window_or_icon_name(vt_parser, 0);
        } else if (ps[0] == 21) {
          report_window_or_icon_name(vt_parser, 1);
        } else if (ps[0] >= 24) {
          /*
           * "CSI Pn t" DECSLPP
           * This changes not only the number of lines but also
           * the number of pages, but mlterm doesn't change the latter.
           */
          resize(vt_parser, -1, ps[0], 1);
        } else if (num == 2) {
          if (ps[0] == 22) {
            /*
             * XXX
             * If Icon and window title are pushed by ps[1] = 0 and either of them is poped,
             * the other should be poped.
             * But it is not supported for now.
             * esctest: XtermWinopsTests.test_XtermWinops_PushIconAndWindow_PopIcon and
             * XtermWinopsTests.test_XtermWinops_PushIconAndWindow_PopWindow fails by this.
             */
            if (ps[1] == 0 || ps[1] == 1) {
              push_to_saved_names(&vt_parser->saved_icon_names, vt_parser->icon_name);
            }

            if (ps[1] == 0 || ps[1] == 2) {
              push_to_saved_names(&vt_parser->saved_win_names, vt_parser->win_name);
            }
          } else if (ps[0] == 23) {
            if ((ps[1] == 0 || ps[1] == 1) && vt_parser->saved_icon_names.num > 0) {
              set_icon_name(vt_parser, pop_from_saved_names(&vt_parser->saved_icon_names));
            }

            if ((ps[1] == 0 || ps[1] == 2) && vt_parser->saved_win_names.num > 0) {
              set_window_name(vt_parser, pop_from_saved_names(&vt_parser->saved_win_names));
            }
          }
        }
      } else if (*str_p == 'u') {
        /* "CSI u" (SCORC) */

        restore_cursor(vt_parser);
      } else if (*str_p == 'x') {
        /* "CSI x" request terminal parameters (DECREQTPARM) */

        /* XXX the same as rxvt */

        if (ps[0] < 0) {
          ps[0] = 0;
        }

        if (ps[0] == 0 || ps[0] == 1) {
          char seq[] = "\x1b[X;1;1;112;112;1;0x";

          /* '+ 0x30' lets int to char */
          seq[2] = ps[0] + 2 + 0x30;

          vt_write_to_pty(vt_parser->pty, seq, sizeof(seq) - 1);
        }
      }
#if 0
      else if (*str_p == '^') {
        /* "CSI ^" initiate hilite mouse tracking. */
      }
#endif
      /* Other final character */
      else if (0x40 <= *str_p && *str_p <= 0x7e) {
#ifdef DEBUG
        debug_print_unknown("ESC [ %c\n", *str_p);
#endif
      } else {
/* not VT100 control sequence. */

#ifdef ESCSEQ_DEBUG
        bl_msg_printf("=> not VT100 control sequence.\n");
#endif

        return 1;
      }
    } else if (*str_p == ']') {
      /* "ESC ]" (OSC) */
      int ps;
      u_char *pt;

      /*
       * OSC 123;456;test ST
       *     ^^^ ^^^^^^^^-> pt
       *       +-> ps
       */

      /* Don't use inc_str_in_esc_seq() in case "OSC ST" etc */
      if (!increment_str(&str_p, &left)) {
        return 0;
      }

      if (*str_p < '0' || '9' < *str_p) {
        ps = -1;
      } else {
        ps = 0;
        do {
          ps = ps * 10 + ((*str_p) - '0');

          /* Don't use inc_str_in_esc_seq() in case "OSC 123 ST" etc */
          if (!increment_str(&str_p, &left)) {
            return 0;
          }
        } while ('0' <= *str_p && *str_p <= '9');

        if (*str_p == ';' && !increment_str(&str_p, &left)) {
          return 0;
        }
      }
#ifdef MAX_PS_DIGIT
      if (ps > MAX_PS_DIGIT) {
        ps = MAX_PS_DIGIT;
      }
#endif

      pt = str_p;

      /*
       * Skip string which was already parsed.
       * +1 in case str_p[left - vt_parser->r_buf.new_len] points
       * "\\" of "\x1b\\".
       */
      if (left > vt_parser->r_buf.new_len + 1) {
        str_p += (left - vt_parser->r_buf.new_len - 1);
        left = vt_parser->r_buf.new_len + 1;
      }

      if (!get_pt_in_esc_seq(&str_p, &left, 0, 1)) {
        if (left == 0) {
          return 0;
        }
#ifdef DEBUG
        else {
          debug_print_unknown("ESC ] %d ; ???\n", ps);
        }
#endif
      } else if (ps == 0) {
        /* "OSC 0" change icon name and window title */

        if (*pt != '\0') {
          if ((pt = parse_title(vt_parser, strdup(pt)))) {
            set_window_name(vt_parser, pt);
            set_icon_name(vt_parser, strdup(pt));
          }
        }
      } else if (ps == 1) {
        /* "OSC 1" change icon name */

        if (*pt != '\0') {
          if ((pt = parse_title(vt_parser, strdup(pt)))) {
            set_icon_name(vt_parser, pt);
          }
        }
      } else if (ps == 2) {
        /* "OSC 2" change window title */

        if (*pt != '\0') {
          if ((pt = parse_title(vt_parser, strdup(pt)))) {
            set_window_name(vt_parser, pt);
          }
        }
      } else if (ps == 4) {
        /* "OSC 4" change 256 color */

        change_color_rgb(vt_parser, pt);
      } else if (ps == 5) {
        /* "OSC 5" change colorBD/colorUL */

        change_special_color(vt_parser, pt);
      } else if (ps == 10) {
        /* "OSC 10" fg color */

        change_fgbg_color(vt_parser, 10, pt);
      } else if (ps == 11) {
        /* "OSC 11" bg color */

        change_fgbg_color(vt_parser, 11, pt);
      } else if (ps == 12) {
        /* "OSC 12" cursor bg color */

        if (strcmp(pt, "?") == 0) /* ?:query rgb */ {
          /* XXX do nothing for now .*/
        } else {
          config_protocol_set_simple(vt_parser, "cursor_bg_color", pt, 1);
        }
      } else if (ps == 20) {
        /* "OSC 20" (Eterm compatible) */

        /* edit commands */
        char *p;

        /* XXX discard all adjust./op. settings.*/
        /* XXX may break multi-byte char string. */
        if ((p = strchr(pt, ';'))) {
          *p = '\0';
        }
        if ((p = strchr(pt, ':'))) {
          *p = '\0';
        }

        if (*pt == '\0') {
          /*
           * Do not change current edit but
           * alter diaplay setting.
           * XXX nothing can be done for now.
           */

          return 0;
        }

        config_protocol_set_simple(vt_parser, "wall_picture", pt, 1);
      }
#if 0
      else if (ps == 46) {
        /* "OSC 46" change log file */
      } else if (ps == 50) {
        /* "OSC 50" set font */
      }
#endif
      else if (ps == 52) {
        set_selection(vt_parser, pt);
      } else if (ps == 104) {
        reset_color_rgb(vt_parser, pt, 0);
      } else if (ps == 105) {
        reset_color_rgb(vt_parser, pt, 1);
      }
#if 0
      else if (ps == 110) {
        config_protocol_set_simple(vt_parser, "fg_color", "black", 1);
      } else if (ps == 111) {
        config_protocol_set_simple(vt_parser, "bg_color", "white", 1);
      } else if (ps == 112) {
        config_protocol_set_simple(vt_parser, "cursor_bg_color", "black", 1);
      }
#endif
#ifdef SUPPORT_ITERM2_OSC1337
      else if (ps == 1337) {
        if (strcmp(pt, "SetMark") == 0) {
          vt_line_t *line;

          if ((line = vt_screen_get_cursor_line(vt_parser->screen))) {
            line->mark = 1;
          }
        } else {
          iterm2_proprietary_set(vt_parser, pt);
        }
      }
#endif
      else if (ps == 5379) {
        /* "OSC 5379" set */

        config_protocol_set(vt_parser, pt, 0);
      } else if (ps == 5380) {
        /* "OSC 5380" get */

        config_protocol_get(vt_parser, pt, 0, NULL);
      } else if (ps == 5381) {
        /* "OSC 5381" get(menu) */

        config_protocol_get(vt_parser, pt, 1, NULL);
      } else if (ps == 5383) {
        /* "OSC 5383" set&save */

        config_protocol_set(vt_parser, pt, 1);
      }
#ifdef DEBUG
      else if (ps == -1) {
        debug_print_unknown("ESC ] %s\n", pt);
      } else {
        debug_print_unknown("ESC ] %d ; %s\n", ps, pt);
      }
#endif
    } else if (*str_p == 'P') {
      /* "ESC P" DCS */

      u_char *dcs_beg;
#ifndef NO_IMAGE
      char *path;
#endif

      while (1) {
        /* ESC P ... */
        dcs_beg = str_p - 1;
        break;

      parse_dcs:
        /* 0x90 ... */
        dcs_beg = str_p;
        break;
      }

      /* Skip parameters (0x30-0x3f) */
      do {
        /* Don't use inc_str_in_esc_seq() in case "DCS 123 ST" etc */
        if (!increment_str(&str_p, &left)) {
          return 0;
        }
      } while (0x30 <= *str_p && *str_p <= 0x3f);

#ifndef NO_IMAGE
      /*
       * Intermediate chars (0x20-0x2f) can appear here as DCS, but
       * they are invalid as Sixel and Regis format.
       */
      if (/* sixel */
          (*str_p == 'q' &&
           (path = get_home_file_path("", vt_pty_get_slave_name(vt_parser->pty) + 5, "six"))) ||
          /* ReGIS */
          (*str_p == 'p' &&
           (path = get_home_file_path("", vt_pty_get_slave_name(vt_parser->pty) + 5, "rgs")))) {
        if (!save_sixel_or_regis(vt_parser, path, dcs_beg, &str_p, &left)) {
          return 0;
        }

        if (strcmp(path + strlen(path) - 4, ".six") == 0) {
          show_picture(vt_parser, path, 0, 0, 0, 0, 0, 0, 0,
                       (!vt_parser->sixel_scrolling &&
                        check_sixel_anim(vt_parser->screen, str_p, left)) ? 2 : 1);
        } else {
          /* ReGIS */
          int orig_flag;

          orig_flag = vt_parser->sixel_scrolling;
          vt_parser->sixel_scrolling = 0;
          show_picture(vt_parser, path, 0, 0, 0, 0, 0, 0, 0,
                       1 /* is_sixel (vt_parser->sixel_scrolling is applied) */);
          vt_parser->sixel_scrolling = orig_flag;
        }

        free(path);
      } else
#endif /* NO_IMAGE */
      if (*str_p == '{') {
        /* DECDLD */

        u_char *param;
        ef_charset_t cs;
        int num;
        u_char *p;
        int ps[9];
        int idx;
        int is_end;
        u_int col_width;
        u_int line_height;

        if (*dcs_beg == '\x1b') {
          param = dcs_beg + 2;
        } else /* if( *dcs_beg == '\x90') */ {
          param = dcs_beg + 1;
        }

        for (num = 0; num < 9; num++) {
          u_char c;

          p = param;

          while ('0' <= *param && *param <= '9') {
            param++;
          }

          c = *param;
          if (c != ';' && c != '{') {
            break;
          }
          *param = '\0';
          ps[num] = *p ? atoi(p) : 0;
          *(param++) = c; /* restore in case of restarting to parse from the begining. */
        }

        if (num < 6 || 8 < num) {
          if (!get_pt_in_esc_seq(&str_p, &left, 1, 0)) {
            return 0;
          }
        } else {
          char *path;

          if (num < 8) {
            /*
             * vt220: 6 parameters
             * (https://manx-docs.org/mirror/vt100.net/docs/vt220-rm/table4-9.html)
             * vt510: 8 parameters
             * (https://vt100.net/docs/vt510-rm/DECDLD.html)
             */
            ps[7] = 0; /* Pcss (0 = 94-character set (default)) */

            if (num == 6) {
              ps[6] = 0; /* Pcmh (0 or omitted = 16 pixels high (default)) */
            }
          }

          if (!increment_str(&str_p, &left)) {
            return 0;
          }

          if (*str_p == ' ') {
            /* ESC ( SP Ft */
            if (!increment_str(&str_p, &left)) {
              return 0;
            }
          }

          idx = ps[1];

          if (0x30 <= *str_p && *str_p <= 0x7e) {
            /* Ft */
            if (ps[7] == 0) {
              cs = CS94SB_ID(*str_p);
            } else {
              cs = CS96SB_ID(*str_p);
            }

            if (ps[3] <= 4 || ps[3] >= 255) {
              col_width = 15;
            } else {
              col_width = ps[3];
            }

            if (ps[6] == 0 || ps[6] >= 255) {
              line_height = 12;
            } else {
              line_height = ps[6];
            }
          } else {
            cs = UNKNOWN_CS;
            col_width = line_height = 0;
          }

#ifndef NO_IMAGE
          if (ps[5] == 3 && cs != UNKNOWN_CS &&
              (path = get_home_file_path("", vt_pty_get_slave_name(vt_parser->pty) + 5,
                                         "six"))) {
            /* DRCS Sixel */
            u_char *orig_drcs_header;
            size_t drcs_header_len = str_p - dcs_beg + 1;
            u_char *orig_sixel_size = NULL;
            size_t sixel_size_len = 0;
            int tmp;
            int pix_width = 0;
            int pix_height = 0;

            orig_drcs_header = alloca(drcs_header_len);
            memcpy(orig_drcs_header, dcs_beg, drcs_header_len);

            dcs_beg = str_p - 1;

            /* skip sixel header parameters */
            do {
              if (!increment_str(&str_p, &left)) {
                free(path);

                return 0;
              }
            } while (*str_p == ';' || ('0' <= *str_p && *str_p <= '9'));

            if (*str_p != 'q') {
              dcs_beg--;
              dcs_beg[0] = '\x1b';
              dcs_beg[1] = 'P';
              dcs_beg[2] = 'q';
              str_p--; /* str_p points 'q' */
              left++;
            } else {
              dcs_beg[0] = '\x1b';
              dcs_beg[1] = 'P';

              if (left == 0) {
                return 0;
              }
            }

            /*
             * Read width and height of sixel graphics from "Pan;Pad;Ph;Pv.
             * If failed, it is impossible to scale image pieces according to Pcmw and Pcmh.
             */
            if (str_p[1] == '"' && !check_cell_size(vt_parser, col_width, line_height)) {
              if (left < 2) { /* XXX */
                return 0;
              }

              if (sscanf(str_p + 2, "%d;%d;%d;%d", &tmp, &tmp, &pix_width, &pix_height) == 4 &&
                  pix_width > 0 && pix_height > 0) {
                sixel_size_len = 1; /* q */
                while (left > ++sixel_size_len &&
                       '0' <= str_p[sixel_size_len] && str_p[sixel_size_len] <= ';');
                orig_sixel_size = alloca(sixel_size_len);
                memcpy(orig_sixel_size, str_p, sixel_size_len);

                if (str_p[sixel_size_len] == 'q') {
                  /*
                   * Starting DRCS Sixel:   q"Pan;Pad;Ph;Pv#...
                   * Continuing DRCS Sixel: q"Pan;Pad;Ph;Pv\0q...
                   */
                  str_p += sixel_size_len;
                  left -= sixel_size_len;
                }
              }
            }

            if (!save_sixel_or_regis(vt_parser, path, dcs_beg, &str_p, &left)) {
              /*
               * q"Pan;Pad;Ph;Pvq\0...
               *                ^^^^^^
               */
              memmove(vt_parser->r_buf.chars + drcs_header_len + sixel_size_len,
                      vt_parser->r_buf.chars + 2 /* ESC P */,
                      vt_parser->r_buf.filled_len - 2);
              /*
               * q"Pan;Pad;Ph;Pvq\0...
               * ^^^^^^^^^^^^^^^
               */
              memcpy(vt_parser->r_buf.chars + drcs_header_len, orig_sixel_size, sixel_size_len);
              memcpy(vt_parser->r_buf.chars, orig_drcs_header, drcs_header_len);
              vt_parser->r_buf.filled_len += (drcs_header_len - 2 + sixel_size_len);
              vt_parser->r_buf.left += (drcs_header_len - 2 + sixel_size_len);

              return 0;
            }

            define_drcs_picture(vt_parser, path, cs, idx, pix_width, pix_height,
                                col_width, line_height);

            free(path);
          } else
#endif
          {
            u_char *pt = str_p;
            vt_drcs_font_t *font;

            if (!get_pt_in_esc_seq(&str_p, &left, 1, 0)) {
              return 0;
            }

            if (cs == UNKNOWN_CS) {
              if (ps[2] == 2) {
                destroy_drcs(vt_parser->drcs);
              }
            } else {
              if (ps[2] == 0) {
                vt_drcs_final(vt_parser->drcs, cs);
              } else if (ps[2] == 2) {
                vt_drcs_final_full(vt_parser->drcs);
              }

              if (!vt_parser->drcs) {
                vt_parser->drcs = vt_drcs_new();
              }

              font = vt_drcs_get_font(vt_parser->drcs, cs, 1);

              while (1) {
                p = ++pt;

                while (*pt == '/' || ('?' <= *pt && *pt <= '~')) {
                  pt++;
                }

                if (*pt) {
                  *pt = '\0';
                  is_end = 0;
                } else {
                  is_end = 1;
                }

                if (*p) {
                  if (strlen(p) == (col_width + 1) * ((line_height + 5) / 6) - 1) {
                    vt_drcs_add_glyph(font, idx, p, col_width, line_height);
                  }
#ifdef DEBUG
                  else {
                    bl_debug_printf(BL_DEBUG_TAG "DRCS illegal size %s\n", p);
                  }
#endif

                  idx++;
                }

                if (is_end) {
                  break;
                }
              }
            }
          }
        }
      } else {
        u_char *macro;
        u_char *tckey;
        u_char *status;
        u_char *present;

        macro = tckey = status = present = NULL;

        if ((*str_p == '!' && *(str_p + 1) == 'z') ||
            ((*str_p == '+' || *str_p == '$') && *(str_p + 1) == 'q') ||
            (*str_p == '$' && *(str_p + 1) == 't')) {
          if (left <= 2) {
            left = 0;

            return 0;
          }

          str_p += 2;
          left -= 2;

          if (*(str_p - 1) == 'z' /* && *(str_p - 2) == '!' */) {
            /* DECDMAC */
            macro = str_p;
          } else if (*(str_p - 1) == 'q') {
            if (*(str_p - 2) == '$') {
              /* DECRQSS */
              status = str_p;
            } else {
              /* Termcap query */
              tckey = str_p;
            }
          } else /* if (*(str_p - 1) == t && *(str_p - 2) == '$') */ {
            /* DECRSPS */
            present = str_p;
          }
        } else {
          if (!increment_str(&str_p, &left)) {
            return 0;
          }
        }

        /*
         * +1 in case str_p[left - vt_parser->r_buf.new_len] points
         * "\\" of "\x1b\\".
         */
        if (left > vt_parser->r_buf.new_len + 1) {
          str_p += (left - vt_parser->r_buf.new_len - 1);
          left = vt_parser->r_buf.new_len + 1;
        }

        if (get_pt_in_esc_seq(&str_p, &left, 1, 0)) {
          if (macro) {
            define_macro(vt_parser, dcs_beg + (*dcs_beg == '\x1b' ? 2 : 1), macro);
          } else if (status) {
            report_status(vt_parser, status);
          } else if (tckey) {
            report_termcap(vt_parser, tckey);
          } else if (present) {
            set_presentation_state(vt_parser, present);
          }
        } else if (left == 0) {
          return 0;
        }
      }
    } else if (*str_p == 'X' || *str_p == '^' || *str_p == '_') {
      /*
       * "ESC X" SOS
       * "ESC ^" PM
       * "ESC _" APC
       */
      /* Don't use inc_str_in_esc_seq() in case "SOS ST" etc */
      if (!increment_str(&str_p, &left)) {
        return 0;
      }

      /* +1 in case str_p[left - new_len] points "\\" of "\x1b\\". */
      if (left > vt_parser->r_buf.new_len + 1) {
        str_p += (left - vt_parser->r_buf.new_len - 1);
        left = vt_parser->r_buf.new_len + 1;
      }

      if (!get_pt_in_esc_seq(&str_p, &left, 1, 0) && left == 0) {
        return 0;
      }
    }
    /* Other final character */
    else if (0x30 <= *str_p && *str_p <= 0x7e) {
#ifdef DEBUG
      debug_print_unknown("ESC %c\n", *str_p);
#endif
    }
    /* intermediate character */
    else if (0x20 <= *str_p && *str_p <= 0x2f) {
      /*
       * ESC I.....I  F
       * 033 040-057  060-176
       */
      u_int ic_num;

      ic_num = 0;

      /* In case more than one intermediate(0x20-0x2f) chars. */
      do {
        ic_num++;

        if (!inc_str_in_esc_seq(vt_parser->screen, &str_p, &left)) {
          return 0;
        }
      } while (0x20 <= *str_p && *str_p <= 0x2f);

      if (ic_num == 1 || ic_num == 2) {
        if (ic_num == 1 && *(str_p - 1) == '#') {
          if ('3' <= *str_p && *str_p <= '6') {
            vt_line_t *line;

            line = vt_screen_get_cursor_line(vt_parser->screen);
            if (*str_p == '3') {
              /*
               * "ESC # 3" DEC double-height line,
               * top half (DECDHL)
               */
              vt_line_set_size_attr(line, DOUBLE_HEIGHT_TOP);
            } else if (*str_p == '4') {
              /*
               * "ESC # 4" DEC double-height line,
               * bottom half (DECDHL)
               */
              vt_line_set_size_attr(line, DOUBLE_HEIGHT_BOTTOM);
            } else if (*str_p == '5') {
              /*
               * "ESC # 5" DEC single-with line (DECSWL)
               */
              vt_line_set_size_attr(line, 0);
            } else /* if( *str_p == '6') */
            {
              /*
               * "ESC # 6" DEC double-with line (DECDWL)
               */
              vt_line_set_size_attr(line, DOUBLE_WIDTH);
            }
          } else if (*str_p == '8') {
            /* "ESC # 8" DEC screen alignment test (DECALN) */

            vt_screen_set_vmargin(vt_parser->screen, -1, -1);
            vt_screen_set_use_hmargin(vt_parser->screen, 0);
            vt_screen_goto(vt_parser->screen, 0, 0);

            vt_screen_fill_area(vt_parser->screen, 'E', vt_parser->is_protected, 0, 0,
                                vt_screen_get_logical_cols(vt_parser->screen),
                                vt_screen_get_logical_rows(vt_parser->screen));
          }
        } else if (*(str_p - ic_num) == '(' || *(str_p - ic_num) == '$') {
          /*
           * "ESC ("(Registered CS),
           * "ESC ( SP"(DRCS) or "ESC $"
           * See vt_convert_to_internal_ch() about CS94MB_ID.
           */

          if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
            /* ESC ( will be processed in mef. */
            return 1;
          }

          vt_parser->g0 = (*(str_p - ic_num) == '$') ? CS94MB_ID(*str_p) : CS94SB_ID(*str_p);

          if (!vt_parser->is_so) {
            vt_parser->gl = vt_parser->g0;
          }
        } else if (*(str_p - ic_num) == ')') {
          /* "ESC )"(Registered CS) or "ESC ( SP"(DRCS) */

          if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
            /* ESC ) will be processed in mef. */
            return 1;
          }

          vt_parser->g1 = (*(str_p - ic_num) == '$') ? CS94MB_ID(*str_p) : CS94SB_ID(*str_p);

          if (vt_parser->is_so) {
            vt_parser->gl = vt_parser->g1;
          }
        } else if (*(str_p - ic_num) == '-') {
          /* "ESC -"(Registered CS) or "ESC - SP"(DRCS) */

          if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
            /* ESC ) will be processed in mef. */
            return 1;
          }

          vt_parser->g1 = CS96SB_ID(*str_p);

          if (vt_parser->is_so) {
            vt_parser->gl = vt_parser->g1;
          }
        } else {
          /*
           * "ESC SP F", "ESC SP G", "ESC SP L", "ESC SP M",
           * "ESC SP N" etc ...
           */
        }
      }
    } else {
/* not VT100 control sequence. */

#ifdef ESCSEQ_DEBUG
      bl_msg_printf("=> not VT100 control sequence.\n");
#endif

      return 1;
    }

#ifdef ESCSEQ_DEBUG
    bl_msg_printf("\n");
#endif
  } else if (*str_p == CTL_SI) {
    if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
      /* SI will be processed in mef. */
      return 1;
    }

#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving SI\n");
#endif

    vt_parser->gl = vt_parser->g0;
    vt_parser->is_so = 0;
  } else if (*str_p == CTL_SO) {
    if (IS_ENCODING_BASED_ON_ISO2022(vt_parser->encoding)) {
      /* SO will be processed in mef. */
      return 1;
    }

#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving SO\n");
#endif

    vt_parser->gl = vt_parser->g1;
    vt_parser->is_so = 1;
  } else if (CTL_LF <= *str_p && *str_p <= CTL_FF) {
#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving LF\n");
#endif

    vt_screen_line_feed(vt_parser->screen);
    if (AUTO_CR(vt_parser)) {
      vt_screen_goto_beg_of_line(vt_parser->screen);
    }
  } else if (*str_p == CTL_CR) {
#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving CR\n");
#endif

    vt_screen_goto_beg_of_line(vt_parser->screen);
  } else if (*str_p == CTL_TAB) {
#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving TAB\n");
#endif

    vt_screen_forward_tabs(vt_parser->screen, 1);
  } else if (*str_p == CTL_BS) {
#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving BS\n");
#endif

    vt_screen_go_back(vt_parser->screen, 1, 0);
  } else if (*str_p == CTL_BEL) {
#ifdef ESCSEQ_DEBUG
    bl_debug_printf(BL_DEBUG_TAG " receiving BEL\n");
#endif

    if (HAS_XTERM_LISTENER(vt_parser, bel)) {
      stop_vt100_cmd(vt_parser, 0);
      (*vt_parser->xterm_listener->bel)(vt_parser->xterm_listener->self);
      /*
       * XXX
       * start_vt100_cmd( ... , *1*) erases cursor which
       * xterm_listener::bell drew if bell mode is visual.
       */
      start_vt100_cmd(vt_parser, 1);
    }
  } else if (*str_p == 0x90) {
    goto parse_dcs;
  } else if (*str_p == 0x18) {
    if (vt_parser->r_buf.left < 3) {
      return 0;
    }

    if (memcmp(str_p + 1, "B01", 3) == 0) {
      vt_parser->is_zmodem_ready = 1; /* rz */
    }
#if 0
    else if (memcmp(str_p + 1, "B00", 3) == 0) {
      vt_parser->is_zmodem_ready = 2; /* sz */
    }
#endif
  } else {
    /* not VT100 control sequence */

    return 1;
  }

#ifdef EDIT_DEBUG
  vt_edit_dump(vt_parser->screen->edit);
#endif

  vt_parser->r_buf.left = left - 1;

  return 1;
}

static int parse_vt100_sequence(vt_parser_t *vt_parser) {
  ef_char_t ch;
  size_t prev_left;

  while (1) {
    prev_left = vt_parser->r_buf.left;

    /*
     * parsing character encoding.
     */
    (*vt_parser->cc_parser->set_str)(vt_parser->cc_parser, CURRENT_STR_P(vt_parser),
                                        vt_parser->r_buf.left);
    while ((*vt_parser->cc_parser->next_char)(vt_parser->cc_parser, &ch)) {
      int ret;
      ef_charset_t orig_cs;

      orig_cs = ch.cs;
      ret = vt_convert_to_internal_ch(vt_parser, &ch);

      if (ret == 1) {
        if (vt_parser->gl != US_ASCII && orig_cs == US_ASCII) {
          orig_cs = vt_parser->gl;
        }

        if (vt_parser->cs != orig_cs) {
          vt_parser->cs = orig_cs;
        }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
        if (IS_ISCII(ch.cs) && ch.size == 2) {
          ch.size = 1;
          put_char(vt_parser, ef_char_to_int(&ch), ch.cs, ch.property);
          ch.ch[0] = ch.ch[1];
          /* nukta is always combined. */
          ch.property |= EF_COMBINING;
        }
#endif

        put_char(vt_parser, ef_char_to_int(&ch), ch.cs, ch.property);

        vt_parser->r_buf.left = vt_parser->cc_parser->left;
      } else if (ret == -1) {
        /*
         * This is a control sequence (C0 or C1), so
         * reparsing this char in vt100_escape_sequence() ...
         */

        vt_parser->cc_parser->left++;
        vt_parser->cc_parser->is_eos = 0;

        break;
      }
    }

    vt_parser->r_buf.left = vt_parser->cc_parser->left;

    flush_buffer(vt_parser);

    if (vt_parser->cc_parser->is_eos) {
      /* expect more input */
      break;
    }

    /*
     * parsing other vt100 sequences.
     * (vt_parser->w_buf is always flushed here.)
     */

    if (!parse_vt100_escape_sequence(vt_parser)) {
      /* expect more input */
      break;
    }

#ifdef EDIT_ROUGH_DEBUG
    vt_edit_dump(vt_parser->screen->edit);
#endif

    if (vt_parser->r_buf.left == prev_left) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " unrecognized sequence[%.2x] is received , ignored...\n",
                      *CURRENT_STR_P(vt_parser));
#endif

      vt_parser->r_buf.left--;
    }

    if (vt_parser->r_buf.left == 0) {
      break;
    }
  }

  if (vt_parser->yield) {
    vt_parser->yield = 0;

    return 0;
  }

  return 1;
}

static int write_loopback(vt_parser_t *vt_parser, const u_char *buf, size_t len,
                          int enable_local_echo,
                          int is_visual /* 1: stop_vt100_cmd(1), -1: stop_vt100_cmd(0) */
                          ) {
  char *orig_buf;
  size_t orig_left;

  if (!vt_parser->pty || /* vt_term_write_loopback() in open_pty_intern() in ui_screen_manager.c can
                          * be called when vt_parser->pty is NULL */
      vt_pty_get_master_fd(vt_parser->pty) != -1) {
    if (vt_parser->r_buf.len < len &&
        /*
         * BL_MAX(len, PTY_RD_BUFFER_SIZE) is necessary because
         * open_pty_intern() in ui_screen_manager.c calls vt_term_write_loopback()
         * when vt_parser->r_buf.len is still 0.
         */
        !change_read_buffer_size(&vt_parser->r_buf, BL_MAX(len, PTY_RD_BUFFER_SIZE))) {
      return 0;
    }

    if ((orig_left = vt_parser->r_buf.left) > 0) {
      if (!(orig_buf = alloca(orig_left))) {
        return 0;
      }

      memcpy(orig_buf, CURRENT_STR_P(vt_parser), orig_left);
    }

    memcpy(vt_parser->r_buf.chars, buf, len);
    vt_parser->r_buf.filled_len = vt_parser->r_buf.left = vt_parser->r_buf.new_len = len;
  } else {
    /* for vterm compatible library */

    if (vt_parser->r_buf.len < len + vt_parser->r_buf.left &&
        !change_read_buffer_size(&vt_parser->r_buf, len + vt_parser->r_buf.left)) {
      return 0;
    }

    memmove(vt_parser->r_buf.chars,
            vt_parser->r_buf.chars + vt_parser->r_buf.filled_len - vt_parser->r_buf.left,
            vt_parser->r_buf.left);
    memcpy(vt_parser->r_buf.chars + vt_parser->r_buf.left, buf, len);
    vt_parser->r_buf.filled_len = (vt_parser->r_buf.left += len);
    vt_parser->r_buf.new_len = len;
    orig_left = 0;
  }

  if (is_visual) {
    start_vt100_cmd(vt_parser, 1);
  }

  if (enable_local_echo) {
    vt_screen_enable_local_echo(vt_parser->screen);
  }

  /*
   * bidi and visual-indian is always stopped from here.
   * If you want to call {start|stop}_vt100_cmd (where vt_xterm_event_listener
   * is called),
   * the second argument of it shoule be 0.
   */
  parse_vt100_sequence(vt_parser);

  if (is_visual) {
    stop_vt100_cmd(vt_parser, is_visual > 0);
  }

  if (orig_left > 0) {
    memcpy(vt_parser->r_buf.chars, orig_buf, orig_left);
    vt_parser->r_buf.filled_len = vt_parser->r_buf.left = orig_left;
  } else {
    /* vt_parser->r_buf.left is > 0 or vterm compatible library. */
  }

  return 1;
}

static void local_echo(vt_parser_t *vt_parser, const u_char *buf, size_t len) {
  size_t count;

  if (len == 1) {
    if (vt_parser->prev_local_echo_char == buf[0]) {
      vt_screen_local_echo_wait(vt_parser->screen, 0);
      vt_parse_vt100_sequence(vt_parser);

      return;
    } else {
      vt_parser->prev_local_echo_char = buf[0];
    }
  } else {
    vt_parser->prev_local_echo_char = 0;
  }

  for (count = 0; count < len; count++) {
    if (buf[count] < 0x20) {
      vt_screen_local_echo_wait(vt_parser->screen, 0);
      vt_parse_vt100_sequence(vt_parser);

      return;
    }
  }

  vt_parse_vt100_sequence(vt_parser);

  if (!(vt_parser->line_style & LS_UNDERLINE)) {
    char *new_buf;
    size_t new_len;

    if ((new_buf = alloca((new_len = 4 + len + 5)))) {
      memcpy(new_buf, "\x1b[4m", 4);
      memcpy(new_buf + 4, buf, len);
      memcpy(new_buf + 4 + len, "\x1b[24m", 5);
      buf = new_buf;
      len = new_len;
    }
  }

  write_loopback(vt_parser, buf, len, 1, 1);
}

static int is_transferring_data(vt_parser_t *vt_parser) {
  int progress_cur;
  int progress_len;
  int state;
  vt_char_t *bar;

  if ((state = vt_transfer_get_state(&progress_cur, &progress_len)) >= 0 &&
      (bar = alloca(sizeof(vt_char_t) * (progress_len + 2)))) {
    int count;

    start_vt100_cmd(vt_parser, 1);

    if (progress_cur == 0) {
      char *msg;
      char *p1;
      char *p2;
      size_t len;
      size_t pre_len;

      if (vt_parser->is_transferring_data == 0x1) {
        pre_len = 5;
        p1 = "Send ";
        p2 = send_file ? send_file : "";
      } else {
        pre_len = 8;
        p1 = "Save in ";
        p2 = recv_dir ? recv_dir : "~/.mlterm/recv";
      }

      len = 10 + pre_len + strlen(p2);
      if ((msg = alloca(len + 1))) {
        memcpy(msg, "\r\nZMODEM: ", 10);
        memcpy(msg + 10, p1, pre_len);
        strcpy(msg + 10 + pre_len, p2);
      }
      write_loopback(vt_parser, msg, len, 0, 0);
      vt_screen_line_feed(vt_parser->screen);
    }

    vt_screen_goto_beg_of_line(vt_parser->screen);
    vt_screen_clear_line_to_right(vt_parser->screen);

    vt_str_init(bar, progress_len + 2);
    vt_char_set(bar, '|', US_ASCII, 0, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);

    for (count = 0; count < progress_cur; count++) {
      vt_char_set(bar + 1 + count, '*', US_ASCII, 0, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);
    }
    for (; count < progress_len; count++) {
      vt_char_set(bar + 1 + count, ' ', US_ASCII, 0, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);
    }
    vt_char_set(bar + 1 + progress_len, '|', US_ASCII, 0, 0, 0, VT_FG_COLOR, VT_BG_COLOR,
                0, 0, 0, 0, 0);

    vt_screen_overwrite_chars(vt_parser->screen, bar, progress_len + 2);

    if (state == 0) {
      vt_screen_line_feed(vt_parser->screen);
      vt_screen_goto_beg_of_line(vt_parser->screen);

      stop_vt100_cmd(vt_parser, 1);

      vt_parser->is_transferring_data = 0;

      return 0;
    }

    stop_vt100_cmd(vt_parser, 1);
  }

  return 1;
}

/* sending or receiving data via zmodem */
static void transfer_data(vt_parser_t *vt_parser) {
  receive_bytes(vt_parser);

  do {
    u_char input[4096];
    u_char output[(2 * (1024 + 4 + 1)) + 1];
    u_int len = 0;
    size_t copy_len;

    if ((copy_len = vt_parser->r_buf.left) > sizeof(input) - 1) {
      copy_len = sizeof(input) - 1;
    }

    memcpy(input, CURRENT_STR_P(vt_parser), copy_len);
    input[copy_len] = '\0';

    if ((vt_parser->r_buf.left -= copy_len) > 0) {
      memmove(vt_parser->r_buf.chars, CURRENT_STR_P(vt_parser), vt_parser->r_buf.left);
      vt_parser->r_buf.filled_len = vt_parser->r_buf.left;
    }

    vt_transfer_data(input, copy_len, output, &len, sizeof(output));

    if (len > 0) {
      vt_write_to_pty(vt_parser->pty, output, len);
    }
  } while (is_transferring_data(vt_parser) && receive_bytes(vt_parser) > 0);
}

/* This function assumes vt_pty_is_loopback(vt_parser->pty) is false if do_zcan is true. */
static void transfer_cancel(vt_parser_t *vt_parser, int do_zcan) {
  if (vt_parser->is_transferring_data) {
    vt_parser->is_transferring_data = 0;
    vt_parser->r_buf.left = 0;
    vt_transfer_cancel();
  }
#if 0
  else if (vt_parser->is_zmodem_ready == 0) {
    return;
  }
#endif

  vt_parser->is_zmodem_ready = 0;

  if (do_zcan) {
    vt_write_to_pty(vt_parser->pty,
                        "**\x18\x18\x18\x18\x18\x18\x18\x18"
                        "\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08", 20);
  }
}

static char *convert_to_locale_encoding(const char *str, ef_parser_t *parser,
                                        vt_char_encoding_t encoding) {
  vt_char_encoding_t locale_encoding;
  char *new_str;
  size_t len = strlen(str);

  if ((locale_encoding = vt_get_char_encoding(bl_get_codeset())) == VT_UNKNOWN_ENCODING) {
    locale_encoding = encoding;
  }

  if (locale_encoding == encoding || (new_str = alloca(len * 2 + 1)) == NULL) {
    return strdup(str);
  }

  if (parser) {
    (*parser->init)(parser);
    (*parser->set_str)(parser, str, len);
    new_str[vt_char_encoding_convert_with_parser(new_str, len * 2, locale_encoding,
                                                 parser)] = '\0';
  } else {
    new_str[vt_char_encoding_convert(new_str, len * 2, locale_encoding,
                                     str, len - 5, encoding)] = '\0';
  }

  return strdup(new_str);
}

/* --- global functions --- */

void vt_set_use_alt_buffer(int use) { use_alt_buffer = use; }

void vt_set_unicode_noconv_areas(char *areas) {
  unicode_noconv_areas =
    set_area_to_table(unicode_noconv_areas, &num_unicode_noconv_areas, areas);
}

void vt_set_full_width_areas(char *areas) {
  full_width_areas = set_area_to_table(full_width_areas, &num_full_width_areas, areas);
}

void vt_set_half_width_areas(char *areas) {
  half_width_areas = set_area_to_table(half_width_areas, &num_half_width_areas, areas);
}

void vt_set_use_ttyrec_format(int use) { use_ttyrec_format = use; }

#ifdef USE_LIBSSH2
void vt_set_use_scp_full(int use) {
  if (use >= 0) {
    use_scp_full = use;
  } else if (use == -1) {
    if (use_scp_full == 0) {
      use_scp_full = -1;
    } else if (use_scp_full == -1) {
      use_scp_full = 0;
    }
  }
}
#endif

void vt_set_recv_dir(const char *dir) {
  if (strstr(dir, "..")) {
    /* insecure dir name */
    bl_msg_printf("%s is insecure dir name.\n", dir);
  } else {
    free(recv_dir);
    recv_dir = strdup(dir);
  }
}

void vt_set_timeout_read_pty(u_long timeout) { timeout_read_pty = timeout; }

void vt_set_primary_da(char *da) {
  free(primary_da);
  primary_da = strdup(da);
}

void vt_set_secondary_da(char *da) {
  free(secondary_da);
  secondary_da = strdup(da);
}

void vt_set_local_echo_wait(u_int msec) {
  local_echo_wait_msec = msec;
}

void vt_set_format_other_keys(int flag) {
  format_other_keys = flag;
}

void vt_parser_final(void) {
  vt_config_proto_final();

  free(unicode_noconv_areas);
  num_unicode_noconv_areas = 0;

  free(full_width_areas);
  num_full_width_areas = 0;

  free(half_width_areas);
  num_half_width_areas = 0;

  vt_set_auto_detect_encodings("");
}

vt_parser_t *vt_parser_new(vt_screen_t *screen, vt_termcap_ptr_t termcap, vt_char_encoding_t encoding,
                           int is_auto_encoding, int use_auto_detect, int logging_vt_seq,
                           vt_unicode_policy_t policy, u_int col_size_a, int use_char_combining,
                           int use_multi_col_char, const char *win_name, const char *icon_name,
                           int use_ansi_colors, vt_alt_color_mode_t alt_color_mode,
                           vt_cursor_style_t cursor_style, int ignore_broadcasted_chars,
                           int use_local_echo, int use_locked_title) {
  vt_parser_t *vt_parser;

  if ((vt_parser = calloc(1, sizeof(vt_parser_t))) == NULL) {
    return NULL;
  }

  vt_str_init(vt_parser->w_buf.chars, PTY_WR_BUFFER_SIZE);
  vt_parser->w_buf.output_func = vt_screen_overwrite_chars;

  vt_parser->screen = screen;
  vt_parser->termcap = termcap;

  vt_parser->log_file = -1;

  vt_parser->cs = UNKNOWN_CS;
  vt_parser->fg_color = VT_FG_COLOR;
  vt_parser->bg_color = VT_BG_COLOR;
  vt_parser->use_char_combining = use_char_combining;
  vt_parser->use_multi_col_char = use_multi_col_char;
  vt_parser->is_auto_encoding = is_auto_encoding;
  vt_parser->use_auto_detect = use_auto_detect;
  vt_parser->logging_vt_seq = logging_vt_seq;
  vt_parser->unicode_policy = policy;
  vt_parser->cursor_style = cursor_style;
  vt_parser->is_visible_cursor = 1;
  vt_parser->hide_pointer_mode = 1; /* Compatible with xterm 370 */

  if ((vt_parser->cc_conv = vt_char_encoding_conv_new(encoding)) == NULL) {
    goto error;
  }

  if ((vt_parser->cc_parser = vt_char_encoding_parser_new(encoding)) == NULL) {
    goto error;
  }

  vt_parser->encoding = encoding;

  if (win_name) {
    vt_parser->win_name = strdup(win_name);
  }

  if (icon_name) {
    vt_parser->icon_name = strdup(icon_name);
  }

  vt_parser->gl = US_ASCII;
  vt_parser->g0 = US_ASCII;
  vt_parser->g1 = US_ASCII;

  set_col_size_of_width_a(vt_parser, col_size_a);

  /* Default value of modify_*_keys except modify_other_keys is 2. */
  vt_parser->modify_cursor_keys = 2;
  vt_parser->modify_function_keys = 2;

  vt_parser->sixel_scrolling = 1;
  vt_parser->use_ansi_colors = use_ansi_colors;
  vt_parser->alt_color_mode = alt_color_mode;
  vt_parser->saved_vtmode_flags[0] = vt_parser->vtmode_flags[0] = INITIAL_VTMODE_FLAGS_0;
  vt_parser->saved_vtmode_flags[1] = vt_parser->vtmode_flags[1] = INITIAL_VTMODE_FLAGS_1;
  vt_parser->ignore_broadcasted_chars = ignore_broadcasted_chars;
  vt_parser->use_local_echo = use_local_echo;
  vt_parser->use_locked_title = use_locked_title;

  return vt_parser;

error:
  if (vt_parser->cc_conv) {
    (*vt_parser->cc_conv->destroy)(vt_parser->cc_conv);
  }

  if (vt_parser->cc_parser) {
    (*vt_parser->cc_parser->destroy)(vt_parser->cc_parser);
  }

  free(vt_parser);

  return NULL;
}

int vt_parser_destroy(vt_parser_t *vt_parser) {
  vt_str_final(vt_parser->w_buf.chars, PTY_WR_BUFFER_SIZE);
  (*vt_parser->cc_parser->destroy)(vt_parser->cc_parser);
  (*vt_parser->cc_conv->destroy)(vt_parser->cc_conv);
  destroy_drcs(vt_parser->drcs);
  destroy_all_macros(vt_parser);
  free(vt_parser->sixel_palette);

  if (vt_parser->log_file != -1) {
    close(vt_parser->log_file);
  }

  free(vt_parser->r_buf.chars);

  free(vt_parser->win_name);
  free(vt_parser->icon_name);
  free(vt_parser->saved_win_names.names);
  free(vt_parser->saved_icon_names.names);

  free(vt_parser);

  return 1;
}

void vt_parser_set_pty(vt_parser_t *vt_parser, vt_pty_t *pty) {
#ifdef USE_LIBSSH2
  /* See set_col_size_of_width_a() */
  if (!vt_parser->pty && vt_pty_get_mode(pty) == PTY_MOSH) {
#ifdef USE_WIN32API
    char *env = getenv("MOSH_AWIDTH");

    if (!env) {
      putenv(vt_parser->col_size_of_width_a == 2 ? "MOSH_AWIDTH=2" : "MOSH_AWIDTH=1");
    } else {
      vt_parser->col_size_of_width_a = (*env == '2' ? 2 : 1);
    }
#else
    vt_parser->col_size_of_width_a = (wcwidth(0x25a0) == 2) ? 2 : 1;
#endif
  }
#endif

  vt_parser->pty = pty;
}

void vt_parser_set_xterm_listener(vt_parser_t *vt_parser,
                                  vt_xterm_event_listener_t *xterm_listener) {
  vt_parser->xterm_listener = xterm_listener;
}

void vt_parser_set_config_listener(vt_parser_t *vt_parser,
                                   vt_config_event_listener_t *config_listener) {
  vt_parser->config_listener = config_listener;
}

int vt_parse_vt100_sequence(vt_parser_t *vt_parser) {
  clock_t beg;

  if (vt_parser->is_transferring_data) {
    /* vt_parser->pty is always non-NULL value. */
    transfer_data(vt_parser);

    return 1;
  }

  if (vt_screen_local_echo_wait(vt_parser->screen, local_echo_wait_msec)) {
    return 1;
  }

  if (!vt_parser->pty || receive_bytes(vt_parser) == 0) {
    return 0;
  }

  beg = clock();

  start_vt100_cmd(vt_parser, 1);

  vt_screen_disable_local_echo(vt_parser->screen);

  /*
   * bidi and visual-indian is always stopped from here.
   * If you want to call {start|stop}_vt100_cmd (where vt_xterm_event_listener
   * is called),
   * the second argument of it shoule be 0.
   */

  while (parse_vt100_sequence(vt_parser) &&
         !vt_parser->is_transferring_data &&
         /* (PTY_RD_BUFFER_SIZE / 2) is baseless. */
         vt_parser->r_buf.filled_len >= (PTY_RD_BUFFER_SIZE / 2) &&
         clock() - beg < timeout_read_pty && receive_bytes(vt_parser))
    ;

  stop_vt100_cmd(vt_parser, 1);

  return 1;
}

size_t vt_parser_write(vt_parser_t *vt_parser, const u_char *buf, size_t len) {
  if (vt_parser->is_transferring_data) {
    return 0;
  }

  if (vt_parser->use_local_echo) {
    local_echo(vt_parser, buf, len);
  }

  if (!SEND_RECV_MODE(vt_parser)) {
    write_loopback(vt_parser, buf, len, 0, 1);
  }

  return vt_write_to_pty(vt_parser->pty, buf, len);
}

int vt_parser_write_modified_key(vt_parser_t *vt_parser,
                                 int key, /* 0 < key < 0x80 */
                                 int ch, int modcode) {
  if (vt_parser->is_transferring_data) {
    return 0;
  }

  if (vt_parser->modify_other_keys == 1) {
    /* Assume cases like key = 0x33, modcode = 5, ch = 0x1b. */
    if (ch < 0x20 || ch == 0x7f) {
      if (ch == CTL_BS || (ch != CTL_CR && modcode == 5 /* Control */)) {
        return 0;
      }
    } else {
      if (modcode == 2 /* Shift */ || modcode == 3 /* Alt */ || modcode == 4 /* Shift+Alt */) {
        return 0;
      }
    }
  } else if (vt_parser->modify_other_keys != 2) {
    return 0;
  }

  if (!(modcode == 2 /* Shift */ &&
        (('!' <= key && key < 'A') || ('Z' < key && key < 'a') || ('z' < key && key <= '~')))) {
    size_t len;
    char buf[12];

    if (format_other_keys) {
      /* formatOtherKeys = 1 */
      sprintf(buf, "\x1b[%d;%du" /* 9 */, key, modcode);
    } else {
      /* formatOtherKeys = 0 */
      sprintf(buf, "\x1b[27;%d;%d~" /* 12 */, modcode, key);
    }

    len = strlen(buf);

    if (!SEND_RECV_MODE(vt_parser)) {
      write_loopback(vt_parser, buf, len, 0, 1);
    }

    vt_write_to_pty(vt_parser->pty, buf, len);

    return 1;
  }

  return 0;
}

int vt_parser_write_special_key(vt_parser_t *vt_parser, vt_special_key_t key,
                                int modcode, int is_numlock) {
  char *buf;

  if (vt_parser->is_transferring_data) {
    return 0;
  }

  if ((buf = vt_termcap_special_key_to_seq(vt_parser->termcap, key, modcode,
                                           (vt_parser->is_app_keypad && !is_numlock),
                                           IS_APP_CURSOR_KEYS(vt_parser), IS_APP_ESCAPE(vt_parser),
                                           vt_parser->modify_cursor_keys, vt_parser->modify_function_keys))) {
    size_t len = strlen(buf);

    if (!SEND_RECV_MODE(vt_parser)) {
      write_loopback(vt_parser, buf, len, 0, 1);
    }

    vt_write_to_pty(vt_parser->pty, buf, len);

    return 1;
  } else {
    return 0;
  }
}

int vt_parser_write_loopback(vt_parser_t *vt_parser, const u_char *buf, size_t len) {
  return write_loopback(vt_parser, buf, len, 0, 1);
}

int vt_parser_show_message(vt_parser_t *vt_parser, char *msg) {
  char *buf;
  size_t len;

  if (!(buf = alloca((len = 3 + strlen(msg) + 4)))) {
    return 0;
  }

  if (vt_screen_is_local_echo_mode(vt_parser->screen)) {
    sprintf(buf, "\r\n%s\x1b[K", msg);

    return write_loopback(vt_parser, buf, len - 2, 0, -1);
  } else {
    sprintf(buf, "\x1b[H%s\x1b[K", msg);

    return write_loopback(vt_parser, buf, len - 1, 1, -1);
  }
}

#if 1 /* defined(__ANDROID__) || defined(__APPLE__) || defined(USE_SDL2) */
int vt_parser_preedit(vt_parser_t *vt_parser, const u_char *buf, size_t len) {
  if (!(vt_parser->line_style & LS_UNDERLINE)) {
    char *new_buf;
    size_t new_len;

    if ((new_buf = alloca((new_len = 4 + len + 5)))) {
      memcpy(new_buf, "\x1b[4m", 4);
      memcpy(new_buf + 4, buf, len);
      memcpy(new_buf + 4 + len, "\x1b[24m", 5);
      buf = new_buf;
      len = new_len;
    }
  }

  return write_loopback(vt_parser, buf, len, 1, 1);
}
#endif

int vt_parser_change_encoding(vt_parser_t *vt_parser, vt_char_encoding_t encoding) {
  ef_parser_t *cc_parser;
  ef_conv_t *cc_conv;

  cc_conv = vt_char_encoding_conv_new(encoding);
  cc_parser = vt_char_encoding_parser_new(encoding);

  if (cc_parser == NULL || cc_conv == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " encoding not changed.\n");
#endif
    if (cc_parser) {
      (*cc_parser->destroy)(cc_parser);
    }

    if (cc_conv) {
      (*cc_conv->destroy)(cc_conv);
    }

    return 0;
  }

#ifdef DEBUG
  bl_warn_printf(BL_DEBUG_TAG " encoding changed.\n");
#endif

  (*vt_parser->cc_parser->destroy)(vt_parser->cc_parser);
  (*vt_parser->cc_conv->destroy)(vt_parser->cc_conv);

  vt_parser->encoding = encoding;
  vt_parser->cc_parser = cc_parser;
  vt_parser->cc_conv = cc_conv;

  /* reset */
  vt_parser->gl = US_ASCII;
  vt_parser->g0 = US_ASCII;
  vt_parser->g1 = US_ASCII;
  vt_parser->is_so = 0;

  vt_parser->is_auto_encoding = 0;

  return 1;
}

size_t vt_parser_convert_to(vt_parser_t *vt_parser, u_char *dst, size_t len,
                            ef_parser_t *parser) {
  return (*vt_parser->cc_conv->convert)(vt_parser->cc_conv, dst, len, parser);
}

void vt_init_encoding_conv(vt_parser_t *vt_parser) {
  (*vt_parser->cc_conv->init)(vt_parser->cc_conv);

  /*
   * XXX
   * this causes unexpected behaviors in some applications(e.g. biew) ,
   * but this is necessary , since 0x00 - 0x7f is not necessarily US-ASCII
   * in these encodings but key input or selection paste assumes that
   * 0x00 - 0x7f should be US-ASCII at the initial state.
   */
  if (IS_STATEFUL_ENCODING(vt_parser->encoding)) {
    init_encoding_parser(vt_parser);
  }
}

int vt_set_auto_detect_encodings(char *encodings) {
  char *p;
  u_int count;

  if (num_auto_detect_encodings > 0) {
    for (count = 0; count < num_auto_detect_encodings; count++) {
      (*auto_detect[count].parser->destroy)(auto_detect[count].parser);
    }

    free(auto_detect);
    num_auto_detect_encodings = 0;
  }

  free(auto_detect_encodings);

  if (*encodings == '\0') {
    auto_detect_encodings = NULL;

    return 1;
  }

  if (!(auto_detect_encodings = strdup(encodings)) ||
      !(auto_detect = malloc(sizeof(*auto_detect) * (bl_count_char_in_str(encodings, ',') + 1)))) {
    free(auto_detect_encodings);

    return 0;
  }

  while ((p = bl_str_sep(&encodings, ","))) {
    if ((auto_detect[num_auto_detect_encodings].encoding = vt_get_char_encoding(p)) !=
        VT_UNKNOWN_ENCODING) {
      num_auto_detect_encodings++;
    }
  }

  if (num_auto_detect_encodings == 0) {
    free(auto_detect);

    return 0;
  }

  for (count = 0; count < num_auto_detect_encodings; count++) {
    auto_detect[count].parser = vt_char_encoding_parser_new(auto_detect[count].encoding);
  }

  return 1;
}

/*
 * XXX
 * ef_map_ucs4_to_iscii() in ef_ucs4_iscii.h is used directly in
 * vt_convert_to_internal_ch(), though it should be used internally in mef
 * library
 */
int ef_map_ucs4_to_iscii(ef_char_t *non_ucs, u_int32_t ucs4_code);

/*
 * Return value
 *  1:  Succeed
 *  0:  Error
 *  -1: Control sequence
 */
int vt_convert_to_internal_ch(vt_parser_t *vt_parser, ef_char_t *orig_ch) {
  ef_char_t ch;

  ch = *orig_ch;

  /*
   * UCS <-> OTHER CS
   */
  if (ch.cs == ISO10646_UCS4_1) {
    u_char decsp;

    if ((vt_parser->unicode_policy & NOT_USE_UNICODE_BOXDRAW_FONT) &&
        (decsp = vt_convert_ucs_to_decsp(ef_char_to_int(&ch)))) {
      ch.ch[0] = decsp;
      ch.size = 1;
      ch.cs = DEC_SPECIAL;
      ch.property = 0;
    }
#if 1
    /* See https://github.com/saitoha/drcsterm/ */
    else if ((vt_parser->unicode_policy & USE_UNICODE_DRCS) &&
             vt_convert_unicode_pua_to_drcs(&ch)) {
      if (ch.cs == US_ASCII) {
        vt_drcs_font_t *font;

        if ((font = vt_drcs_get_font(vt_parser->drcs, US_ASCII, 0)) &&
            vt_drcs_is_picture(font, ch.ch[0])) {
          ch.cs = CS_REVISION_1(US_ASCII);
        }
      }

      /*
       * Go to end to skip 'if (ch.ch[0] == 0x7f) { return 0; }' in next block.
       * Otherwise, 0x10XX7f doesn't work.
       */
      goto end_func;
    }
#endif
    else {
      ef_char_t non_ucs;
      u_int32_t code = ef_char_to_int(&ch);

      ch.property = modify_ucs_property(code, vt_parser->col_size_of_width_a, ch.property);

      if (vt_parser->unicode_policy & NOT_USE_UNICODE_FONT) {
        /* convert ucs4 to appropriate charset */

        if (!is_noconv_unicode(ch.ch) && ef_map_locale_ucs4_to(&non_ucs, &ch) &&
            non_ucs.cs != ISO8859_6_R && /* ARABIC */
            non_ucs.cs != ISO8859_8_R)   /* HEBREW */
        {
          /* Use width property of unicode (the original charset). */
          non_ucs.property = ch.property;
          ch = non_ucs;

          goto end_block;
        }
      }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
      if ((vt_parser->unicode_policy & CONVERT_UNICODE_TO_ISCII) &&
          0x900 <= code && code <= 0xd7f) {
        int ret = ef_map_ucs4_to_iscii(&non_ucs, code);

        if (!HAS_XTERM_LISTENER(vt_parser,check_iscii_font) ||
            /* non_ucs.cs is set if ef_map_ucs4_to_iscii() fails. */
            !(*vt_parser->xterm_listener->check_iscii_font)(vt_parser->xterm_listener->self,
                                                            non_ucs.cs)) {
          goto end_block;
        }

        if (ret) {
          ch.ch[0] = non_ucs.ch[0];
          ch.cs = non_ucs.cs;
          ch.size = 1;
          /* ch.property is not changed. */
        } else {
          switch (code & 0x07f) {
            case 0x0c:
              ch.ch[0] = '\xa6';
              break;
            case 0x3d:
              ch.ch[0] = '\xea';
              break;
            case 0x44:
              ch.ch[0] = '\xdf';
              break;
            case 0x50:
              ch.ch[0] = '\xa1';
              break;
            case 0x58:
              ch.ch[0] = '\xb3';
              break;
            case 0x59:
              ch.ch[0] = '\xb4';
              break;
            case 0x5a:
              ch.ch[0] = '\xb5';
              break;
            case 0x5b:
              ch.ch[0] = '\xba';
              break;
            case 0x5c:
              ch.ch[0] = '\xbf';
              break;
            case 0x5d:
              ch.ch[0] = '\xc0';
              break;
            case 0x5e:
              ch.ch[0] = '\xc9';
              break;
            case 0x60:
              ch.ch[0] = '\xaa';
              break;
            case 0x61:
              ch.ch[0] = '\xa7';
              break;
            case 0x62:
              ch.ch[0] = '\xdb';
              break;
            case 0x63:
              ch.ch[0] = '\xdc';
              break;
            default:
              goto end_block;
          }

          ch.ch[1] = '\xe9';
          /* non_ucs.cs is set if ef_map_ucs4_to_iscii() fails. */
          ch.cs = non_ucs.cs;
          ch.size = 2;
          /* ch.property is not changed. */
        }
      }
#endif

    end_block:
      ;
    }
  } else if (ch.cs != US_ASCII) {
    if ((vt_parser->unicode_policy & ONLY_USE_UNICODE_FONT) ||
        /* XXX converting japanese gaiji to ucs. */
        IS_JIS_EXT(ch.cs) ||
        /* XXX converting RTL characters to ucs. */
        ch.cs == ISO8859_6_R || /* Arabic */
        ch.cs == ISO8859_8_R    /* Hebrew */
#if 0
        /* GB18030_2000 2-byte chars(==GBK) are converted to UCS */
        || (encoding == VT_GB18030 && ch.cs == GBK)
#endif
        ) {
      ef_char_t ucs;

      if (ef_map_to_ucs4(&ucs, &ch)) {
        u_int32_t code = ef_char_to_int(&ucs);
        ef_charset_t orig_cs = ch.cs;

        ch = ucs;

        /* Use width property of the original charset. */
        ch.property = ef_get_ucs_property(code) & ~(EF_FULLWIDTH|EF_AWIDTH);
        if (IS_FULLWIDTH_CS(orig_cs)) {
          ch.property |= EF_FULLWIDTH;
        }
        ch.property = modify_ucs_property(code, vt_parser->col_size_of_width_a, ch.property);
      }
    } else if (IS_FULLWIDTH_CS(ch.cs)) {
      ch.property |= EF_FULLWIDTH;
    }
  }

  if (ch.size == 1) {
    /* single byte cs */
    vt_drcs_font_t *font;

    if ((ch.ch[0] == 0x0 || ch.ch[0] == 0x7f) &&
        (!(font = vt_drcs_get_font(vt_parser->drcs, US_ASCII, 0)) ||
         !(vt_drcs_is_picture(font, ch.ch[0])))) {
      /* DECNULM is always set => discarding 0x0 */
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " 0x0/0x7f sequence is received , ignored...\n");
#endif
      return 0;
    } else if ((ch.ch[0] & 0x7f) <= 0x1f && ch.cs == US_ASCII) {
      /* Control sequence (C0 or C1) */
      return -1;
    }

    if (vt_is_msb_set(ch.cs)) {
      SET_MSB(ch.ch[0]);
    } else {
      u_int16_t ucs;

      if (ch.cs == US_ASCII) {
        /* XXX prev_ch should not be static. */
        static u_char prev_ch;
        static ef_charset_t prev_gl = US_ASCII;

        if (vt_parser->gl == US_ASCII) {
          goto end_func;
        }

        if (IS_CS94MB(vt_parser->gl)) {
          /* iso2022 multi byte characters can be parsed in utf-8 encoding */
          if (vt_parser->gl == prev_gl && prev_ch) {
            ch.ch[1] = ch.ch[0];
            ch.ch[0] = prev_ch;
            ch.size = 2;
            ch.property = EF_FULLWIDTH;
            prev_ch = 0;
            prev_gl = US_ASCII;

            ch.cs = vt_parser->gl;

            goto end_func;
          } else {
            prev_ch = ch.ch[0];
            prev_gl = vt_parser->gl;

            return 0;
          }
        } else {
          ch.cs = vt_parser->gl;
        }
      }

      if (ch.cs == DEC_SPECIAL) {
        if ((vt_parser->unicode_policy & ONLY_USE_UNICODE_BOXDRAW_FONT)) {
          ucs = vt_convert_decsp_to_ucs(ch.ch[0]);
        } else {
          goto end_func;
        }
      } else if (ch.cs == DEC_TECHNICAL) {
        ucs = vt_convert_dectech_to_ucs(ch.ch[0]);
      } else {
        goto end_func;
      }

      if (ucs) {
        ef_int_to_bytes(ch.ch, 4, ucs);
        ch.size = 4;
        ch.cs = ISO10646_UCS4_1;
        /* Use modify_ucs_property() to make it possible to aplly full_width_areas setting. */
        ch.property = modify_ucs_property(ucs, 0 /* not used */, 0 /* Use original property */);
      }
    }
  } else {
    /*
     * NON UCS <-> NON UCS
     */

    /* multi byte cs */

    /*
     * XXX hack
     * how to deal with johab 10-4-4(8-4-4) font ?
     * is there any uhc font ?
     */

    if (ch.cs == JOHAB) {
      ef_char_t uhc;

      if (ef_map_johab_to_uhc(&uhc, &ch) == 0) {
        return 0;
      }

      ch = uhc;
    }

    /*
     * XXX
     * switching option whether this conversion is done should
     * be introduced.
     */
    if (ch.cs == UHC) {
      ef_char_t ksc;

      if (ef_map_uhc_to_ksc5601_1987(&ksc, &ch) == 0) {
        return 0;
      }

      ch = ksc;
    }
  }

end_func:
  *orig_ch = ch;

  return 1;
}

void vt_parser_set_alt_color_mode(vt_parser_t *vt_parser, vt_alt_color_mode_t mode) {
  vt_parser->alt_color_mode = mode;
}

void vt_set_broadcasting(int flag) {
  is_broadcasting = flag;
}

int vt_parser_is_broadcasting(vt_parser_t *vt_parser) {
  return (is_broadcasting && !vt_parser->ignore_broadcasted_chars);
}

int true_or_false(const char *str) {
  if (strcmp(str, "true") == 0) {
    return 1;
  } else if (strcmp(str, "false") == 0) {
    return 0;
  } else {
    return -1;
  }
}

int vt_parser_get_config(
    vt_parser_t *vt_parser,
    vt_pty_t *output, /* if vt_parser->pty == output, NULL is set */
    const char *key, int to_menu, int *flag) {
  char *value;
  char digit[DIGIT_STR_LEN(u_int) + 1];
  char cwd[PATH_MAX];

  if (strcmp(key, "encoding") == 0) {
    value = vt_get_char_encoding_name(vt_parser->encoding);
  } else if (strcmp(key, "is_auto_encoding") == 0) {
    if (vt_parser->is_auto_encoding) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "word_separators") == 0) {
    value = vt_get_word_separators();
  } else if (strcmp(key, "regard_uri_as_word") == 0) {
    if (vt_get_regard_uri_as_word()) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_alt_buffer") == 0) {
    if (use_alt_buffer) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "vt_color_mode") == 0) {
    value = vt_get_color_mode();
  } else if (strcmp(key, "use_ansi_colors") == 0) {
    if (vt_parser->use_ansi_colors) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "tabsize") == 0) {
    sprintf(digit, "%d", vt_screen_get_tab_size(vt_parser->screen));
    value = digit;
  } else if (strcmp(key, "logsize") == 0) {
    if (vt_screen_log_size_is_unlimited(vt_parser->screen)) {
      value = "unlimited";
    } else {
      sprintf(digit, "%d", vt_screen_get_log_size(vt_parser->screen));
      value = digit;
    }
  } else if (strcmp(key, "static_backscroll_mode") == 0) {
    if (vt_get_backscroll_mode(vt_parser->screen) == BSM_STATIC) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_combining") == 0) {
    if (vt_parser->use_char_combining) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "col_size_of_width_a") == 0) {
    if (vt_parser->col_size_of_width_a == 2) {
      value = "2";
    } else {
      value = "1";
    }
  } else if (strcmp(key, "locale") == 0) {
    value = bl_get_locale();
  } else if (strcmp(key, "pwd") == 0) {
    value = getcwd(cwd, sizeof(cwd));
  } else if (strcmp(key, "logging_vt_seq") == 0) {
    if (vt_parser->logging_vt_seq) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "vt_seq_format") == 0) {
    if (use_ttyrec_format) {
      value = "ttyrec";
    } else {
      value = "raw";
    }
  } else if (strcmp(key, "rows") == 0) {
    sprintf(digit, "%d", vt_screen_get_logical_rows(vt_parser->screen));
    value = digit;
  } else if (strcmp(key, "cols") == 0) {
    sprintf(digit, "%d", vt_screen_get_logical_cols(vt_parser->screen));
    value = digit;
  } else if (strcmp(key, "not_use_unicode_font") == 0) {
    if (vt_parser->unicode_policy & NOT_USE_UNICODE_FONT) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "only_use_unicode_font") == 0) {
    if (vt_parser->unicode_policy & ONLY_USE_UNICODE_FONT) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "box_drawing_font") == 0) {
    if (vt_parser->unicode_policy & NOT_USE_UNICODE_BOXDRAW_FONT) {
      value = "decsp";
    } else if (vt_parser->unicode_policy & ONLY_USE_UNICODE_BOXDRAW_FONT) {
      value = "unicode";
    } else {
      value = "noconv";
    }
  } else if (strcmp(key, "auto_detect_encodings") == 0) {
    if ((value = auto_detect_encodings) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "use_auto_detect") == 0) {
    if (vt_parser->use_auto_detect) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "allow_scp") == 0) {
#ifdef USE_LIBSSH2
    if (use_scp_full) {
      value = "true";
    } else
#endif
    {
      value = "false";
    }
  } else if (strcmp(key, "unicode_noconv_areas") == 0) {
    response_area_table(vt_parser->pty, key, unicode_noconv_areas, num_unicode_noconv_areas,
                        to_menu);

    return 1;
  } else if (strcmp(key, "unicode_full_width_areas") == 0) {
    response_area_table(vt_parser->pty, key, full_width_areas, num_full_width_areas, to_menu);

    return 1;
  } else if (strcmp(key, "unicode_half_width_areas") == 0) {
    response_area_table(vt_parser->pty, key, half_width_areas, num_half_width_areas, to_menu);

    return 1;
  } else if (strcmp(key, "blink_cursor") == 0) {
    if (vt_parser->cursor_style & CS_BLINK) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "ignore_broadcasted_chars") == 0) {
    if (vt_parser->ignore_broadcasted_chars) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "broadcast") == 0) {
    if (is_broadcasting) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_multi_column_char") == 0) {
    if (vt_parser->use_multi_col_char) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "old_drcs_sixel") == 0) {
    if (old_drcs_sixel) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "send_file") == 0) {
    if (send_file) {
      value = send_file;
    } else {
      value = "";
    }
  } else if (strcmp(key, "receive_directory") == 0) {
    if (recv_dir) {
      value = recv_dir;
    } else {
      value = "";
    }
  } else if (strcmp(key, "local_echo_wait") == 0) {
    sprintf(digit, "%d", local_echo_wait_msec);
    value = digit;
  } else if (strcmp(key, "use_local_echo") == 0) {
    if (vt_parser->use_local_echo) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "format_other_keys") == 0) {
    if (format_other_keys) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_locked_title") == 0) {
    if (vt_parser->use_locked_title) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_arabic_dynamic_comb") == 0) {
    if (vt_get_use_arabic_dynamic_comb()) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "challenge") == 0) {
    value = vt_get_proto_challenge();
    if (to_menu < 0) {
      to_menu = 0;
    }
  } else {
    /* Continue to process it in x_screen.c */
    return 0;
  }

  if (!output) {
    output = vt_parser->pty;
  }

/* value is never set NULL above. */
#if 0
  if (!value) {
    vt_response_config(output, "error", NULL, to_menu);
  }
#endif

  if (flag) {
    *flag = value ? true_or_false(value) : -1;
  } else {
    vt_response_config(output, key, value, to_menu);
  }

  return 1;
}

/* Called in visual context */
int vt_parser_set_config(vt_parser_t *vt_parser, const char *key, const char *value) {
  char *tmp;

  if (strcmp(key, "encoding") == 0) {
    if (strcmp(value, "auto") == 0) {
      vt_parser->is_auto_encoding = strcasecmp(value, "auto") == 0 ? 1 : 0;
    }

    return 0; /* Continue to process it in x_screen.c */
  } else if (strcmp(key, "logging_msg") == 0) {
    if (true_or_false(value) > 0) {
      bl_set_msg_log_file_name("mlterm/msg.log");
    } else {
      bl_set_msg_log_file_name(NULL);
    }
  } else if (strcmp(key, "word_separators") == 0) {
    vt_set_word_separators(value);
  } else if (strcmp(key, "regard_uri_as_word") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_set_regard_uri_as_word(flag);
    }
  } else if (strcmp(key, "vt_color_mode") == 0) {
    vt_set_color_mode(value);
  } else if (strcmp(key, "use_alt_buffer") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      use_alt_buffer = flag;
    }
  } else if (strcmp(key, "use_ansi_colors") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->use_ansi_colors = flag;
    }
  } else if (strcmp(key, "unicode_noconv_areas") == 0) {
    if ((tmp = alloca(strlen(value) + 1))) {
      vt_set_unicode_noconv_areas(strcpy(tmp, value));
    }
  } else if (strcmp(key, "unicode_full_width_areas") == 0) {
    if ((tmp = alloca(strlen(value) + 1))) {
      vt_set_full_width_areas(strcpy(tmp, value));
    }
  } else if (strcmp(key, "unicode_half_width_areas") == 0) {
    if ((tmp = alloca(strlen(value) + 1))) {
      vt_set_half_width_areas(strcpy(tmp, value));
    }
  } else if (strcmp(key, "tabsize") == 0) {
    u_int tab_size;

    if (bl_str_to_uint(&tab_size, value)) {
      vt_screen_set_tab_size(vt_parser->screen, tab_size);
    }
  } else if (strcmp(key, "static_backscroll_mode") == 0) {
    vt_bs_mode_t mode;

    if (strcmp(value, "true") == 0) {
      mode = BSM_STATIC;
    } else if (strcmp(value, "false") == 0) {
      mode = BSM_DEFAULT;
    } else {
      return 1;
    }

    vt_set_backscroll_mode(vt_parser->screen, mode);
  } else if (strcmp(key, "use_combining") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->use_char_combining = flag;
    }
  } else if (strcmp(key, "col_size_of_width_a") == 0) {
    u_int size;

    if (strcmp(value, "switch") == 0) {
      size = vt_parser->col_size_of_width_a == 1 ? 2 : 1;
    } else if (!bl_str_to_uint(&size, value)) {
      goto end;
    }

    set_col_size_of_width_a(vt_parser, size);

  end:
    ;
  } else if (strcmp(key, "locale") == 0) {
    bl_locale_init(value);
  } else if (strcmp(key, "logging_vt_seq") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->logging_vt_seq = flag;
    }
  } else if (strcmp(key, "vt_seq_format") == 0) {
    use_ttyrec_format = (strcmp(value, "ttyrec") == 0);
  } else if (strcmp(key, "geometry") == 0) {
    u_int cols;
    u_int rows;

    if (sscanf(value, "%ux%u", &cols, &rows) == 2) {
      resize(vt_parser, cols, rows, 1);
    }
  } else if (strcmp(key, "box_drawing_font") == 0) {
    if (strcmp(value, "unicode") == 0) {
      vt_parser->unicode_policy &= ~NOT_USE_UNICODE_BOXDRAW_FONT;
      vt_parser->unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT;
    } else if (strcmp(value, "decsp") == 0) {
      vt_parser->unicode_policy &= ~ONLY_USE_UNICODE_BOXDRAW_FONT;
      vt_parser->unicode_policy |= NOT_USE_UNICODE_BOXDRAW_FONT;
    } else {
      vt_parser->unicode_policy &=
          (~NOT_USE_UNICODE_BOXDRAW_FONT & ~ONLY_USE_UNICODE_BOXDRAW_FONT);
    }
  } else if (strcmp(key, "auto_detect_encodings") == 0) {
    if ((tmp = alloca(strlen(value) + 1))) {
      vt_set_auto_detect_encodings(strcpy(tmp, value));
    }
  } else if (strcmp(key, "use_auto_detect") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->use_auto_detect = flag;
    }
  } else if (strcmp(key, "blink_cursor") == 0) {
    if (strcmp(value, "true") == 0) {
      vt_parser->cursor_style |= CS_BLINK;
    } else {
      vt_parser->cursor_style &= ~CS_BLINK;
    }
  } else if (strcmp(key, "ignore_broadcasted_chars") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->ignore_broadcasted_chars = flag;
    }
  } else if (strcmp(key, "broadcast") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      is_broadcasting = flag;
    }
  } else if (strcmp(key, "use_multi_column_char") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->use_multi_col_char = flag;
    }
  } else if (strcmp(key, "old_drcs_sixel") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      old_drcs_sixel = flag;
    }
  } else if (strcmp(key, "local_echo_wait") == 0) {
    u_int msec;

    if (bl_str_to_uint(&msec, value)) {
      local_echo_wait_msec = msec;
    }
  } else if (strcmp(key, "use_local_echo") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      if ((vt_parser->use_local_echo ? 1 : 0) != flag && !(vt_parser->use_local_echo = flag)) {
        vt_screen_logical(vt_parser->screen);
        vt_screen_disable_local_echo(vt_parser->screen);
        vt_screen_visual(vt_parser->screen);
      }
    }
  } else if (strcmp(key, "format_other_keys") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      format_other_keys = flag;
    }
  } else if (strcmp(key, "use_locked_title") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_parser->use_locked_title = flag;
    }
  } else if (strcmp(key, "use_arabic_dynamic_comb") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_set_use_arabic_dynamic_comb(flag);
    }
  } else if (strncmp(key, "send_file", 9) == 0) {
    if (strstr(value, "..")) {
      /* insecure file name */
      bl_msg_printf("%s is insecure file name.\n", value);
    } else {
      if (key[9] == '\0') {
        free(send_file);
        send_file = convert_to_locale_encoding(value, vt_parser->cc_parser, vt_parser->encoding);
      } else if (strcmp(key + 9, "_utf8") == 0) {
        /* Hack for set_xdnd_config() in ui_screen.c */
        free(send_file);
        send_file = convert_to_locale_encoding(value, NULL, VT_UTF8);
      }
    }
  } else {
    /* Continue to process it in x_screen.c */
    return 0;
  }

  return 1;
}

/* Called in visual context */
int vt_parser_exec_cmd(vt_parser_t *vt_parser, char *cmd) {
  if (strcmp(cmd, "gen_proto_challenge") == 0) {
    vt_gen_proto_challenge();
  } else if (strcmp(cmd, "full_reset") == 0) {
    vt_parser_reset(vt_parser, 0);
  } else if (strncmp(cmd, "snapshot", 8) == 0) {
    char **argv;
    int argc;

    if ((argv = bl_argv_alloca(cmd)) && bl_arg_str_to_array(argv, &argc, cmd)) {
      vt_char_encoding_t encoding;
      char *file;

      if (argc >= 3) {
        encoding = vt_get_char_encoding(argv[2]);
      } else {
        encoding = VT_UNKNOWN_ENCODING;
      }

      if (argc >= 2) {
        file = argv[1];
      } else {
        /* skip /dev/ */
        file = vt_pty_get_slave_name(vt_parser->pty) + 5;
      }

      if (strstr(file, "..")) {
        /* insecure file name */
        bl_msg_printf("%s is insecure file name.\n", file);
      } else {
        snapshot(vt_parser, encoding, file, WCA_ALL);
      }
    }
  } else if (strcmp(cmd, "zmodem_start") == 0) {
    int count = 0;

    while (1) {
      if (!vt_pty_is_loopback(vt_parser->pty)) {
        if ((send_file || recv_dir || (recv_dir = bl_get_user_rc_path("mlterm/recv"))) &&
            vt_transfer_start(send_file, recv_dir, 0,
                              vt_screen_get_cols(vt_parser->screen) / 2 + 1)) {
          vt_parser->is_zmodem_ready = 0;
          vt_parser->is_transferring_data = (send_file ? 0x1 : 0x2);
          vt_parser->r_buf.left = 0;
          transfer_data(vt_parser);
        } else {
          transfer_cancel(vt_parser, 1);
          /* send_file is freed in vt_transfer_start() */
        }
        send_file = NULL;

        break;
      }

      if (++count == 10) {
        bl_msg_printf("Retry zmodem_start.\n");

        break;
      }

      bl_usleep(100000);
    }
  }
#if !defined(NO_IMAGE) && defined(ENABLE_OSC5379PICTURE)
  else if (strncmp(cmd, "show_picture ", 13) == 0 || strncmp(cmd, "add_frame ", 10) == 0) {
    int clip_beg_col = 0;
    int clip_beg_row = 0;
    int clip_cols = 0;
    int clip_rows = 0;
    int img_cols = 0;
    int img_rows = 0;
    char **argv;
    int argc;

    if (!(argv = bl_argv_alloca(cmd)) || !bl_arg_str_to_array(argv, &argc, cmd) || argc == 1) {
      return 1;
    }

    if (argc >= 3) {
      int has_img_size;

      if (strchr(argv[argc - 1], '+')) {
        sscanf(argv[argc - 1], "%dx%d+%d+%d", &clip_cols, &clip_rows, &clip_beg_col, &clip_beg_row);

        has_img_size = (argc >= 4);
      } else {
        has_img_size = 1;
      }

      if (has_img_size) {
        sscanf(argv[2], "%dx%d", &img_cols, &img_rows);
      }
    }

    if (*argv[0] == 's') {
      show_picture(vt_parser, argv[1], clip_beg_col, clip_beg_row, clip_cols, clip_rows,
                   img_cols, img_rows, 0, 0);
    } else if (HAS_XTERM_LISTENER(vt_parser, add_frame_to_animation)) {
      (*vt_parser->xterm_listener->add_frame_to_animation)(vt_parser->xterm_listener->self,
                                                              argv[1], &img_cols, &img_rows);
    }
  }
#endif
#ifdef USE_LIBSSH2
  else if (strncmp(cmd, "scp ", 4) == 0) {
    char **argv;
    int argc;

    if ((argv = bl_argv_alloca(cmd)) && bl_arg_str_to_array(argv, &argc, cmd) &&
        (argc == 3 || argc == 4)) {
      vt_char_encoding_t encoding;

      if (!argv[3] || (encoding = vt_get_char_encoding(argv[3])) == VT_UNKNOWN_ENCODING) {
        encoding = vt_parser->encoding;
      }

      vt_pty_ssh_scp(vt_parser->pty, vt_parser->encoding, encoding, argv[2], argv[1],
                     use_scp_full, recv_dir, vt_screen_get_cols(vt_parser->screen) / 2 + 1);
    }
  }
#endif
  else {
    return 0;
  }

  return 1;
}

/*
 * level=1: Unuse loopback, Output ZCAN.
 */
void vt_parser_reset(vt_parser_t *vt_parser, int level) {
#ifdef USE_LIBSSH2
  if (level >= 1) {
    if (vt_pty_is_loopback(vt_parser->pty)) {
      if (vt_pty_get_mode(vt_parser->pty) == PTY_MOSH) {
        vt_pty_mosh_set_use_loopback(vt_parser->pty, 0);
      } else {
        vt_pty_ssh_set_use_loopback(vt_parser->pty, 0);
      }
    }
  }
#endif

  soft_reset(vt_parser);
  vt_parser->r_buf.left = 0; /* Reset DCS or OSC etc sequence */
  vt_parser->sixel_scrolling = 1;
  transfer_cancel(vt_parser, level >= 1);
}

#define MOUSE_POS_LIMIT (0xff - 0x20)
#define EXT_MOUSE_POS_LIMIT (0x7ff - 0x20)

void vt_parser_report_mouse_tracking(vt_parser_t *vt_parser, int col, int row,
                                           int button,
                                           int is_released, /* is_released is 0 if PointerMotion */
                                           int key_state, int button_state) {
  if (vt_parser->mouse_mode >= LOCATOR_REPORT) {
    char seq[10 + DIGIT_STR_LEN(int)*4 + 1];
    int ev;
    int is_outside_filter_rect;

    is_outside_filter_rect = 0;
    if (vt_parser->loc_filter.top > row || vt_parser->loc_filter.left > col ||
        vt_parser->loc_filter.bottom < row || vt_parser->loc_filter.right < col) {
      vt_parser->loc_filter.top = vt_parser->loc_filter.bottom = row;
      vt_parser->loc_filter.left = vt_parser->loc_filter.right = col;

      if (vt_parser->locator_mode & LOCATOR_FILTER_RECT) {
        vt_parser->locator_mode &= ~LOCATOR_FILTER_RECT;
        is_outside_filter_rect = 1;
      }
    }

    if (button == 0) {
      if (is_outside_filter_rect) {
        ev = 10;
      } else if (vt_parser->locator_mode & LOCATOR_REQUEST) {
        ev = 1;
      } else {
        return;
      }
    } else {
      if ((is_released && !(vt_parser->locator_mode & LOCATOR_BUTTON_UP)) ||
          (!is_released && !(vt_parser->locator_mode & LOCATOR_BUTTON_DOWN))) {
        return;
      }

      if (button == 1) {
        ev = is_released ? 3 : 2;
      } else if (button == 2) {
        ev = is_released ? 5 : 4;
      } else if (button == 3) {
        ev = is_released ? 7 : 6;
      } else {
        ev = 1;
      }
    }

    sprintf(seq, "\x1b[%d;%d;%d;%d;%d&w", ev, button_state, row, col,
            vt_screen_get_page_id(vt_parser->screen) + 1);

    vt_write_to_pty(vt_parser->pty, seq, strlen(seq));

    if (vt_parser->locator_mode & LOCATOR_ONESHOT) {
      set_mouse_report(vt_parser, 0);
      vt_parser->locator_mode = 0;
    }
  } else {
    /*
     * Max length is SGR style => ESC [ < %d ; %d(col) ; %d(row) ; %c('M' or
     * 'm') NULL
     *                            1   1 1 3  1 3       1  3      1 1 1
     */
    u_char seq[17];
    size_t seq_len;
    vt_extended_mouse_report_mode_t ext_mouse_mode;

    if (vt_parser->ext_mouse_mode == EXTENDED_MOUSE_REPORT_SGR_PIXEL) {
      ext_mouse_mode = EXTENDED_MOUSE_REPORT_SGR;
    } else {
      ext_mouse_mode = vt_parser->ext_mouse_mode;
    }

    if (is_released && ext_mouse_mode != EXTENDED_MOUSE_REPORT_SGR) {
      key_state = 0;
      button = 3;
    } else if (button == 0) {
      /* PointerMotion */
      button = 3 + 32;
    } else {
      if (is_released) {
        /* for EXTENDED_MOUSE_REPORT_SGR */
        key_state += 0x80;
      }

      button--; /* button1 == 0, button2 == 1, button3 == 2 */

      while (button >= 3) {
        /* Wheel mouse */
        key_state += 64;
        button -= 3;
      }
    }

    if (ext_mouse_mode == EXTENDED_MOUSE_REPORT_SGR) {
      sprintf(seq, "\x1b[<%d;%d;%d%c", (button + key_state) & 0x7f, col, row,
              ((button + key_state) & 0x80) ? 'm' : 'M');
      seq_len = strlen(seq);
    } else if (ext_mouse_mode == EXTENDED_MOUSE_REPORT_URXVT) {
      sprintf(seq, "\x1b[%d;%d;%dM", 0x20 + button + key_state, col, row);
      seq_len = strlen(seq);
    } else {
      memcpy(seq, "\x1b[M", 3);
      seq[3] = 0x20 + button + key_state;

      if (ext_mouse_mode == EXTENDED_MOUSE_REPORT_UTF8) {
        int ch;
        u_char *p;

        p = seq + 4;

        if (col > EXT_MOUSE_POS_LIMIT) {
          col = EXT_MOUSE_POS_LIMIT;
        }

        if ((ch = 0x20 + col) >= 0x80) {
          *(p++) = ((ch >> 6) & 0x1f) | 0xc0;
          *(p++) = (ch & 0x3f) | 0x80;
        } else {
          *(p++) = ch;
        }

        if (row > EXT_MOUSE_POS_LIMIT) {
          row = EXT_MOUSE_POS_LIMIT;
        }

        if ((ch = 0x20 + row) >= 0x80) {
          *(p++) = ((ch >> 6) & 0x1f) | 0xc0;
          *p = (ch & 0x3f) | 0x80;
        } else {
          *p = ch;
        }

        seq_len = p - seq + 1;
      } else {
        seq[4] = 0x20 + (col < MOUSE_POS_LIMIT ? col : MOUSE_POS_LIMIT);
        seq[5] = 0x20 + (row < MOUSE_POS_LIMIT ? row : MOUSE_POS_LIMIT);
        seq_len = 6;
      }
    }

    vt_write_to_pty(vt_parser->pty, seq, seq_len);

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " [reported cursor pos] %d %d\n", col, row);
#endif
  }
}

#ifdef BL_DEBUG

#include <assert.h>

void TEST_vt_parser(void) {
#ifndef NO_IMAGE
  u_char a[] = "\x1b]8k @\x1f\x1bP;1";
  u_char b[] = "\x1b]k @\x1f\x90;1";
  u_char c[] = "\x1b]16k @@@@\x1f\x1bP0;1";
  u_char d[] = "\x1b]A @\x90;2";
  u_char e[] = "\x1b]A hogefuga\x1f\x1bP;1";
  u_char f[] = "\x1b]A hogefug\x1f\x1bP;1";

  assert(is_separated_sixel(a, sizeof(a) - 1) == 1);
  assert(is_separated_sixel(b, sizeof(b) - 1) == 1);
  assert(is_separated_sixel(c, sizeof(c) - 1) == 1);
  assert(is_separated_sixel(d, sizeof(d) - 1) == 0);
  assert(is_separated_sixel(e, sizeof(e) - 1) == 0);
  assert(is_separated_sixel(f, sizeof(f) - 1) == 1);

  bl_msg_printf("PASS vt_parser test.\n");
#endif
}

#endif
