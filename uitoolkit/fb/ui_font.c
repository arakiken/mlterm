/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_font.h"

#include <stdio.h>
#include <fcntl.h>    /* open */
#include <unistd.h>   /* close */
#include <string.h>   /* memcmp */
#include <sys/stat.h> /* fstat */
#include <utime.h>    /* utime */

#include <pobl/bl_def.h> /* WORDS_BIGENDIAN */
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* strdup */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_conf_io.h>/* bl_get_user_rc_path */
#include <pobl/bl_util.h>   /* TOINT32 */
#include <mef/ef_char.h>
#ifdef __ANDROID__
#include <dirent.h>
#endif

#ifdef USE_WIN32API
#define mmap(a, b, c, d, e, f) (NULL)
#define munmap(a, b) (0)
#else
#include <sys/mman.h> /* mmap */
#endif

#ifdef USE_OT_LAYOUT
#include <otl.h>
#endif

#include "ui_decsp_font.h"

#define DIVIDE_ROUNDING(a, b) (((int)((a)*10 + (b)*5)) / ((int)((b)*10)))
#define DIVIDE_ROUNDINGUP(a, b) (((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)))
#define DIVIDE_ROUNDINGDOWN(a, b) (((int)(a)*10) / ((int)((b)*10)))

#ifdef WORDS_BIGENDIAN
#define _TOINT32(p, is_be) ((is_be) ? TOINT32(p) : LE32DEC(p))
#define _TOINT16(p, is_be) ((is_be) ? TOINT16(p) : LE16DEC(p))
#else
#define _TOINT32(p, is_be) ((is_be) ? BE32DEC(p) : TOINT32(p))
#define _TOINT16(p, is_be) ((is_be) ? BE16DEC(p) : TOINT16(p))
#endif

#define PCF_PROPERTIES (1 << 0)
#define PCF_ACCELERATORS (1 << 1)
#define PCF_METRICS (1 << 2)
#define PCF_BITMAPS (1 << 3)
#define PCF_INK_METRICS (1 << 4)
#define PCF_BDF_ENCODINGS (1 << 5)
#define PCF_SWIDTHS (1 << 6)
#define PCF_GLYPH_NAMES (1 << 7)
#define PCF_BDF_ACCELERATORS (1 << 8)

#define MAX_GLYPH_TABLES 512
#define GLYPH_TABLE_SIZE 128
#define INITIAL_GLYPH_INDEX_TABLE_SIZE 0x1000

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static XFontStruct **xfonts;
static u_int num_xfonts;

static XFontStruct *get_cached_xfont(const char *file, int32_t format);
static int add_xfont_to_cache(XFontStruct *xfont);
static void xfont_unref(XFontStruct *xfont);

/* ===== PCF ===== */

/* --- static functions --- */

static int load_bitmaps(XFontStruct *xfont, u_char *p, size_t size, int is_be, int glyph_pad_type) {
  int32_t *offsets;
  int32_t bitmap_sizes[4];
  int32_t count;

  /* 0 -> byte , 1 -> short , 2 -> int */
  xfont->glyph_width_bytes = (glyph_pad_type == 2 ? 4 : (glyph_pad_type == 1 ? 2 : 1));

  xfont->num_glyphs = _TOINT32(p, is_be);
  p += 4;

  if (size < 8 + sizeof(*offsets) * xfont->num_glyphs) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " size %d is too small.\n", size);
#endif

    return 0;
  }

  if (!(xfont->glyph_offsets = malloc(sizeof(*offsets) * xfont->num_glyphs))) {
    return 0;
  }

#ifdef WORDS_BIGENDIAN
  if (is_be)
#else
  if (!is_be)
#endif
  {
    memcpy(xfont->glyph_offsets, p, sizeof(*offsets) * xfont->num_glyphs);
    p += (sizeof(*offsets) * xfont->num_glyphs);
  } else {
    for (count = 0; count < xfont->num_glyphs; count++) {
      xfont->glyph_offsets[count] = _TOINT32(p, is_be);
      p += 4;
    }
  }

  for (count = 0; count < 4; count++) {
    bitmap_sizes[count] = _TOINT32(p, is_be);
    p += 4;
  }

  if (size < 8 + sizeof(*offsets) * xfont->num_glyphs + 16 + bitmap_sizes[glyph_pad_type]) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " size %d is too small.\n", size);
#endif

    return 0;
  }

  if (!(xfont->glyphs = malloc(bitmap_sizes[glyph_pad_type]))) {
    return 0;
  }

  if (is_be) {
    /* Regard the bit order of p as msb first */
    memcpy(xfont->glyphs, p, bitmap_sizes[glyph_pad_type]);
  } else {
    /* Regard the bit order of p as lsb first. Reorder it to msb first. */
    for (count = 0; count < bitmap_sizes[glyph_pad_type]; count++) {
      xfont->glyphs[count] = ((p[count] << 7) & 0x80) | ((p[count] << 5) & 0x40) |
                             ((p[count] << 3) & 0x20) | ((p[count] << 1) & 0x10) |
                             ((p[count] >> 1) & 0x08) | ((p[count] >> 3) & 0x04) |
                             ((p[count] >> 5) & 0x02) | ((p[count] >> 7) & 0x01);
    }
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG "GLYPH COUNT %d x WIDTH BYTE %d = SIZE %d\n", xfont->num_glyphs,
                  xfont->glyph_width_bytes, bitmap_sizes[glyph_pad_type]);

  {
    FILE* fp;

    p = xfont->glyphs;

    fp = fopen("log.txt", "w");

    for (count = 0; count < xfont->num_glyphs; count++) {
      fprintf(fp, "NUM %x\n", count);
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n", _TOINT32(p, is_be));
      p += 4;
      fprintf(fp, "%x\n\n", _TOINT32(p, is_be));
      p += 4;
    }

    fclose(fp);
  }
#endif

  return 1;
}

static int load_encodings(XFontStruct *xfont, u_char *p, size_t size, int is_be) {
  size_t idx_size;

  xfont->min_char_or_byte2 = _TOINT16(p, is_be);
  p += 2;
  xfont->max_char_or_byte2 = _TOINT16(p, is_be);
  p += 2;
  xfont->min_byte1 = _TOINT16(p, is_be);
  p += 2;
  xfont->max_byte1 = _TOINT16(p, is_be);
  p += 2;

  /* skip default_char */
  p += 2;

  idx_size = (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) *
             (xfont->max_byte1 - xfont->min_byte1 + 1) * sizeof(int16_t);

  if (size < 14 + idx_size) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " size %d is too small.\n", size);
#endif

    return 0;
  }

  if (!(xfont->glyph_indeces = malloc(idx_size))) {
    return 0;
  }

#ifdef WORDS_BIGENDIAN
  if (is_be)
#else
  if (!is_be)
#endif
  {
    memcpy(xfont->glyph_indeces, p, idx_size);
  } else {
    size_t count;

    for (count = 0; count < (idx_size / sizeof(int16_t)); count++) {
      xfont->glyph_indeces[count] = _TOINT16(p, is_be);
      p += 2;
    }
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG "GLYPH INDEX %d %d %d %d\n", xfont->min_char_or_byte2,
                  xfont->max_char_or_byte2, xfont->min_byte1, xfont->max_byte1);

  {
    int count;
    int16_t *p;

    p = xfont->glyph_indeces;

    for (count = xfont->min_char_or_byte2; count <= xfont->max_char_or_byte2; count++) {
      bl_msg_printf("%d %x\n", count, (int)*p);
      p++;
    }
  }
#endif

  return 1;
}

static int get_metrics(u_int8_t *width, u_int8_t *width_full, u_int8_t *height, u_int8_t *ascent,
                       u_char *p, size_t size, int is_be, int is_compressed) {
  int16_t num_metrics;

  /* XXX Proportional font is not considered. */

  if (is_compressed) {
    num_metrics = _TOINT16(p, is_be);
    p += 2;

    *width = p[2] - 0x80;
    *ascent = p[3] - 0x80;
    *height = *ascent + (p[4] - 0x80);

    if (num_metrics > 0x3000) {
      /* U+3000: Unicode ideographic space (Full width) */
      p += (5 * 0x3000);
      *width_full = p[2] - 0x80;
    } else {
      *width_full = *width;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " COMPRESSED METRICS %d %d %d %d %d\n", num_metrics, *width,
                    *width_full, *height, *ascent);
#endif
  } else {
    num_metrics = _TOINT32(p, is_be);
    p += 4;

    /* skip {left|right}_sided_bearing */
    p += 4;

    *width = _TOINT16(p, is_be);
    p += 2;

    *ascent = _TOINT16(p, is_be);
    p += 2;

    *height = *ascent + _TOINT16(p, is_be);

    if (num_metrics > 0x3000) {
      /* skip character_descent and character attributes */
      p += 4;

      /* U+3000: Unicode ideographic space (Full width) */
      p += (12 * 0x2999);

      /* skip {left|right}_sided_bearing */
      p += 4;

      *width_full = _TOINT16(p, is_be);
    } else {
      *width_full = *width;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " NOT COMPRESSED METRICS %d %d %d %d %d\n", num_metrics, *width,
                    *width_full, *height, *ascent);
#endif
  }

  return 1;
}

static char *gunzip(const char *file_path, struct stat *st) {
  size_t len;
  char *new_file_path;
  struct stat new_st;
  char *cmd;
  struct utimbuf ut;

  if (stat(file_path, st) == -1) {
    return NULL;
  }

  if ((len = strlen(file_path)) <= 3 || strcmp(file_path + len - 3, ".gz") != 0) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " USE UNCOMPRESSED FONT\n");
#endif

    return strdup(file_path);
  }

  if (!(new_file_path = alloca(7 + len + 1))) {
    goto error;
  }

  sprintf(new_file_path, "mlterm/%s", bl_basename(file_path));
  new_file_path[strlen(new_file_path) - 3] = '\0'; /* remove ".gz" */

  if (!(new_file_path = bl_get_user_rc_path(new_file_path))) {
    goto error;
  }

  if (stat(new_file_path, &new_st) == 0) {
    if (st->st_mtime <= new_st.st_mtime) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " USE CACHED UNCOMPRESSED FONT.\n");
#endif

      *st = new_st;

      return new_file_path;
    }
  }

  if (!(cmd = alloca(10 + len + 3 + strlen(new_file_path) + 1))) {
    goto error;
  }

  sprintf(cmd, "gunzip -c %s > %s", file_path, new_file_path);

  /*
   * The returned value is not checked because -1 with errno=ECHILD may be
   * returned even if cmd is executed successfully.
   */
  system(cmd);

  /* st->st_size can be 0 if file_path points an illegally gzipped file. */
  if (stat(new_file_path, st) == -1 || st->st_size <= 8) {
    unlink(new_file_path);

    goto error;
  }

  /*
   * The atime and mtime of the uncompressed pcf font is the same
   * as those of the original gzipped font.
   */
  ut.actime = st->st_atime;
  ut.modtime = st->st_mtime;
  utime(new_file_path, &ut);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " USE NEWLY UNCOMPRESSED FONT\n");
#endif

  return new_file_path;

error:
#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Failed to gunzip %s.\n", file_path);
#endif

  free(new_file_path);

  return NULL;
}

static int load_pcf(XFontStruct *xfont, const char *file_path) {
  char *uzfile_path;
  int fd;
  struct stat st;
  u_char *pcf = NULL;
  u_char *p;
  int32_t num_tables;
  int table_load_count;
  int32_t count;

  if (!(uzfile_path = gunzip(file_path, &st))) {
    return 0;
  }

  fd = open(uzfile_path, O_RDONLY);
  free(uzfile_path);

  if (fd == -1) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Failed to open %s.", xfont->file);
#endif

    return 0;
  }

  if (!(xfont->file = strdup(file_path))) {
    close(fd);

    return 0;
  }

  table_load_count = 0;

  /* "st.st_size > 8" is ensured. (see gunzip()) */
  if (!(p = pcf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) ||
      memcmp(p, "\1fcp", 4) != 0) {
    goto end;
  }

  p += 4;

  num_tables = _TOINT32(p, 0);
  p += 4;

  if (st.st_size <= 8 + 16 * num_tables) {
    goto end;
  }

  for (count = 0; count < num_tables; count++) {
    int32_t type;
    int32_t format;
    int32_t size;
    int32_t offset;

    type = _TOINT32(p, 0);
    p += 4;
    format = _TOINT32(p, 0);
    p += 4;
    size = _TOINT32(p, 0);
    p += 4;
    offset = _TOINT32(p, 0);
    p += 4;

    if (/* (format & 8) != 0 || */      /* MSBit first */
            ((format >> 4) & 3) != 0 || /* the bits aren't stored in
                                            bytes(0) but in short(1) or int(2). */
        offset + size > st.st_size ||
        format != _TOINT32(pcf + offset, 0)) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " %s is unsupported pcf format.\n", xfont->file);
#endif
    } else if (type == PCF_BITMAPS) {
      if (!load_bitmaps(xfont, pcf + offset + 4, size, format & 4, format & 3)) {
        goto end;
      }

      table_load_count++;
    } else if (type == PCF_BDF_ENCODINGS) {
      if (!load_encodings(xfont, pcf + offset + 4, size, format & 4)) {
        goto end;
      }

      table_load_count++;
    } else if (type == PCF_METRICS) {
      if (!get_metrics(&xfont->width, &xfont->width_full, &xfont->height, &xfont->ascent,
                       pcf + offset + 4, size, format & 4, format & 0x100)) {
        goto end;
      }

      table_load_count++;
    }
  }

#ifdef __DEBUG
  {
#if 1
    u_char ch[] = "\x97\xf3";
#elif 0
    u_char ch[] = "a";
#else
    u_char ch[] = "\x06\x22"; /* UCS2 */
#endif
    u_char *bitmap;
    int i;
    int j;

    if ((bitmap = ui_get_bitmap(xfont, ch, sizeof(ch) - 1, 0, NULL))) {
      for (j = 0; j < xfont->height; j++) {
        u_char *line;

        ui_get_bitmap_line(xfont, bitmap, j * xfont->glyph_width_bytes, line);

        for (i = 0; i < xfont->width; i++) {
          bl_msg_printf("%d", (line && ui_get_bitmap_cell(line, i)) ? 1 : 0);
        }
        bl_msg_printf("\n");
      }
    }
  }
#endif

end:
  close(fd);

  if (pcf) {
    munmap(pcf, st.st_size);
  }

  if (table_load_count == 3) {
    return 1;
  }

  return 0;
}

static void unload_pcf(XFontStruct *xfont) {
  free(xfont->file);
  free(xfont->glyphs);
  free(xfont->glyph_offsets);
  free(xfont->glyph_indeces);
}

#ifdef USE_FREETYPE
static int is_pcf(const char *file_path) {
  return (strcasecmp(file_path + strlen(file_path) - 6, "pcf.gz") == 0 ||
          strcasecmp(file_path + strlen(file_path) - 3, "pcf") == 0);
}
#else
#define is_pcf(path) (1)
#endif

/* ===== FREETYPE ===== */

#ifdef USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#ifdef FT_LCD_FILTER_H
#include FT_LCD_FILTER_H
#endif
#include FT_OUTLINE_H

/* 0 - 511 */
#define SEG(idx) (((idx) >> 7) & 0x1ff)
/* 0 - 127 */
#define OFF(idx) ((idx) & 0x7f)
/* +3 is for storing glyph position info. */
#define HAS_POSITION_INFO_BY_GLYPH(xfont) \
  ((xfont)->glyph_size == (xfont)->glyph_width_bytes * (xfont)->height + 3)
#define FONT_ROTATED (FONT_ITALIC << 1)

/* --- static variables --- */

static FT_Library library;

/* --- static functions --- */

static int get_glyph_index(FT_Face face, u_int32_t code) {
  int idx;

  if ((idx = FT_Get_Char_Index(face, code)) == 0) {
    /* XXX Some glyph indeces of ISCII fonts becomes 0 wrongly. */
    if (0x80 <= code && code <= 0xff) {
      u_int32_t prev_idx;
      u_int32_t next_idx;
      u_int32_t c;

      for (c = code + 1; c <= 0xff; c++) {
        if ((next_idx = FT_Get_Char_Index(face, c)) > 0) {
          for (c = code - 1; c >= 80; c--) {
            if ((prev_idx = FT_Get_Char_Index(face, c)) > 0) {
              if (prev_idx + 1 < next_idx) {
                idx = prev_idx + 1;
                break;
              }
            }
          }

          break;
        }
      }
    }
  }

  return idx;
}

static int load_glyph(FT_Face face, int32_t format, u_int32_t code, int is_aa) {
  FT_Glyph_Format orig_glyph_format;
  if (is_aa) {
    FT_Load_Glyph(face, code, FT_LOAD_NO_BITMAP);

    if (face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
      return 0;
    }
  } else {
    FT_Load_Glyph(face, code, 0);
  }

  orig_glyph_format = face->glyph->format;

  if (format & FONT_ROTATED) {
    FT_Matrix matrix;

    matrix.xx = 0;
    matrix.xy = 0x10000L;  /* (FT_Fixed)(-sin((-90.0 / 360.0) * 3.141592 * 2) * 0x10000L */
    matrix.yx = -0x10000L; /* (FT_Fixed)(sin((-90.0 / 360.0) * 3.141592 * 2) * 0x10000L */
    matrix.yy = 0;

    FT_Outline_Transform(&face->glyph->outline, &matrix);
  } else if (format & FONT_ITALIC) {
    FT_Matrix matrix;

    matrix.xx = 1 << 16;
    matrix.xy = 0x3000;
    matrix.yx = 0;
    matrix.yy = 1 << 16;

    FT_Outline_Transform(&face->glyph->outline, &matrix);
  }

  if (format & FONT_BOLD) {
    FT_Outline_Embolden(&face->glyph->outline, 1 << 5);
  }

  if (is_aa) {
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD);
  } else {
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
  }

  /*
   * get_ft_bitmap_intern() uses face->glyph->format to check if glyph is actually rotated.
   * (face->glyph->format == FT_GLYPH_FORMAT_BITMAP just after FT_Load_Glyph() means that
   * glyphs is not rotated.)
   *
   * Note that FT_Render_Glyph() turns face->glyph->format from FT_GLYPH_FORMAT_OUTLINE
   * to FT_GLYPH_FORMAT_BITMAP.
   */
  face->glyph->format = orig_glyph_format;

  return 1;
}

static int load_ft(XFontStruct *xfont, const char *file_path, int32_t format, int is_aa,
                   u_int force_height) {
  u_int count;
  FT_Face face;
  u_int fontsize;

  if (!library) {
    if (FT_Init_FreeType(&library)) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG "FT_Init_FreeType() failed.\n");
#endif

      return 0;
    }

#ifdef FT_LCD_FILTER_H
    FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);
#endif
  }

  fontsize = (format & ~(FONT_BOLD | FONT_ITALIC | FONT_ROTATED));

  for (count = 0; count < num_xfonts; count++) {
    if (strcmp(xfonts[count]->file, file_path) == 0 &&
        /* The same face is used for normal, italic and bold. */
        (xfonts[count]->format & ~(FONT_BOLD | FONT_ITALIC | FONT_ROTATED)) == fontsize) {
      face = xfonts[count]->face;

      goto face_found;
    }
  }

  if (FT_New_Face(library, file_path, 0, &face)) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG "FT_New_Face() failed.\n");
#endif

    return 0;
  }

face_found:
  if (face->units_per_EM == 0 /* units_per_EM can be 0 ("Noto Color Emoji") */) {
    goto error;
  }

  FT_Set_Pixel_Sizes(face, fontsize, fontsize);

  if (!load_glyph(face, format, get_glyph_index(face, 'M'), is_aa)) {
    bl_msg_printf("%s doesn't have outline glyphs.\n", file_path);

    goto error;
  }

  xfont->format = format;
  xfont->face = face;
  xfont->is_aa = is_aa;
  xfont->num_indeces = INITIAL_GLYPH_INDEX_TABLE_SIZE;

  if (!(xfont->file = strdup(file_path)) ||
      !(xfont->glyph_indeces = calloc(xfont->num_indeces, sizeof(u_int16_t))) ||
      !(xfont->glyphs = calloc(MAX_GLYPH_TABLES, sizeof(u_char*)))) {
    goto error;
  }

  face->generic.data = ((int)face->generic.data) + 1; /* ref_count */

  if (force_height) {
    xfont->height = force_height;
  } else {
    xfont->height = (face->max_advance_height * face->size->metrics.y_ppem +
                     face->units_per_EM - 1) / face->units_per_EM;
#ifdef __DEBUG
    bl_debug_printf("maxh %d ppem %d units %d => h %d\n",
                    face->max_advance_height, face->size->metrics.y_ppem,
                    face->units_per_EM, xfont->height);
#endif
  }

  if (format & FONT_ROTATED) {
    xfont->width = xfont->width_full = xfont->height;
    xfont->ascent = 0;
  } else {
    xfont->width_full = (face->max_advance_width * face->size->metrics.x_ppem +
                         face->units_per_EM - 1) / face->units_per_EM;
#ifdef __DEBUG
    bl_debug_printf("maxw %d ppem %d units %d => w %d\n",
                    face->max_advance_width, face->size->metrics.x_ppem,
                    face->units_per_EM, xfont->width_full);
#endif

    if (is_aa) {
      xfont->width = face->glyph->bitmap.width / 3;
    } else {
      xfont->width = face->glyph->bitmap.width;
    }

    xfont->ascent =
      (face->ascender * face->size->metrics.y_ppem + face->units_per_EM - 1) / face->units_per_EM;

    if (load_glyph(face, format, get_glyph_index(face, 'j'), is_aa)) {
      int descent = face->glyph->bitmap.rows - face->glyph->bitmap_top;
      if (descent > xfont->height - xfont->ascent) {
        xfont->height = xfont->ascent + descent;
      }
    }
  }

  if (is_aa) {
    xfont->glyph_width_bytes = xfont->width_full * 3;
  } else {
    xfont->glyph_width_bytes = (xfont->width_full + 7) / 8;
  }
  xfont->glyph_size = xfont->glyph_width_bytes * xfont->height;

#if 0
  bl_debug_printf("w %d %d h %d a %d\n", xfont->width, xfont->width_full, xfont->height,
                  xfont->ascent);
#endif

  return 1;

error:
  FT_Done_Face(face);
  free(xfont->file);
  free(xfont->glyph_indeces);

  return 0;
}

static void init_iscii_ft(FT_Face face) {
  int count;

  for (count = 0; count < face->num_charmaps; count++) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " ISCII font encoding %c%c%c%c\n",
                    ((face->charmaps[count]->encoding) >> 24) & 0xff,
                    ((face->charmaps[count]->encoding) >> 16) & 0xff,
                    ((face->charmaps[count]->encoding) >> 8) & 0xff,
                    (face->charmaps[count]->encoding & 0xff));
#endif

    if (face->charmaps[count]->encoding == FT_ENCODING_APPLE_ROMAN) {
      FT_Set_Charmap(face, face->charmaps[count]);

      return;
    }
  }
}

static void clear_glyph_cache_ft(XFontStruct *xfont) {
  int count;

  for (count = 0; ((u_char**)xfont->glyphs)[count]; count++) {
    free(((u_char**)xfont->glyphs)[count]);
  }

  memset(xfont->glyphs, 0, MAX_GLYPH_TABLES * sizeof(u_char*));
  xfont->num_glyphs = 0;
  memset(xfont->glyph_indeces, 0, xfont->num_indeces * sizeof(u_int16_t));
}

static void enable_position_info_by_glyph(XFontStruct *xfont) {
  /* Shrink glyph size (+1 is margin) */
  xfont->glyph_width_bytes = (xfont->width_full / 2 + 1) * 3;
  /* +3 is for storing glyph position info. */
  xfont->glyph_size = xfont->glyph_width_bytes * xfont->height + 3;
}

static void unload_ft(XFontStruct *xfont) {
  FT_Face face;
  int count;

  free(xfont->file);

  face = xfont->face;
  face->generic.data = ((int)face->generic.data) - 1;
  if (!face->generic.data) {
    FT_Done_Face(xfont->face);
  }

  for (count = 0; ((u_char**)xfont->glyphs)[count]; count++) {
    free(((u_char**)xfont->glyphs)[count]);
  }

  free(xfont->glyphs);
  free(xfont->glyph_indeces);

  if (num_xfonts == 0 && library) {
    FT_Done_FreeType(library);
    library = NULL;
  }
}

static int is_rotated_char(u_int32_t ch) {
  if (ch < 0x80) {
    return 1;
  } else if ((ch & 0xffffff00) == 0x3000) {
    if ((0x3008 <= ch && ch <= 0x3011) || (0x3013 <= ch && ch <= 0x301c) || ch == 0x30fc) {
      return 1;
    }
  } else if ((ch & 0xffffff00) == 0xff00) {
    if ((0xff08 <= ch && ch <= 0xff09) || ch == 0xff0d || (0xff1a <= ch && ch <= 0xff1e) ||
        ch == 0xff3b || ch == 0xff3d || (0xff5b <= ch && ch <= 0xff5e) ||
        (0xff62 <= ch && ch <= 0xff63)) {
      return 1;
    }
  }
  return 0;
}

static int is_right_aligned_char(u_int32_t ch) {
  if ((ch & 0xffffff00) == 0x3000) {
    if ((0x3001 <= ch && ch <= 0x3002) || ch == 0x3063 || ch == 0x30c3 ||
        (((0x3041 <= ch && ch <= 0x3049) || (0x3083 <= ch && ch <= 0x3087) ||
          (0x30a1 <= ch && ch <= 0x30a9) || (0x30e3 <= ch && ch <= 0x30e7)) &&
         ch % 2 == 1)) {
      return 1;
    }
  } else if ((ch & 0xffffff00) == 0xff00) {
    if (ch == 0xff0c || ch == 0xff61 || ch == 0xff64) {
      return 1;
    }
  }
  return 0;
}

static u_char *get_ft_bitmap_intern(XFontStruct *xfont, u_int32_t code /* glyph index */,
                                    u_int32_t ch) {
  u_int16_t *indeces;
  int idx;
  u_char **glyphs;
  u_char *glyph;

  if (code >= xfont->num_indeces) {
    if (!(indeces = realloc(xfont->glyph_indeces, sizeof(u_int16_t) * (code + 1)))) {
      return NULL;
    }
    memset(indeces + xfont->num_indeces, 0,
           sizeof(u_int16_t) * (code + 1 - xfont->num_indeces));
    xfont->num_indeces = code + 1;
    xfont->glyph_indeces = indeces;
  } else {
    indeces = xfont->glyph_indeces;
  }

  glyphs = xfont->glyphs; /* Cast to u_char* to u_char** */

  if (!(idx = indeces[code])) {
    FT_Face face;
    int y;
    u_char *src;
    u_char *dst;
    int left_pitch;
    int pitch;
    int rows;
    int32_t format;

    if (xfont->num_glyphs >= GLYPH_TABLE_SIZE * MAX_GLYPH_TABLES - 1) {
      bl_msg_printf("Unable to show U+%x because glyph cache is full.\n", code);

      return NULL;
    }

    face = xfont->face;
    format = xfont->format;
    left_pitch = 0;

    /* CJK characters etc aren't rotated. */
    if (format & FONT_ROTATED) {
      if (is_rotated_char(ch)) {
        if (!load_glyph(face, xfont->format & ~FONT_ROTATED, code, xfont->is_aa)) {
          return NULL;
        }

        left_pitch = (face->max_advance_height - face->ascender) * face->size->metrics.y_ppem /
                     face->units_per_EM;
        if (face->glyph->bitmap.rows > face->glyph->bitmap_top) {
          if ((left_pitch -= (face->glyph->bitmap.rows - face->glyph->bitmap_top)) < 0) {
            left_pitch = 0;
          }
        }

        /* XXX 'if (xfont->is_aa) { left_pitch *= 3 }' is in the following block. */
      } else {
        format &= ~FONT_ROTATED;
      }
    }

    if (!load_glyph(face, format, code, xfont->is_aa)) {
      return NULL;
    }

    if (OFF(xfont->num_glyphs) == 0) {
      if (!(glyphs[SEG(xfont->num_glyphs)] = calloc(GLYPH_TABLE_SIZE, xfont->glyph_size))) {
        return NULL;
      }
    }

    /* idx can be changed if (pitch + left_pitch - 1) / xfont->glyph_width_bytes > 0. */
    idx = ++xfont->num_glyphs;

#if 0
    bl_debug_printf("%x %c w %d %d(%d) h %d(%d) at %d %d\n", code, code, face->glyph->bitmap.width,
                    face->glyph->bitmap.pitch, xfont->glyph_width_bytes, face->glyph->bitmap.rows,
                    xfont->height, face->glyph->bitmap_left, face->glyph->bitmap_top);
#endif

    indeces[code] = idx;

    if (xfont->format & FONT_ROTATED) {
      if (!(format & FONT_ROTATED) || face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
        /* Glyph isn't rotated. */

        if (is_right_aligned_char(ch)) {
          /* XXX Hack for vertical kutouten and sokuon */
          left_pitch += (xfont->width / 2);
        } else if (face->glyph->bitmap_left + face->glyph->bitmap.width <= xfont->width / 2) {
          left_pitch += (xfont->width / 4);
        }
      }

      if (face->glyph->bitmap.rows < xfont->height) {
        rows = face->glyph->bitmap.rows;
        y = (xfont->height - face->glyph->bitmap.rows) / 2;
      } else {
        rows = xfont->height;
        y = 0;
      }
    } else {
      if (xfont->ascent > face->glyph->bitmap_top) {
        y = xfont->ascent - face->glyph->bitmap_top;
      } else {
        y = 0;
      }

      if (face->glyph->bitmap.rows < xfont->height) {
        rows = face->glyph->bitmap.rows;

        if (rows + y > xfont->height) {
          y = xfont->height - rows;
        }
      } else {
        rows = xfont->height;
        y = 0;
      }
    }

    if (face->glyph->bitmap_left > 0) {
      left_pitch += face->glyph->bitmap_left;
    }

    if (xfont->is_aa) {
      left_pitch *= 3;

      if (HAS_POSITION_INFO_BY_GLYPH(xfont)) {
        int additional_glyphs;
        int count;

        pitch = face->glyph->bitmap.pitch;

        additional_glyphs = (pitch + left_pitch - 1) / xfont->glyph_width_bytes;
#if 0
        bl_debug_printf("Use %d(=(%d+%d-1)/%d) glyph slots for one glyph.\n",
                        1 + additional_glyphs, pitch, left_pitch, xfont->glyph_width_bytes);
#endif
        for (count = 0; count < additional_glyphs; count++) {
          if (OFF(xfont->num_glyphs + count) == 0) {
            if (1 + additional_glyphs > GLYPH_TABLE_SIZE ||
                !(glyphs[SEG(xfont->num_glyphs + count)] =
                    calloc(GLYPH_TABLE_SIZE, xfont->glyph_size))) {
              xfont->num_glyphs--;
              indeces[code] = 0;

              return NULL;
            }
            xfont->num_glyphs += (1 + count);
            indeces[code] = idx = xfont->num_glyphs;
          }
        }
        xfont->num_glyphs += additional_glyphs;
      } else if (face->glyph->bitmap.pitch < xfont->glyph_width_bytes) {
        pitch = face->glyph->bitmap.pitch;

        if (pitch + left_pitch > xfont->glyph_width_bytes) {
          left_pitch = xfont->glyph_width_bytes - pitch;
        }
      } else {
        pitch = xfont->glyph_width_bytes;
        left_pitch = 0;
      }
    } else {
      if (face->glyph->bitmap.pitch <= xfont->glyph_width_bytes) {
        pitch = face->glyph->bitmap.pitch;

        if (left_pitch >= (xfont->glyph_width_bytes - pitch + 1) * 8) {
          left_pitch = 7;
        }
      } else {
        pitch = xfont->glyph_width_bytes;
        left_pitch = 0;
      }
    }

    glyph = glyphs[SEG(idx - 1)] + xfont->glyph_size * OFF(idx - 1);

    src = face->glyph->bitmap.buffer;
    dst = glyph + (xfont->glyph_width_bytes * y);

    if (xfont->is_aa) {
      if (HAS_POSITION_INFO_BY_GLYPH(xfont)) {
        /* Storing glyph position info. (ISCII or ISO10646_UCS4_1_V) */

        glyph[0] = (face->glyph->advance.x >> 6);                /* advance */
        glyph[2] = (face->glyph->bitmap.width + left_pitch) / 3; /* width */

        if (face->glyph->bitmap_left < 0) {
          glyph[1] = -face->glyph->bitmap_left; /* retreat */
        } else {
          glyph[1] = 0;
        }

        if (glyph[0] == 0 && glyph[2] > glyph[0] + glyph[1]) {
          glyph[1] = glyph[2] - glyph[0]; /* retreat */
        }

        dst = glyph + glyph[2] * 3 * y + 3;

#if 0
        bl_debug_printf("%x %c A %d R %d W %d-> A %d R %d W %d\n", code, code,
                        face->glyph->advance.x >> 6, face->glyph->bitmap_left,
                        face->glyph->bitmap.width, glyph[0], glyph[1], glyph[2]);
#endif
        for (y = 0; y < rows; y++) {
          memcpy(dst + left_pitch, src, pitch);

          src += face->glyph->bitmap.pitch;
          dst += (glyph[2] * 3);
        }
      } else {
        for (y = 0; y < rows; y++) {
          memcpy(dst + left_pitch, src, pitch);

          src += face->glyph->bitmap.pitch;
          dst += xfont->glyph_width_bytes;
        }
      }
    } else {
      int shift;
      if ((shift = left_pitch / 8) > 0) {
        left_pitch -= (shift * 8);
        dst += shift;
      }

      if (left_pitch == 0) {
        for (y = 0; y < rows; y++) {
          memcpy(dst, src, pitch);
          src += face->glyph->bitmap.pitch;
          dst += xfont->glyph_width_bytes;
        }
      } else {
        int count;
        for (y = 0; y < rows; y++) {
          dst[0] = (src[0] >> left_pitch);
          for (count = 1; count < pitch; count++) {
            dst[count] = (src[count - 1] << (8 - left_pitch)) | (src[count] >> left_pitch);
          }
          if (shift + pitch < xfont->glyph_width_bytes) {
            dst[count] = (src[count - 1] << (8 - left_pitch));
          }

          src += face->glyph->bitmap.pitch;
          dst += xfont->glyph_width_bytes;
        }
      }
    }
  } else {
    glyph = glyphs[SEG(idx - 1)] + xfont->glyph_size * OFF(idx - 1);
  }

  return glyph;
}

static int load_xfont(XFontStruct *xfont, const char *file_path, int32_t format,
                      u_int bytes_per_pixel, ef_charset_t cs, int noaa) {
  if (!is_pcf(file_path)) {
    return load_ft(xfont, file_path, format, (bytes_per_pixel > 1) && !noaa, 0);
  } else {
    return load_pcf(xfont, file_path);
  }
}

static void unload_xfont(XFontStruct *xfont) {
  if (xfont->face) {
    unload_ft(xfont);
  } else {
    unload_pcf(xfont);
  }
}

#ifdef USE_FONTCONFIG

#include <fontconfig/fontconfig.h>

static int use_fontconfig;
static double dpi_for_fc;
static FcPattern *compl_pattern;
static char **fc_files;
static u_int num_fc_files;
static FcCharSet **fc_charsets;

static void compl_final(void) {
  if (compl_pattern) {
    u_int count;

    FcPatternDestroy(compl_pattern);
    compl_pattern = NULL;

    if (fc_files) {
      for (count = 0; count < num_fc_files; count++) {
        free(fc_files[count]);
        FcCharSetDestroy(fc_charsets[count]);
      }

      free(fc_files); /* fc_charsets is also free'ed */
      fc_files = NULL;
      fc_charsets = NULL;
    }
  }
}

static void compl_xfonts_delete(XFontStruct **xfonts) {
  if (xfonts) {
    u_int count;

    for (count = 0; count < num_fc_files; count++) {
      if (xfonts[count]) {
        xfont_unref(xfonts[count]);
      }
    }
    free(xfonts);
  }
}

/* Same processing as win32/ui_font.c (partially) and libtype/ui_font_ft.c */
static int parse_fc_font_name(char **font_family,
                              int *is_bold,   /* if bold is not specified in font_name,
                                                 not changed. */
                              int *is_italic, /* if italic is not specified in font_name,
                                                 not changed. */
                              u_int *percent, /* if percent is not specified in font_name,
                                                 not changed. */
                              char *font_name /* modified by this function. */ ) {
  char *p;
  size_t len;

  /*
   * [Family]( [WEIGHT] [SLANT] [SIZE]:[Percentage])
   */

  *font_family = font_name;

  p = font_name;
  while (1) {
    if (*p == '\\' && *(p + 1)) {
      /* Compat with 3.6.3 or before. (e.g. Foo\-Bold-iso10646-1) */

      /* skip backslash */
      p++;
    } else if (*p == '\0') {
      /* encoding and percentage is not specified. */

      *font_name = '\0';

      break;
    } else if (*p == ':') {
      /* Parsing ":[Percentage]" */

      *font_name = '\0';

      if (!bl_str_to_uint(percent, p + 1)) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " Percentage(%s) is illegal.\n", p + 1);
#endif
      }

      break;
    }

    *(font_name++) = *(p++);
  }

/*
 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
 * Following is the same as ui_font_win32.c:parse_font_name()
 * except FC_*.
 */

#if 0
  bl_debug_printf("Parsing %s for [Family] [Weight] [Slant]\n", *font_family);
#endif

  p = bl_str_chop_spaces(*font_family);
  len = strlen(p);
  while (len > 0) {
    size_t step = 0;

    if (*p == ' ') {
      char *orig_p;

      orig_p = p;
      do {
        p++;
        len--;
      } while (*p == ' ');

      if (len == 0) {
        *orig_p = '\0';

        break;
      } else {
        int count;
        char *styles[] = { "italic",  "bold", "oblique", "light", "semi-bold", "heavy",
                           "semi-condensed", };

        for (count = 0; count < sizeof(styles) / sizeof(styles[0]); count++) {
          size_t len_v;

          len_v = strlen(styles[count]);

          /* XXX strncasecmp is not portable? */
          if (len >= len_v && strncasecmp(p, styles[count], len_v) == 0) {
            /* [WEIGHT] [SLANT] */
            *orig_p = '\0';
            step = len_v;

            if (count <= 1) {
              if (count == 0) {
                *is_italic = 1;
              } else {
                *is_bold = 1;
              }
            }

            goto next_char;
          }
        }

        if (*p != '0' ||      /* In case of "DevLys 010" font family. */
            *(p + 1) == '\0') /* "MS Gothic 0" => "MS Gothic" + "0" */
        {
          char *end;
          double size;

          size = strtod(p, &end);
          if (*end == '\0') {
            /* [SIZE] */
            *orig_p = '\0';
            break; /* p has no more parameters. */
          }
        }

        step = 1;
      }
    } else {
      step = 1;
    }

  next_char:
    p += step;
    len -= step;
  }

  return 1;
}

static int is_same_family(FcPattern *pattern, const char *family) {
  int count;
  FcValue val;

  for (count = 0; FcPatternGet(pattern, FC_FAMILY, count, &val) == FcResultMatch; count++) {
    if (strcmp(family, val.u.s) == 0) {
      return 1;
    }
  }

  return 0;
}

static u_int strip_pattern(FcPattern *pattern, FcPattern *remove) {
  u_int count = 0;
  FcValue val;

  while (FcPatternGet(pattern, FC_FAMILY, count, &val) == FcResultMatch) {
    if (is_same_family(remove, val.u.s)) {
      /* Remove not only matched name but also alias names */
      FcPatternRemove(pattern, FC_FAMILY, count);
    } else {
      int count2 = ++count;
      FcValue val2;

      while(FcPatternGet(pattern, FC_FAMILY, count2, &val2) == FcResultMatch) {
        if (strcmp(val.u.s, val2.u.s) == 0) {
          FcPatternRemove(pattern, FC_FAMILY, count2);
        } else {
          count2++;
        }
      }
    }
  }

  return count;
}

static FcPattern *fc_pattern_create(const FcChar8* family) {
  FcPattern *pattern;

  if (!(pattern = FcPatternCreate())) {
    return NULL;
  }

  if (family) {
    FcPatternAddString(pattern, FC_FAMILY, family);
  }

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);

  FcPatternRemove(pattern, FC_FAMILYLANG, 0);
  FcPatternRemove(pattern, FC_STYLELANG, 0);
  FcPatternRemove(pattern, FC_FULLNAMELANG, 0);
#ifdef FC_NAMELANG
  FcPatternRemove(pattern, FC_NAMELANG, 0);
#endif
  FcPatternRemove(pattern, FC_LANG, 0);

  FcDefaultSubstitute(pattern);

  return pattern;
}

/* XXX Lazy check */
static int check_iscii_font(FcPattern *pattern) {
  FcValue val;

  if (FcPatternGet(pattern, FC_FAMILY, 0, &val) == FcResultMatch && strstr(val.u.s, "-TT")) {
    return 1;
  } else {
    return 0;
  }
}

static FcPattern *parse_font_name(const char *fontname, int *is_bold, int *is_italic,
                                  u_int *percent, ef_charset_t cs) {
  FcPattern *pattern;
  FcPattern *match;
  FcResult result;
  char *family;

  *percent = 0;
  *is_bold = 0;
  *is_italic = 0;

  if (!fontname) {
    family = NULL;
  } else {
    parse_fc_font_name(&family, is_bold, is_italic, percent, bl_str_alloca_dup(fontname));
  }

  if ((pattern = fc_pattern_create(family))) {
    match = FcFontMatch(NULL, pattern, &result);

    if (IS_ISCII(cs) && match && !check_iscii_font(match)) {
      FcPatternDestroy(match);
      FcPatternDestroy(pattern);

      return NULL;
    }

    if (compl_pattern == NULL && match) {
      num_fc_files = strip_pattern((compl_pattern = pattern), match);
    } else {
      FcPatternDestroy(pattern);
    }
  } else {
    match = NULL;
  }

  return match;
}

#ifdef USE_WIN32API
static void init_fontconfig(void) {
  if (!getenv("FONTCONFIG_PATH") && !getenv("FONTCONFIG_FILE")) {
    /*
     * See fontconfig-x.x.x/src/fccfg.c
     * (DllMain(), FcConfigFileExists(), FcConfigGetPath() and FcConfigFilename())
     *
     * [commant in DllMain]
     * If the fontconfig DLL is in a "bin" or "lib" subfolder, assume it's a Unix-style
     * installation tree, and use "etc/fonts" in there as FONTCONFIG_PATH.
     * Otherwise use the folder where the DLL is as FONTCONFIG_PATH.
     *
     * [comment in FcConfigFileExists]
     * make sure there's a single separator
     * (=> If FONTCONFIG_PATH="", FONTCONFIG_FILE="/...")
     */
    putenv("FONTCONFIG_PATH=.");
  }
}
#endif

#endif /* USE_FONTCONFIG */

static u_char *get_ft_bitmap(XFontStruct *xfont, u_int32_t ch, int use_ot_layout,
                             XFontStruct **compl_xfont) {
#ifdef USE_FONTCONFIG
  u_int count;
#endif
  u_char *bitmap;
  u_int32_t code;

  if (!use_ot_layout) {
    /* char => glyph index */
    if (ch == 0x20) {
      return NULL;
    }

    if ((code = get_glyph_index(xfont->face, ch)) == 0) {
      goto compl_font;
    }
  } else {
    code = ch;
    ch = 0xffffffff; /* check_rotate() always returns 0. */
  }

  return get_ft_bitmap_intern(xfont, code, ch);

 compl_font:
  /*
   * Complementary glyphs are searched only if xfont->face && !IS_PROPOTIONAL && !use_ot_layout.
   */
#ifdef USE_FONTCONFIG
  if (!compl_pattern) {
    return NULL;
  }

  if (!fc_files) {
    if (!(fc_files = calloc(num_fc_files, sizeof(*fc_charsets) + sizeof(*fc_files)))) {
      return NULL;
    }

    fc_charsets = fc_files + num_fc_files;
  }

  if (!xfont->compl_xfonts &&
      !(xfont->compl_xfonts = calloc(num_fc_files, sizeof(*xfont->compl_xfonts)))) {
    return NULL;
  }

  for (count = 0; count < num_fc_files;) {
    if (!fc_files[count]) {
      FcResult result;
      FcPattern *match;

      match = FcFontMatch(NULL, compl_pattern, &result);
      FcPatternRemove(compl_pattern, FC_FAMILY, 0);

      if (match) {
        num_fc_files = count + 1 + strip_pattern(compl_pattern, match);

        if (FcPatternGetCharSet(match, FC_CHARSET, 0, &fc_charsets[count]) == FcResultMatch) {
          FcValue val;

          fc_charsets[count] = FcCharSetCopy(fc_charsets[count]);
          if (FcPatternGet(match, FC_FILE, 0, &val) == FcResultMatch) {
            fc_files[count] = strdup(val.u.s);
          }
        }

        FcPatternDestroy(match);
      }

      if (!fc_files[count]) {
        num_fc_files --;
        if (fc_charsets[count]) {
          FcCharSetDestroy(fc_charsets[count]);
        }
        continue;
      }
    }

    if (FcCharSetHasChar(fc_charsets[count], ch)) {
      XFontStruct *compl;

      if (!xfont->compl_xfonts[count]) {
        if (!(compl = get_cached_xfont(fc_files[count], xfont->format))) {
          if (!(compl = calloc(1, sizeof(XFontStruct)))) {
            continue;
          }
          /*
           * XXX
           * If xfont->height is 17 and compl->height is 15, garbage is left in drawing glyphs
           * by compl.
           * force_height (xfont->height) forcibly changes the height of the font from natural one
           * to the same one as xfont->height.
           */
          else if (!load_ft(compl, fc_files[count], xfont->format, xfont->is_aa, xfont->height)) {
            free(compl);
            continue;
          }

          if (!add_xfont_to_cache(compl)) {
            continue;
          }
        }

        xfont->compl_xfonts[count] = compl;
      } else {
        compl = xfont->compl_xfonts[count];
      }

      if (HAS_POSITION_INFO_BY_GLYPH(xfont) && !HAS_POSITION_INFO_BY_GLYPH(compl)) {
        clear_glyph_cache_ft(compl);
        enable_position_info_by_glyph(compl);
      }

      if ((code = get_glyph_index(compl->face, ch)) > 0) {
        if ((bitmap = get_ft_bitmap_intern(compl, code, ch))) {
          if (compl_xfont) {
            *compl_xfont = compl;
          }
#ifdef __DEBUG
          bl_debug_printf("Use %s font to show U+%x\n", (*compl_xfont)->file, ch);
#endif

          return bitmap;
        }
      }
    }

    count++;
  }
#endif
  return NULL;
}

#else /* USE_FREETYPE */

#define load_xfont(xfont, file_path, format, bytes_per_pixel, cs, noaa) load_pcf(xfont, file_path)
#define unload_xfont(xfont) unload_pcf(xfont)

#endif /* USE_FREETYPE */

static XFontStruct *get_cached_xfont(const char *file, int32_t format) {
  u_int count;

  for (count = 0; count < num_xfonts; count++) {
    if (strcmp(xfonts[count]->file, file) == 0
#ifdef USE_FREETYPE
        && (xfonts[count]->face == NULL || xfonts[count]->format == format)
#endif
        ) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Use cached XFontStruct for %s.\n", xfonts[count]->file);
#endif
      xfonts[count]->ref_count++;

      return xfonts[count];
    }
  }

  return NULL;
}

static int add_xfont_to_cache(XFontStruct *xfont) {
  void *p;

  if (!(p = realloc(xfonts, sizeof(XFontStruct*) * (num_xfonts + 1)))) {
    unload_xfont(xfont);
    free(xfont);

    return 0;
  }

  xfonts = p;
  xfonts[num_xfonts++] = xfont;
  xfont->ref_count = 1;

  return 1;
}

static void xfont_unref(XFontStruct *xfont) {
  if (--xfont->ref_count == 0) {
    u_int count;

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
    compl_xfonts_delete(xfont->compl_xfonts);
#endif

    for (count = 0; count < num_xfonts; count++) {
      if (xfonts[count] == xfont) {
        if (--num_xfonts > 0) {
          xfonts[count] = xfonts[num_xfonts];
        } else {
          free(xfonts);
          xfonts = NULL;
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
          compl_final();
#endif
        }

        break;
      }
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " %s font is unloaded. => CURRENT NUM OF XFONTS %d\n",
                    xfont->file, num_xfonts);
#endif

    unload_xfont(xfont);
    free(xfont);
  }
}

/* --- static variables --- */

static int compose_dec_special_font;

/* --- global functions --- */

void ui_compose_dec_special_font(void) {
  compose_dec_special_font = 1;
}

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
void ui_font_use_fontconfig(void) { use_fontconfig = 1; }
#endif

ui_font_t *ui_font_new(Display *display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char *fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold,
                       u_int letter_space /* Ignored for now. */
                       ) {
  ef_charset_t cs;
  char *font_file;
  char *decsp_id = NULL;
  u_int percent;
  ui_font_t *font;
  vt_font_t orig_id = id;
#ifdef USE_FREETYPE
  u_int format;
#endif
  u_int cols;

  cs = FONT_CS(id);

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  if (use_fontconfig) {
    FcPattern *pattern;
    FcValue val;
    int is_bold;
    int is_italic;

#ifdef USE_WIN32API
    init_fontconfig();
#endif

    if (!(pattern = parse_font_name(fontname, &is_bold, &is_italic, &percent, cs))) {
      return NULL;
    }

    if (is_bold) {
      id |= FONT_BOLD;
    }
    if (is_italic) {
      id |= FONT_ITALIC;
    }

    if (FcPatternGet(pattern, FC_FILE, 0, &val) != FcResultMatch) {
      FcPatternDestroy(pattern);

      return NULL;
    }

    font_file = bl_str_alloca_dup(val.u.s);
    FcPatternDestroy(pattern);
  } else
#endif
  if (!fontname) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Font file is not specified.\n");
#endif

    if (cs == DEC_SPECIAL) {
      /* use ui_decsp_font_new() */
      font_file = NULL;
      percent = 0;
    }
    /*
     * Default font on Androidis is ttf, so charsets except unicode and ISO88591 are available.
     * (Chars except unicode are converted to unicode in ui_window.c for freetype.)
     */
#ifndef __ANDROID__
    else if (!IS_ISO10646_UCS4(cs) && cs != ISO8859_1_R) {
      return NULL;
    }
#endif
    else {
      struct stat st;

#if defined(__FreeBSD__)
      if (stat("/usr/local/lib/X11/fonts/local/unifont.pcf.gz", &st) == 0) {
        font_file = "/usr/local/lib/X11/fonts/local/unifont.pcf.gz";
        percent = 100;
      } else {
        font_file = "/usr/local/lib/X11/fonts/misc/10x20.pcf.gz";
        percent = 0;
      }
#elif defined(__NetBSD__)
      percent = 0;
      if (stat("/usr/pkg/lib/X11/fonts/efont/b16.pcf.gz", &st) == 0) {
        font_file = "/usr/pkg/lib/X11/fonts/efont/b16.pcf.gz";
      } else {
        font_file = "/usr/X11R7/lib/X11/fonts/misc/10x20.pcf.gz";
      }
#elif defined(__OpenBSD__)
      if (stat("/usr/X11R6/lib/X11/fonts/misc/unifont.pcf.gz", &st) == 0) {
        font_file = "/usr/X11R6/lib/X11/fonts/misc/unifont.pcf.gz";
        percent = 100;
      } else {
        font_file = "/usr/X11R6/lib/X11/fonts/misc/10x20.pcf.gz";
        percent = 0;
      }
#elif defined(__ANDROID__)
      if (stat("/system/fonts/DroidSansMono.ttf", &st) == 0) {
        font_file = "/system/fonts/DroidSansMono.ttf";
      } else {
        DIR* dir;
        struct dirent *entry;
        const char *cand;

        if ((dir = opendir("/system/fonts")) == NULL) {
          return NULL;
        }

        cand = NULL;

        while ((entry = readdir(dir))) {
          if (strcasestr(entry->d_name, ".tt")) {
            if (cand == NULL) {
              cand = bl_str_alloca_dup(entry->d_name);
            } else if (strcasestr(entry->d_name, "Mono")) {
              cand = bl_str_alloca_dup(entry->d_name);
              break;
            }
          }
        }

        closedir(dir);

        if (cand == NULL || !(font_file = alloca(14 + strlen(cand) + 1))) {
          return NULL;
        }

        strcpy(font_file, "/system/fonts");
        font_file[13] = '/';
        strcpy(font_file + 14, cand);
      }
      percent = 0;
#else /* __linux__ */
      if (stat("/usr/share/fonts/X11/misc/unifont.pcf.gz", &st) == 0) {
        font_file = "/usr/share/fonts/X11/misc/unifont.pcf.gz";
        percent = 100;
      } else {
        font_file = "/usr/share/fonts/X11/misc/10x20.pcf.gz";
        percent = 0;
      }
#endif

#ifndef __ANDROID__
      /* XXX double drawing is used to bolden pcf font. */
      if (id & FONT_BOLD) {
        use_medium_for_bold = 1;
      }
#endif
    }
  } else {
    char *percent_str;

    if (!(percent_str = bl_str_alloca_dup(fontname))) {
      return NULL;
    }

#ifdef USE_WIN32API
    if (percent_str[0] != '\0' && percent_str[1] == ':') {
      /* c:/Users/... */
      font_file = percent_str;
      percent_str += 2;
      bl_str_sep(&percent_str, ":");
    } else
#endif
    {
      font_file = bl_str_sep(&percent_str, ":");
    }

    if (!percent_str || !bl_str_to_uint(&percent, percent_str)) {
      percent = 0;
    }
  }

  if (cs == DEC_SPECIAL && (compose_dec_special_font || font_file == NULL || !is_pcf(font_file))) {
    if (!(decsp_id = alloca(6 + DIGIT_STR_LEN(u_int) + 1 + DIGIT_STR_LEN(u_int) + 1))) {
      return NULL;
    }
  }

  if (type_engine != TYPE_XCORE || !(font = calloc(1, sizeof(ui_font_t)))) {
    return NULL;
  }

  if (size_attr >= DOUBLE_HEIGHT_TOP) {
    fontsize *= 2;
    col_width *= 2;
  }

#ifdef USE_FREETYPE
#ifdef USE_FONTCONFIG
  if (use_fontconfig && dpi_for_fc > 0.0) {
    fontsize = DIVIDE_ROUNDINGDOWN(dpi_for_fc * fontsize, 72);
  }
#endif
  format = fontsize | (id & (FONT_BOLD | FONT_ITALIC));
  if (font_present & FONT_VERT_RTL) {
    format |= FONT_ROTATED;
    /* XXX Not support ROTATED and ITALIC at the same time. (see load_glyph()). */
    format &= ~FONT_ITALIC;
  }
#endif

  if (decsp_id) {
    sprintf(decsp_id, "decsp-%dx%d", col_width, fontsize);
  }

  if ((font->xfont = get_cached_xfont(decsp_id ? decsp_id : font_file,
#ifdef USE_FREETYPE
                                      format
#else
                                      0
#endif
       ))) {
    goto xfont_loaded;
  }

  if (!(font->xfont = calloc(1, sizeof(XFontStruct)))) {
    free(font);

    return NULL;
  }

  font->display = display;

  if (decsp_id) {
    if (!ui_load_decsp_xfont(font->xfont, decsp_id)) {
      compose_dec_special_font = 0;

      free(font->xfont);
      free(font);

      if (size_attr >= DOUBLE_HEIGHT_TOP) {
        fontsize /= 2;
        col_width /= 2;
      }

      return ui_font_new(display, id, size_attr, type_engine, font_present,
                         font_file, fontsize, col_width,
                         use_medium_for_bold, letter_space);
    }
  } else if (!load_xfont(font->xfont, font_file, format, display->bytes_per_pixel, cs,
                         font_present & FONT_NOAA)) {
    bl_msg_printf("Failed to load %s.\n", font_file);

    free(font->xfont);
    free(font);

    if (fontname) {
      if (size_attr >= DOUBLE_HEIGHT_TOP) {
        fontsize /= 2;
        col_width /= 2;
      }

      return ui_font_new(display, id, size_attr, type_engine, font_present,
                         NULL /* Fall back to the default font */, fontsize, col_width,
                         use_medium_for_bold, letter_space);
    } else {
      return NULL;
    }
  }

  if (!add_xfont_to_cache(font->xfont)) {
    free(font);

    return NULL;
  }

xfont_loaded:
  /* Following is almost the same processing as xlib. */

  font->id = orig_id;

  if (font->id & FONT_FULLWIDTH) {
    cols = 2;
  } else {
    cols = 1;
  }

#ifdef USE_FREETYPE
  if (HAS_POSITION_INFO_BY_GLYPH(font->xfont)) {
    font->is_proportional = 1;
  }

  if (IS_ISCII(cs) && font->xfont->is_aa &&
      (font->xfont->ref_count == 1 || HAS_POSITION_INFO_BY_GLYPH(font->xfont))) {
    /* Proportional glyph is available on ISCII alone for now. */
    font->is_var_col_width = font->is_proportional = 1;

    if (font->xfont->ref_count == 1) {
      init_iscii_ft(font->xfont->face);
      enable_position_info_by_glyph(font->xfont);
    }
  } else
#endif
  if (font_present & FONT_VAR_WIDTH) {
    /*
     * If you use fixed-width fonts whose width is differnet from
     * each other.
     */
    font->is_var_col_width = 1;

#ifdef USE_FREETYPE
    font->is_proportional = 1;

    if (font->xfont->ref_count == 1) {
      enable_position_info_by_glyph(font->xfont);
    } else if (!HAS_POSITION_INFO_BY_GLYPH(font->xfont)) {
      clear_glyph_cache_ft(font->xfont);
      enable_position_info_by_glyph(font->xfont);
    }
#endif
  }

  if (font_present & FONT_VERTICAL) {
    font->is_vertical = 1;
  }

  if (use_medium_for_bold) {
    font->double_draw_gap = 1;
  }

  if (id & FONT_FULLWIDTH) {
    font->width = font->xfont->width_full;
  } else {
    font->width = font->xfont->width;
  }

  font->height = font->xfont->height;
  font->ascent = font->xfont->ascent;

  if (col_width == 0) {
    /* standard(usascii) font */

    if (percent > 0) {
      u_int ch_width;

      if (font->is_vertical) {
        /*
         * !! Notice !!
         * The width of full and half character font is the same.
         */
        ch_width = DIVIDE_ROUNDING(fontsize * percent, 100);
      } else {
        ch_width = DIVIDE_ROUNDING(fontsize * percent, 200);
      }

      if (font->width != ch_width) {
        if (!font->is_var_col_width) {
          /*
           * If width(2) of '1' doesn't match ch_width(4)
           * x_off = (4-2)/2 = 1.
           * It means that starting position of drawing '1' is 1
           * as follows.
           *
           *  0123
           * +----+
           * | ** |
           * |  * |
           * |  * |
           * +----+
           */
          if (font->width < ch_width) {
            font->x_off = (ch_width - font->width) / 2;
          }

          font->width = ch_width;
        }
      }
    } else if (font->is_vertical) {
#ifdef USE_FREETYPE
      if (!font->xfont->face || !(font->xfont->format & FONT_ROTATED))
#endif
      {
        /*
         * !! Notice !!
         * The width of full and half character font is the same.
         */
        font->x_off = font->width / 2;
        font->width *= 2;
      }
    }

    if (letter_space > 0) {
      font->width += letter_space;
      font->x_off += (letter_space / 2);
    }
  } else {
    /* not a standard(usascii) font */

    /*
     * XXX hack
     * forcibly conforming non standard font width to standard font width.
     */

    if (font->is_vertical) {
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      if (font->width != col_width) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, col_width);

/* is_var_col_width is always false if is_vertical is true. */
#if 0
        if (!font->is_var_col_width)
#endif
        {
          if (font->width < col_width) {
            font->x_off = (col_width - font->width) / 2;
          }

          font->width = col_width;
        }
      }
    } else {
      if (font->width != col_width * cols) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, col_width * cols);

        if (!font->is_var_col_width) {
          if (font->width < col_width * cols) {
            font->x_off = (col_width * cols - font->width) / 2;
          }

          font->width = col_width * cols;
        }
      }
    }
  }

  if (size_attr == DOUBLE_WIDTH) {
    font->x_off += (font->width / 2);
    font->width *= 2;
  }

  font->size_attr = size_attr;

  /*
   * checking if font width/height/ascent member is sane.
   */

  if (font->width == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " font width is 0.\n");
#endif

    /* XXX this may be inaccurate. */
    font->width = DIVIDE_ROUNDINGUP(fontsize * cols, 2);
  }

  if (font->height == 0) {
    /* XXX this may be inaccurate. */
    font->height = fontsize;
  }

  if (font->ascent == 0) {
    /* XXX this may be inaccurate. */
    font->ascent = fontsize;
  }

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s font is loaded. => CURRENT NUM OF XFONTS %d\n",
                  font->xfont->file, num_xfonts);
#endif

#ifdef DEBUG
  ui_font_dump(font);
#endif

  return font;
}

void ui_font_delete(ui_font_t *font) {
  xfont_unref(font->xfont);

#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  free(font);
}

#ifdef USE_OT_LAYOUT

int ui_font_has_ot_layout_table(ui_font_t *font) {
#ifdef USE_FREETYPE
  if (font->xfont->face) {
    if (!font->ot_font) {
      if (font->ot_font_not_found || !(font->ot_font = otl_open(font->xfont->face, 0))) {
        font->ot_font_not_found = 1;

        return 0;
      }

      if (!HAS_POSITION_INFO_BY_GLYPH(font->xfont)) {
        clear_glyph_cache_ft(font->xfont);
        enable_position_info_by_glyph(font->xfont);
      }
      font->is_proportional = 1; /* font->is_var_col_width can be 0 or 1. */
    }

    return 1;
  }
#endif
  return 0;
}

u_int ui_convert_text_to_glyphs(ui_font_t *font, u_int32_t *shaped, u_int shaped_len,
                                int8_t *offsets, u_int8_t *widths, u_int32_t *cmapped,
                                u_int32_t *src, u_int src_len, const char *script,
                                const char *features) {
  return otl_convert_text_to_glyphs(font->ot_font, shaped, shaped_len, offsets, widths, cmapped,
                                    src, src_len, script, features, 0);
}

#endif

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int *draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

#if defined(USE_FREETYPE)
  if (font->xfont->is_aa && font->is_proportional && font->is_var_col_width) {
    u_char *glyph;

    if ((glyph = get_ft_bitmap(font->xfont, ch,
#ifdef USE_OT_LAYOUT
                               (font->use_ot_layout /* && font->ot_font */)
#else
                               0
#endif
                               , NULL))) {
      return glyph[0];
    }
  }
#endif

  return font->width;
}

/* Return written size */
size_t ui_convert_ucs4_to_utf16(u_char *dst, /* 4 bytes. Big endian. */
                                u_int32_t src) {
  if (src < 0x10000) {
    dst[0] = (src >> 8) & 0xff;
    dst[1] = src & 0xff;

    return 2;
  } else if (src < 0x110000) {
    /* surrogate pair */

    u_char c;

    src -= 0x10000;
    c = (u_char)(src / (0x100 * 0x400));
    src -= (c * 0x100 * 0x400);
    dst[0] = c + 0xd8;

    c = (u_char)(src / 0x400);
    src -= (c * 0x400);
    dst[1] = c;

    c = (u_char)(src / 0x100);
    src -= (c * 0x100);
    dst[2] = c + 0xdc;
    dst[3] = (u_char)src;

    return 4;
  }

  return 0;
}

#ifdef DEBUG

void ui_font_dump(ui_font_t *font) {
  bl_msg_printf("Font id %x: XFont %p (width %d, height %d, ascent %d, x_off %d)", font->id,
                font->xfont, font->width, font->height, font->ascent, font->x_off);

  if (font->is_proportional) {
    bl_msg_printf(" (proportional)");
  }

  if (font->is_var_col_width) {
    bl_msg_printf(" (var col width)");
  }

  if (font->is_vertical) {
    bl_msg_printf(" (vertical)");
  }

  if (font->double_draw_gap) {
    bl_msg_printf(" (double drawing)");
  }

  bl_msg_printf("\n");
}

#endif

u_char *ui_get_bitmap(XFontStruct *xfont, u_char *ch, size_t len, int use_ot_layout,
                      XFontStruct **compl_xfont) {
  size_t ch_idx;
  int16_t glyph_idx;
  int32_t glyph_offset;

#ifdef USE_FREETYPE
  if (xfont->face) {
    return get_ft_bitmap(xfont, ef_bytes_to_int(ch, len), use_ot_layout, compl_xfont);
  } else
#endif
      if (len == 1) {
    ch_idx = ch[0] - xfont->min_char_or_byte2;
  } else if (len == 2) {
    ch_idx =
        (ch[0] - xfont->min_byte1) * (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) +
        ch[1] - xfont->min_char_or_byte2;
  } else /* if( len == 4) */
  {
    ch_idx = (ch[1] * 0x100 + ch[2] - xfont->min_byte1) *
                 (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) +
             ch[3] - xfont->min_char_or_byte2;
  }

  if (ch_idx >= (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1)*(xfont->max_byte1 -
                                                                           xfont->min_byte1 + 1) ||
      (glyph_idx = xfont->glyph_indeces[ch_idx]) == -1) {
    return NULL;
  }

  /*
   * glyph_idx should be casted to unsigned in order not to be minus
   * if it is over 32767.
   */
  glyph_offset = xfont->glyph_offsets[(u_int16_t)glyph_idx];

#if 0
  bl_debug_printf(BL_DEBUG_TAG " chindex %d glindex %d glyph offset %d\n", ch_idx, glyph_idx,
                  glyph_offset);
#endif

  return xfont->glyphs + glyph_offset;
}

/* For mlterm-libvte */
void ui_font_set_dpi_for_fc(double dpi) {
#ifdef USE_FONTCONFIG
  dpi_for_fc = dpi;
#endif
}
