/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_font_config.h"

#include <string.h> /* memset */

#include <pobl/bl_mem.h>  /* malloc */
#include <pobl/bl_str.h>  /* strdup */
#include <pobl/bl_util.h> /* DIGIT_STR_LEN */
#include <pobl/bl_conf_io.h>
#include <pobl/bl_file.h>
#include <pobl/bl_debug.h>

#include <vt_char.h>
#include <vt_char_encoding.h> /* vt_parse_unicode_area */

#define DEFAULT_FONT 0x1ff /* MAX_CHARSET */

#if 0
#define __DEBUG
#endif

typedef struct cs_table {
  char *name;
  ef_charset_t cs;

} cs_table_t;

typedef struct custom_cache {
  const char *file;
  char *key;
  char *value;

} custom_cache_t;

/* --- static variables --- */

#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE) || defined(USE_WAYLAND)

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
static int use_aafont;
#define FONT_FILE (use_aafont ? aafont_file + 7 : "font")
#define VFONT_FILE (use_aafont ? vaafont_file + 7 : "vfont")
#define TFONT_FILE (use_aafont ? taafont_file + 7 : "tfont")
#else
#define FONT_FILE "font"
#define VFONT_FILE "vfont"
#define TFONT_FILE "tfont"
#endif

static char *font_file = "mlterm/font-fb";
static char *vfont_file = "mlterm/vfont-fb";
static char *tfont_file = "mlterm/tfont-fb";

#else

#define FONT_FILE (font_file + 7)
#define VFONT_FILE (vfont_file + 7)
#define TFONT_FILE (tfont_file + 7)
static char *font_file = "mlterm/font";
static char *vfont_file = "mlterm/vfont";
static char *tfont_file = "mlterm/tfont";

#endif

static char *aafont_file = "mlterm/aafont";
static char *vaafont_file = "mlterm/vaafont";
static char *taafont_file = "mlterm/taafont";

/*
 * If this table is changed, ui_font.c:cs_info_table and mc_font.c:cs_info_table
 * shoule be also changed.
 */
static cs_table_t cs_table[] = {
    {"ISO10646_UCS4_1", ISO10646_UCS4_1},

    {"DEC_SPECIAL", DEC_SPECIAL},
    {"ISO8859_1", ISO8859_1_R},
    {"ISO8859_2", ISO8859_2_R},
    {"ISO8859_3", ISO8859_3_R},
    {"ISO8859_4", ISO8859_4_R},
    {"ISO8859_5", ISO8859_5_R},
    {"ISO8859_6", ISO8859_6_R},
    {"ISO8859_7", ISO8859_7_R},
    {"ISO8859_8", ISO8859_8_R},
    {"ISO8859_9", ISO8859_9_R},
    {"ISO8859_10", ISO8859_10_R},
    {"TIS620", TIS620_2533},
    {"ISO8859_13", ISO8859_13_R},
    {"ISO8859_14", ISO8859_14_R},
    {"ISO8859_15", ISO8859_15_R},
    {"ISO8859_16", ISO8859_16_R},
    {"TCVN5712", TCVN5712_3_1993},
    {"ISCII_ASSAMESE", ISCII_ASSAMESE},
    {"ISCII_BENGALI", ISCII_BENGALI},
    {"ISCII_GUJARATI", ISCII_GUJARATI},
    {"ISCII_HINDI", ISCII_HINDI},
    {"ISCII_KANNADA", ISCII_KANNADA},
    {"ISCII_MALAYALAM", ISCII_MALAYALAM},
    {"ISCII_ORIYA", ISCII_ORIYA},
    {"ISCII_PUNJABI", ISCII_PUNJABI},
    {"ISCII_TAMIL", ISCII_TAMIL},
    {"ISCII_TELUGU", ISCII_TELUGU},
    {"VISCII", VISCII},
    {"KOI8_R", KOI8_R},
    {"KOI8_U", KOI8_U},
#if 0
    /*
     * Koi8_t and georgian_ps charsets can be shown by unicode font only.
     */
    {"KOI8_T", KOI8_T},
    {"GEORGIAN_PS", GEORGIAN_PS},
#endif
#ifdef USE_WIN32GUI
    {"CP1250", CP1250},
    {"CP1251", CP1251},
    {"CP1252", CP1252},
    {"CP1253", CP1253},
    {"CP1254", CP1254},
    {"CP1255", CP1255},
    {"CP1256", CP1256},
    {"CP1257", CP1257},
    {"CP1258", CP1258},
#endif
    {"JISX0201_KATA", JISX0201_KATA},
    {"JISX0201_ROMAN", JISX0201_ROMAN},
    {"JISX0208_1978", JISC6226_1978},
    {"JISC6226_1978", JISC6226_1978},
    {"JISX0208_1983", JISX0208_1983},
    {"JISX0208_1990", JISX0208_1990},
    {"JISX0212_1990", JISX0212_1990},
    {"JISX0213_2000_1", JISX0213_2000_1},
    {"JISX0213_2000_2", JISX0213_2000_2},
    {"KSC5601_1987", KSC5601_1987},
    {"KSX1001_1997", KSC5601_1987},
#if 0
    /*
     * XXX
     * UHC and JOHAB fonts are not used at the present time.
     * see vt_vt100_parser.c:vt_parse_vt100_sequence().
     */
    {"UHC", UHC},
    {"JOHAB", JOHAB},
#endif
    {"GB2312_80", GB2312_80},
    {"GBK", GBK},
    {"BIG5", BIG5},
    {"HKSCS", HKSCS},
    {"CNS11643_1992_1", CNS11643_1992_1},
    {"CNS11643_1992_2", CNS11643_1992_2},
    {"CNS11643_1992_3", CNS11643_1992_3},
    {"CNS11643_1992_4", CNS11643_1992_4},
    {"CNS11643_1992_5", CNS11643_1992_5},
    {"CNS11643_1992_6", CNS11643_1992_6},
    {"CNS11643_1992_7", CNS11643_1992_7},

};

static ui_font_config_t **font_configs;
static u_int num_of_configs;

/*
 * These will be leaked unless change_custom_cache( ... , "") deletes them.
 * change_custom_cache( ... , "") is called only from save_conf, which means
 * that they are deleted when all of them are saved to ~/.mlterm/(vt)(aa)font
 * file.
 */
static custom_cache_t *custom_cache;
static u_int num_of_customs;

/* --- static functions --- */

#ifdef BL_DEBUG
static void TEST_font_config(void);
#endif

static BL_PAIR(ui_font_name) get_font_name_pair(BL_MAP(ui_font_name) table, vt_font_t font) {
  BL_PAIR(ui_font_name) pair;

  bl_map_get(table, font, pair);

  return pair;
}

static BL_PAIR(ui_font_name) * get_font_name_pairs_array(u_int *size, BL_MAP(ui_font_name) table) {
  BL_PAIR(ui_font_name) * array;

  bl_map_get_pairs_array(table, array, *size);

  return array;
}

static int set_font_name_to_table(BL_MAP(ui_font_name) table, vt_font_t font, char *fontname) {
  int result;

  bl_map_set(result, table, font, fontname);

  return result;
}

static vt_font_t parse_key(const char *key) {
  int count;
  size_t key_len;
  ef_charset_t cs;
  vt_font_t font;

  key_len = strlen(key);

  if (key_len >= 7 && strncmp(key, "DEFAULT", 7) == 0) {
    font = DEFAULT_FONT;

    goto check_style;
  }

  if (key_len >= 3 && strncmp(key, "U+", 2) == 0) {
    u_int min;
    u_int max;

    if (vt_parse_unicode_area(key, &min, &max) &&
        (font = vt_get_unicode_area_font(min, max)) != UNKNOWN_CS) {
      goto check_style;
    } else {
      return UNKNOWN_CS;
    }
  }

  for (count = 0; count < sizeof(cs_table) / sizeof(cs_table[0]); count++) {
    size_t nlen;

    nlen = strlen(cs_table[count].name);

    if (key_len >= nlen && strncmp(cs_table[count].name, key, nlen) == 0 &&
        (key[nlen] == '\0' ||
         /* "_BOLD" or "_FULLWIDTH" is trailing */ key[nlen] == '_')) {
      cs = cs_table[count].cs;

      break;
    }
  }

  if (count == sizeof(cs_table) / sizeof(cs_table[0])) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " %s is not valid charset.\n", key);
#endif

    return UNKNOWN_CS;
  }

  font = NORMAL_FONT_OF(cs);

  if (!(font & FONT_FULLWIDTH) && (strstr(key, "_BIWIDTH") || /* XXX compat with 3.2.2 or before. */
                                   strstr(key, "_FULLWIDTH"))) {
    font |= FONT_FULLWIDTH;
  }

check_style:
  if (strstr(key, "_BOLD")) {
    font |= FONT_BOLD;
  }

  if (strstr(key, "_ITALIC")) {
    font |= FONT_ITALIC;
  }

  return font;
}

static int parse_value(char **font_name, /* if value is "" or illegal format, not changed. */
                       char *value       /* Don't specify NULL. */
                       ) {
  if (strchr(value, ',')) {
    /* XXX Compat with old format (3.6.3 or before): [size],[font name] */

    /*
     * bl_str_sep() never returns NULL and and value never becomes NULL
     * because strchr( value , ',') is succeeded.
     */
    bl_str_sep(&value, ",");
  }

  *font_name = value;

  return 1;
}

/*
 * <Return value>
 * 1: Valid "%d" or no '%' is found.
 * 0: Invalid '%' is found.
 */
static int is_valid_font_format(const char *format) {
  char *p;

  if ((p = strchr(format, '%'))) {
    /* force to be '%d' */
    if (p[1] != 'd') {
      return 0;
    }

    /* '%' can happen only once at most */
    if (p != strrchr(format, '%')) {
      return 0;
    }
  }

  return 1;
}

/*
 * <Return value>
 * 0: Not changed(including the case of failure).
 * 1: Succeeded.
 */
static int customize_font_name(ui_font_config_t *font_config, vt_font_t font,
                               const char *fontname) {
  BL_PAIR(ui_font_name) pair;

  if (is_valid_font_format(fontname) == 0) {
    bl_msg_printf("%s is invalid format for font name.\n");

    return 0;
  }

  if ((pair = get_font_name_pair(font_config->font_name_table, font))) {
    if (*fontname == '\0') {
      int result;

      /* Curent setting in font_config is removed. */
      free(pair->value);
      bl_map_erase_simple(result, font_config->font_name_table, font);
    } else if (strcmp(pair->value, fontname) != 0) {
      char *value;

      if ((value = strdup(fontname)) == NULL) {
        return 0;
      }

      free(pair->value);
      pair->value = value;
    } else {
      /* If new fontname is the same as current one, nothing is done. */
      return 0;
    }
  } else {
    char *value;

    if (*fontname == '\0' || (value = strdup(fontname)) == NULL) {
      return 0;
    }

    set_font_name_to_table(font_config->font_name_table, font, value);
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Set %x font => fontname %s.\n", font, fontname);
#endif

  return 1;
}

static int parse_conf(ui_font_config_t *font_config, const char *key,
                      const char *value /* value = "" or ";" => Reset font name. */
                      ) {
  vt_font_t font;
  char *font_name;

  if ((font = parse_key(key)) == UNKNOWN_CS) {
    return 0;
  }

  /*
   * XXX Compat with old formats (3.6.3 or before): [entry];[entry];[entry];...
   *
   * bl_str_sep() returns NULL only if value == NULL.
   */
  font_name = bl_str_alloca_dup(value);
  font_name = bl_str_sep(&font_name, ";");

  if (parse_value(&font_name, font_name)) {
    customize_font_name(font_config, font, font_name);
  }

  return 1;
}

static int apply_custom_cache(ui_font_config_t *font_config, const char *filename) {
  u_int count;

  for (count = 0; count < num_of_customs; count++) {
    if (filename == custom_cache[count].file) {
#ifdef __DEBUG
      bl_debug_printf("Appling customization %s=%s\n", custom_cache[count].key,
                      custom_cache[count].value);
#endif

      parse_conf(font_config, custom_cache[count].key, custom_cache[count].value);
    }
  }

  return 1;
}

static int read_conf(ui_font_config_t *font_config, const char *filename) {
  bl_file_t *from;
  char *key;
  char *value;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " read_conf( %s)\n", filename);
#endif

  if (!(from = bl_file_open(filename, "r"))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " %s couldn't be opened.\n", filename);
#endif

    return 0;
  }

  while (bl_conf_io_read(from, &key, &value)) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Read line from %s => %s = %s\n", filename, key, value);
#endif

    parse_conf(font_config, key, value);
  }

  bl_file_close(from);

  return 1;
}

static int read_all_conf(ui_font_config_t *font_config,
                         const char *changed_font_file /* If this function is
                                                        * called after a font
                                                        * file is
                                                        * changed, specify it
                                                        * here to avoid re-read
                                                        * font files.
                                                        * Otherwise specify
                                                        * NULL. */
                         ) {
  char *font_rcfile;
  char *font_rcfile2; /* prior to font_rcfile */
  char *rcpath;

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  if (use_aafont)
#else
  /* '>= XFT' means XFT or Cairo */
  if (font_config->type_engine >= TYPE_XFT)
#endif
  {
    font_rcfile = aafont_file;

    switch (font_config->font_present & ~FONT_AA) {
      default:
        font_rcfile2 = NULL;
        break;

      case FONT_VAR_WIDTH:
        font_rcfile2 = vaafont_file;
        break;

      case FONT_VERTICAL:
        font_rcfile2 = taafont_file;
        break;
    }
  } else {
    font_rcfile = font_file;

    switch (font_config->font_present & ~FONT_AA) {
      default:
        font_rcfile2 = NULL;
        break;

      case FONT_VAR_WIDTH:
        font_rcfile2 = vfont_file;
        break;

      case FONT_VERTICAL:
        font_rcfile2 = tfont_file;
        break;
    }
  }

  if (!changed_font_file) {
    if ((rcpath = bl_get_sys_rc_path(font_rcfile))) {
      read_conf(font_config, rcpath);
      free(rcpath);
    }
  }

  if (!changed_font_file || changed_font_file == font_rcfile) {
    if ((rcpath = bl_get_user_rc_path(font_rcfile))) {
      read_conf(font_config, rcpath);
      free(rcpath);
    }
  }

  apply_custom_cache(font_config, font_rcfile);

  if (font_rcfile2) {
    if (!changed_font_file) {
      if ((rcpath = bl_get_sys_rc_path(font_rcfile2))) {
        read_conf(font_config, rcpath);
        free(rcpath);
      }
    }

    if ((rcpath = bl_get_user_rc_path(font_rcfile2))) {
      read_conf(font_config, rcpath);
      free(rcpath);
    }

    apply_custom_cache(font_config, font_rcfile2);
  }

  return 1;
}

static int change_custom_cache(const char *file, const char *key, const char *value) {
  void *p;
  u_int count;

  for (count = 0; count < num_of_customs; count++) {
    if (custom_cache[count].file == file && strcmp(custom_cache[count].key, key) == 0) {
      if (*value) {
        /* replace */
        char *p;

        if (strcmp(custom_cache[count].value, value) == 0) {
          /* not changed */
          return 0;
        }

        if ((p = strdup(value))) {
          free(custom_cache[count].value);
          custom_cache[count].value = p;
        }
      } else {
        /* remove */

        free(custom_cache[count].key);
        free(custom_cache[count].value);
        custom_cache[count] = custom_cache[--num_of_customs];
        if (num_of_customs == 0) {
          free(custom_cache);
          custom_cache = NULL;

#ifdef __DEBUG
          bl_debug_printf("Custom cache is completely freed.\n");
#endif
        }
      }

      return 1;
    }
  }

  /* #if 1 => Don't remove font settings read from *font files in ~/.mlterm */
#if 0
  if (*value == '\0') {
    return 0;
  }
#endif

  if ((p = realloc(custom_cache, sizeof(custom_cache_t) * (num_of_customs + 1))) == NULL) {
    return 0;
  }

  custom_cache = p;

  if ((custom_cache[num_of_customs].key = strdup(key)) == NULL) {
    return 0;
  }

  if ((custom_cache[num_of_customs].value = strdup(value)) == NULL) {
    free(custom_cache[num_of_customs].key);

    return 0;
  }

  custom_cache[num_of_customs++].file = file;

#ifdef __DEBUG
  bl_debug_printf("%s=%s is newly added to custom cache.\n", key, value);
#endif

  return 1;
}

static int write_conf(char *path, /* Can be destroyed in this function. */
                      const char *key, const char *value) {
  bl_conf_write_t *conf;

  if (!(conf = bl_conf_write_open(path))) {
    return 0;
  }

  bl_conf_io_write(conf, key, value);

  bl_conf_write_close(conf);

  return 1;
}

static int save_conf(const char *file, const char *key,
                     char *value /* Includes multiple entries. Destroyed in this function. */
                     ) {
  char *path;
  int ret;

  if ((path = bl_get_user_rc_path(file)) == NULL) {
    return 0;
  }

  if ((ret = write_conf(path, key, value))) {
    /* Remove from custom_cache */
    change_custom_cache(file, key, "");
  }

  free(path);

  return ret;
}

static ui_font_config_t *find_font_config(ui_type_engine_t type_engine,
                                          ui_font_present_t font_present) {
  if (font_configs) {
    u_int count;

    for (count = 0; count < num_of_configs; count++) {
      if (font_configs[count]->font_present == font_present &&
          font_configs[count]->type_engine == type_engine) {
        return font_configs[count];
      }
    }
  }

  return NULL;
}

static u_int match_font_configs(ui_font_config_t **matched_configs,
                                u_int max_size, /* must be over 0. */
                                int is_xcore, ui_font_present_t present_mask) {
  u_int count;
  u_int size;

  size = 0;
  for (count = 0; count < num_of_configs; count++) {
    if (
#if !defined(USE_FREETYPE) || !defined(USE_FONTCONFIG)
        (is_xcore ? font_configs[count]->type_engine == TYPE_XCORE :
                    /* '>= XFT' means XFT or Cairo */
                    font_configs[count]->type_engine >= TYPE_XFT) &&
#endif
        (present_mask ? (font_configs[count]->font_present & present_mask) : 1)) {
      matched_configs[size++] = font_configs[count];
      if (size >= max_size) {
        break;
      }
    }
  }

  return size;
}

static ui_font_config_t *create_shared_font_config(ui_type_engine_t type_engine,
                                                   ui_font_present_t font_present) {
  u_int count;

  for (count = 0; count < num_of_configs; count++) {
    if ((type_engine == TYPE_XCORE ? font_configs[count]->type_engine == TYPE_XCORE :
                                   /* '>= XFT' means XFT or Cairo */
             font_configs[count]->type_engine >= TYPE_XFT) &&
        ((font_configs[count]->font_present & ~FONT_AA) == (font_present & ~FONT_AA))) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Found sharable font_config.\n");
#endif

      ui_font_config_t *font_config;

      if ((font_config = malloc(sizeof(ui_font_config_t))) == NULL) {
        return NULL;
      }

      font_config->type_engine = type_engine;
      font_config->font_present = font_present;
      font_config->font_name_table = font_configs[count]->font_name_table;
      font_config->ref_count = 0;

      return font_config;
    }
  }

  return NULL;
}

/* --- global functions --- */

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
int ui_use_aafont(void) {
  if (num_of_configs > 0 || use_aafont) {
    return 0;
  }

  use_aafont = 1;
  ui_font_use_fontconfig();

  return 1;
}

int ui_is_using_aafont(void) { return use_aafont; }
#endif

ui_font_config_t *ui_acquire_font_config(ui_type_engine_t type_engine,
                                         ui_font_present_t font_present) {
  ui_font_config_t *font_config;
  void *p;

  BL_TESTIT_ONCE(font_config, ());

  if ((font_config = find_font_config(type_engine, font_present))) {
    font_config->ref_count++;

    return font_config;
  }

  if ((p = realloc(font_configs, sizeof(ui_font_config_t *) * (num_of_configs + 1))) == NULL) {
    return NULL;
  }

  font_configs = p;

  if ((font_config = create_shared_font_config(type_engine, font_present)) == NULL) {
    if ((font_config = ui_font_config_new(type_engine, font_present)) == NULL ||
        !read_all_conf(font_config, NULL)) {
      return NULL;
    }
  }

  font_config->ref_count++;

  return font_configs[num_of_configs++] = font_config;
}

int ui_release_font_config(ui_font_config_t *font_config) {
  u_int count;
  int has_share;
  int found;

  if (--font_config->ref_count > 0) {
    return 1;
  }

  has_share = 0;
  found = 0;
  count = 0;
  while (count < num_of_configs) {
    if (font_configs[count] == font_config) {
      font_configs[count] = font_configs[--num_of_configs];
      found = 1;

      continue;
    } else if ((font_config->type_engine == TYPE_XCORE
                    ? font_configs[count]->type_engine == TYPE_XCORE
                    :
                    /* '>= XFT' means XFT or Cairo */
                    font_configs[count]->type_engine >= TYPE_XFT) &&
               ((font_configs[count]->font_present & ~FONT_AA) ==
                (font_config->font_present & ~FONT_AA))) {
      has_share = 1;
    }

    count++;
  }

  if (!found) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " font_config is not found in font_configs.\n");
#endif

    return 0;
  }

  if (has_share /* && num_of_configs > 0 */) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Sharable font_config exists.\n");
#endif

    free(font_config);

    return 1;
  }

  ui_font_config_delete(font_config);

  if (num_of_configs == 0) {
    free(font_configs);
    font_configs = NULL;
  }

  return 1;
}

ui_font_config_t *ui_font_config_new(ui_type_engine_t type_engine, ui_font_present_t font_present) {
  ui_font_config_t *font_config;

  if ((font_config = malloc(sizeof(ui_font_config_t))) == NULL) {
    return NULL;
  }

  bl_map_new_with_size(vt_font_t, char *, font_config->font_name_table, bl_map_hash_int,
                       bl_map_compare_int, 16);

  font_config->type_engine = type_engine;
  font_config->font_present = font_present;
  font_config->ref_count = 0;

  return font_config;
}

int ui_font_config_delete(ui_font_config_t *font_config) {
  int count;
  u_int size;
  BL_PAIR(ui_font_name) * fn_array;

  fn_array = get_font_name_pairs_array(&size, font_config->font_name_table);

  for (count = 0; count < size; count++) {
    free(fn_array[count]->value);
  }

  bl_map_delete(font_config->font_name_table);

  free(font_config);

  return 1;
}

/*
 * 0 => customization is failed.
 * -1 => customization is succeeded but saving is failed.
 * 1 => succeeded.
 */
int ui_customize_font_file(const char *file, /* if null, use "mlterm/font" file. */
                           char *key,        /* charset name */
                           char *value,      /* font list */
                           int save) {
  /*
   * Max number of target font_config is 6.
   * [file == aafont_file]
   * TYPE_XFT, TYPE_XFT & FONT_VAR_WIDTH , TYPE_XFT & FONT_VERTICAL , TYPE_XFT &
   * FONT_AA ,
   * TYPE_XFT & FONT_VAR_WIDTH & FONT_AA , TYPE_XFT & FONT_VERTICAL & FONT_AA
   */
  ui_font_config_t *targets[6];
  u_int num_of_targets;
  u_int count;

  if (file == NULL ||
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
      strcmp(file, "font") == 0
#else
      strcmp(file, FONT_FILE) == 0
#endif
      ) {
    file = font_file;
    num_of_targets = match_font_configs(targets, 6, /* is xcore */ 1, 0);
  } else if (strcmp(file, aafont_file + 7) == 0) {
    file = aafont_file;
    num_of_targets = match_font_configs(targets, 6, /* is not xcore */ 0, 0);
  } else if (strcmp(file, VFONT_FILE) == 0) {
    file = vfont_file;
    num_of_targets = match_font_configs(targets, 6, /* is xcore */ 1, FONT_VAR_WIDTH);
  } else if (strcmp(file, TFONT_FILE) == 0) {
    file = tfont_file;
    num_of_targets = match_font_configs(targets, 6, /* is xcore */ 1, FONT_VERTICAL);
  } else if (strcmp(file, vaafont_file + 7) == 0) {
    file = vaafont_file;
    num_of_targets = match_font_configs(targets, 6, /* is not xcore */ 0, FONT_VAR_WIDTH);
  } else if (strcmp(file, taafont_file + 7) == 0) {
    file = taafont_file;
    num_of_targets = match_font_configs(targets, 6, /* is not xcore */ 0, FONT_VERTICAL);
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " font file %s is not found.\n", file);
#endif

    return 0;
  }

#ifdef __DEBUG
  if (num_of_targets) {
    bl_debug_printf("customize font file %s %s %s\n", file, key, value);
  } else {
    bl_debug_printf("customize font file %s %s %s(not changed in run time)\n", file, key, value);
  }
#endif

  if (save) {
    int ret;

    if (change_custom_cache(file, key, value)) {
      ret = 1;
      for (count = 0; count < num_of_targets; count++) {
        read_all_conf(targets[count], file);
      }
    } else {
      ret = 0;
    }

    if (!save_conf(file, key, value)) {
      return ret ? -1 : 0;
    } else {
      return ret;
    }
  } else {
    if (!change_custom_cache(file, key, value)) {
      return 0;
    }

    for (count = 0; count < num_of_targets; count++) {
      read_all_conf(targets[count], file);
    }
  }

  return 1;
}

char *ui_get_config_font_name(ui_font_config_t *font_config, u_int font_size, vt_font_t font) {
  vt_font_t cand_font;
  BL_PAIR(ui_font_name) pair;
  char *font_name;
  char *encoding;
  size_t encoding_len;
  int has_percentd;
#ifdef USE_XLIB
  static char *orig_style[] = {"-medium-", "-r-", "-medium-r-"};
  static char *new_style[] = {"-bold-", "-i-", "-bold-i-"};
#endif

  if (HAS_UNICODE_AREA(font)) {
    font &= ~FONT_FULLWIDTH;
  }

  encoding = NULL;
  cand_font = NO_SIZE_ATTR(font);

  while (!(pair = get_font_name_pair(font_config->font_name_table, cand_font))) {
#ifdef USE_XLIB
    int idx;

    if (font_config->type_engine == TYPE_XCORE) {
      if ((idx = FONT_STYLE_INDEX(font)) >= 0 &&
          ((pair = get_font_name_pair(font_config->font_name_table, FONT_CS(cand_font))) &&
           (font_name = bl_str_replace(pair->value, orig_style[idx], new_style[idx])))) {
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG "Set font %s for %x\n", font_name, font);
#endif

        set_font_name_to_table(font_config->font_name_table, cand_font, font_name);

        continue;
      }
    } else
#endif
    {
      if (cand_font & FONT_STYLES) {
        cand_font &= ~FONT_STYLES;

        continue;
      }
    }

    if (cand_font & FONT_FULLWIDTH) {
      cand_font &= ~FONT_FULLWIDTH;
    } else if (FONT_CS(cand_font) != DEFAULT_FONT) {
      cand_font = DEFAULT_FONT;
    } else {
      return NULL;
    }
  }

#ifdef USE_XLIB
  if (font_config->type_engine == TYPE_XCORE && FONT_CS(cand_font) == DEFAULT_FONT &&
      /* encoding is appended if font_name is XLFD (not alias name). */
      (strchr(pair->value, '*') || strchr(pair->value, '-'))) {
    char **names;

    if ((names = ui_font_get_encoding_names(FONT_CS(font))) && names[0]) {
      encoding = names[0];
    }
  }
#endif

#if 1
  if (*(pair->value) == '&' &&
      /* XXX font variable is overwritten. */
      (font = parse_key(pair->value + 1)) != UNKNOWN_CS) {
    /*
     * XXX (Undocumented)
     *
     * JISX0213_2000_1 = &JISX0208_1983 in font configuration files.
     * => try to get a font name of JISX0208_1983 instead of
     *    JISX0213_2000_1 recursively.
     */
    return ui_get_config_font_name(font_config, font_size, font);
  }
#endif

  /*
   * If pair->value is valid format or not is checked by is_valid_font_format()
   * in customize_font_name().
   * So all you have to do here is strchr( ... , '%') alone.
   */
  if (strchr(pair->value, '%')) {
    has_percentd = 1;
  } else if (encoding == NULL) {
    return strdup(pair->value);
  } else {
    has_percentd = 0;
  }

  if (!(font_name = malloc(strlen(pair->value) +
                           /* -2 is for "%d" */
                           (has_percentd ? DIGIT_STR_LEN(font_size) - 2 : 0) +
                           (encoding_len = encoding ? strlen(encoding) : 0) + 1))) {
    return NULL;
  }

  if (has_percentd) {
    sprintf(font_name, pair->value, font_size);
  } else {
    strcpy(font_name, pair->value);
  }

  if (encoding) {
    char *percent;

    if ((percent = strchr(font_name, ':'))) {
      /* -*-:200 -> -*-iso8859-1:200 */

      memmove(percent + encoding_len, percent, strlen(percent) + 1);
      memcpy(percent, encoding, encoding_len);
    } else {
      strcat(font_name, encoding);
    }
  }

  return font_name;
}

char *ui_get_config_font_name2(const char *file, /* can be NULL */
                               u_int font_size, char *font_cs) {
  vt_font_t font;
  ui_font_config_t *font_config;
  ui_type_engine_t engine;
  ui_font_present_t present;
  char *font_name;

  if (file == NULL || strcmp(file, FONT_FILE) == 0) {
    engine = TYPE_XCORE;
    present = 0;
  }
#if !defined(USE_FREETYPE) || !defined(USE_FONTCONFIG)
  else if (strcmp(file, aafont_file + 7) == 0) {
    engine = TYPE_XFT;

    /*
     * font_config::font_name_table is shared with
     * font_configs whose difference is only FONT_AA.
     */
    present = 0;
  } else if (strcmp(file, vaafont_file + 7) == 0) {
    engine = TYPE_XFT;
    present = FONT_VAR_WIDTH;
  } else if (strcmp(file, taafont_file + 7) == 0) {
    engine = TYPE_XFT;
    present = FONT_VERTICAL;
  }
#endif
  else if (strcmp(file, VFONT_FILE) == 0) {
    engine = TYPE_XCORE;
    present = FONT_VAR_WIDTH;
  } else if (strcmp(file, TFONT_FILE) == 0) {
    engine = TYPE_XCORE;
    present = FONT_VERTICAL;
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " font file %s is not found.\n", file);
#endif

    return NULL;
  }

  if ((font_config = ui_acquire_font_config(engine, present)) == NULL) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " ui_font_config_t is not found.\n");
#endif

    return NULL;
  }

  if ((font = parse_key(font_cs)) == UNKNOWN_CS) {
    return NULL;
  }

  font_name = ui_get_config_font_name(font_config, font_size, font);

  ui_release_font_config(font_config);

  return font_name;
}

char *ui_get_all_config_font_names(ui_font_config_t *font_config, u_int font_size) {
  BL_PAIR(ui_font_name) * array;
  u_int size;
  char *font_name_list;
  size_t list_len;
  char *p;
  u_int count;

  array = get_font_name_pairs_array(&size, font_config->font_name_table);

  if (size == 0) {
    return NULL;
  }

  list_len = 0;

  for (count = 0; count < size; count++) {
    list_len += (strlen(array[count]->value) - 2 + DIGIT_STR_LEN(font_size) + 1);
  }

  if ((font_name_list = malloc(list_len)) == NULL) {
    return NULL;
  }

  p = font_name_list;

  for (count = 0; count < size; count++) {
    /*
     * XXX
     * Ignore DEFAULT_FONT setting because it doesn't have encoding name.
     */
    if (FONT_CS(array[count]->key) != DEFAULT_FONT) {
      sprintf(p, array[count]->value, font_size);
      p += strlen(p);
      *(p++) = ',';
    }
  }

  if (p > font_name_list) {
    --p;
  }
  *p = '\0';

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Font list is %s\n", font_name_list);
#endif

  return font_name_list;
}

char *ui_get_charset_name(ef_charset_t cs) {
  int count;

  for (count = 0; count < sizeof(cs_table) / sizeof(cs_table[0]); count++) {
    if (cs_table[count].cs == cs) {
      return cs_table[count].name;
    }
  }

  return NULL;
}

#ifdef BL_DEBUG

#include <assert.h>

static void TEST_font_config(void) {
#ifdef USE_XLIB
  ui_font_config_t *font_config;
  char *value;

  font_config = ui_font_config_new(TYPE_XCORE, 0);
  customize_font_name(font_config, ISO8859_1_R, "-hoge-medium-r-fuga-");

  value = ui_get_config_font_name(font_config, 12, ISO8859_1_R | FONT_BOLD);
  assert(strcmp("-hoge-bold-r-fuga-", value) == 0);
  free(value);

  value = ui_get_config_font_name(font_config, 12, ISO8859_1_R | FONT_ITALIC);
  assert(strcmp("-hoge-medium-i-fuga-", value) == 0);
  free(value);

  value = ui_get_config_font_name(font_config, 12, ISO8859_1_R | FONT_BOLD | FONT_ITALIC);
  assert(strcmp("-hoge-bold-i-fuga-", value) == 0);
  free(value);

  ui_font_config_delete(font_config);
#endif
}

#endif
