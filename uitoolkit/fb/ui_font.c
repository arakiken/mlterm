/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#define _GNU_SOURCE /* strcasestr */

#if 0
#define DEBUG
#define __DEBUG
#endif

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
#define strcasestr(a, b) strstr(a, b)
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

/* --- static variables --- */

static XFontStruct **xfonts;
static u_int num_xfonts;

static XFontStruct *get_cached_xfont(const char *file, int32_t format, int is_aa);
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
#if 0
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
      ((int16_t*)xfont->glyph_indeces)[count] = _TOINT16(p, is_be);
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

static int get_metrics(u_int16_t *width, u_int16_t *width_full, u_int16_t *height,
                       u_int16_t *ascent, u_char *p, size_t size, int is_be, int is_compressed) {
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

  if (*width == 0 || *height == 0) {
    /* XXX The height of mplus_h10r.pcf.gz, mplus_s10r.pcf.gz etc is 0. */
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Illegal pcf metrics: width (%d), height (%d)\n",
                    *width, *height);
#endif

    return 0;
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
#ifdef DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " Unsupported table: %x\n", type);
    }
#endif
  }

#ifdef __DEBUG
  {
#if 0
    u_char ch[] = "\x97\xf3";
#elif 1
    u_char ch[] = "a";
#else
    u_char ch[] = "\x06\x22"; /* UCS2 */
#endif
    u_char *bitmap;
    int i;
    int j;

    if ((bitmap = ui_get_bitmap(xfont, ch, sizeof(ch) - 1, 0, 0, NULL))) {
      for (j = 0; j < xfont->height; j++) {
        u_char *line;

        ui_get_bitmap_line(xfont, bitmap, j * xfont->glyph_width_bytes, line);

        for (i = 0; i < xfont->width; i++) {
          bl_msg_printf("%c", (line && ui_get_bitmap_cell(line, i)) ? '*' : ' ');
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

#define MAX_GLYPH_TABLES 512
#define GLYPH_TABLE_SIZE 128
#define INITIAL_GLYPH_INDEX_TABLE_SIZE 0x1000
/* 0 - 511 */
#define SEG(idx) (((idx) >> 23) & 0x1ff)
/* 0 - 8388607 */
#define OFF(idx) ((idx) & 0x7fffff)
#define MAX_OFF OFF(0xffffffff)

/*
 * for stroing height position info and pitch (See get_ft_bitmap())
 * 1st-2nd byte: left_pitch + pitch, 3rd byte: bitmap_top, 4th byte: height
 * (This should be aligned because face->glyph->bitmap.pitch (not bitmap.width)
 * is used if xfont->has_each_glyph_width_info is 0.)
 */
#define GLYPH_HEADER_SIZE_MIN 4

/* for storing height and width position info */
#define GLYPH_HEADER_SIZE_MAX (GLYPH_HEADER_SIZE_MIN + 3)

#define GLYPH_ADV_IDX (GLYPH_HEADER_SIZE_MIN + 0)
#define GLYPH_RETREAT_IDX (GLYPH_HEADER_SIZE_MIN + 1)
#define GLYPH_WIDTH_IDX (GLYPH_HEADER_SIZE_MIN + 2)
#define GLYPH_ADV(glyph) ((glyph)[GLYPH_ADV_IDX])
#define GLYPH_RETREAT(glyph) ((glyph)[GLYPH_RETREAT_IDX])
#define GLYPH_WIDTH(glyph) ((glyph)[GLYPH_WIDTH_IDX])

#define FONT_ROTATED (FONT_ITALIC << 1)
#define FONTSIZE_IN_FORMAT(format) ((format) & ~(FONT_BOLD | FONT_ITALIC | FONT_ROTATED))

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
                   u_int force_height, u_int force_ascent) {
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

  fontsize = FONTSIZE_IN_FORMAT(format);

  for (count = 0; count < num_xfonts; count++) {
    if (strcmp(xfonts[count]->file, file_path) == 0 &&
        /* The same face is used for normal, italic and bold. */
        FONTSIZE_IN_FORMAT(xfonts[count]->format) == fontsize) {
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
      !(xfont->glyph_indeces = calloc(xfont->num_indeces, sizeof(u_int32_t))) ||
      !(xfont->glyphs = calloc(MAX_GLYPH_TABLES, sizeof(u_char*)))) {
    goto error;
  }

  face->generic.data = ((int)face->generic.data) + 1; /* ref_count */

  if (force_height) {
    xfont->height = force_height;
  } else {
    /* XXX face->max_advance_height might be unexpectedly big. (e.g. NotoSansCJKjp-Reguar.otf) */
    xfont->height = (face->height * face->size->metrics.y_ppem +
                     face->units_per_EM - 1) / face->units_per_EM;
#ifdef __DEBUG
    bl_debug_printf("height %d (max height %d) ppem %d units %d => h %d\n",
                    face->height, face->max_advance_height, face->size->metrics.y_ppem,
                    face->units_per_EM, xfont->height);
    bl_debug_printf("(yMax %d yMin %d)\n", face->bbox.yMax, face->bbox.yMin);
#endif
  }

  if (format & FONT_ROTATED) {
    xfont->width = xfont->width_full = xfont->height;
    xfont->ascent = 0;
  } else {
    u_int limit;

    xfont->width_full = (face->max_advance_width * face->size->metrics.x_ppem +
                         face->units_per_EM - 1) / face->units_per_EM;
#ifdef __DEBUG
    bl_debug_printf("maxw %d ppem %d units %d => xfont->width_full %d\n",
                    face->max_advance_width, face->size->metrics.x_ppem,
                    face->units_per_EM, xfont->width_full);
    bl_debug_printf("(Adv M %d MaxAdv %d xMax %d xMin %d)\n",
                    face->glyph->advance.x >> 6, face->size->metrics.max_advance,
                    face->bbox.xMax, face->bbox.xMin);
#endif

    xfont->width = face->glyph->advance.x >> 6;

    if (force_ascent) {
      xfont->ascent = force_ascent;
    } else {
      xfont->ascent =
        (face->ascender * face->size->metrics.y_ppem + face->units_per_EM - 1) / face->units_per_EM;

#if 1
      if (xfont->height < xfont->ascent) {
        xfont->height = xfont->ascent;
      }
#else
      if (load_glyph(face, format, get_glyph_index(face, 'j'), is_aa)) {
        int descent = face->glyph->bitmap.rows - face->glyph->bitmap_top;

        if (descent + xfont->ascent > xfont->height) {
#ifdef __DEBUG
          bl_debug_printf("Modify xfont->height to %d\n", xfont->ascent + descent);
#endif
          xfont->height = xfont->ascent + descent;
        }

        if (face->glyph->bitmap_top > xfont->ascent) {
          /* Pragmata Pro Mono: bitmap_top of 'i' and 'j' > xfont->ascent. */
#ifdef __DEBUG
          bl_debug_printf("Modify xfont->ascent to %d\n", face->glyph->bitmap_top);
#endif
          xfont->height += (face->glyph->bitmap_top - xfont->ascent);
          xfont->ascent = face->glyph->bitmap_top;
        }
      }
#endif
    }

#if 0
    limit = xfont->width * 3 - 1;
    if (limit <= xfont->width_full) {
      char *file = bl_basename(file_path);

#ifdef DEBUG
      bl_msg_printf("Max advance of %s font (%d) is too larger than the normal width (%d).\n",
                    file_path, xfont->width_full, xfont->width);
#endif

      if (strstr(file, "NotoSansMono") || strstr(file, "Inconsolata")) {
        /*
         * XXX
         * 'width_full' of Inconsolata and NotoSansMono calculated above is 3 times
         * larger than expected. (Subpixel length ?)
         */
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " width_full %d -> %d\n",
                        xfont->width_full, (xfont->width_full + 2) / 3);
#endif
        xfont->width_full = (xfont->width_full + 2) / 3;
      }
    }
#endif

    if (load_glyph(face, format, get_glyph_index(face, 'W'), is_aa)) {
      u_int w;

      if (is_aa) {
        w = face->glyph->bitmap.width / 3;
      } else {
        w = face->glyph->bitmap.width;
      }

      if (xfont->width_full < w) {
#ifdef __DEBUG
        bl_debug_printf("Modify xfont->width_full to %d\n", w);
#endif
        xfont->width_full = w;
      }
    }
  }

  if (is_aa) {
    /*
     * If xfont->is_aa is true, xfont->glyph_width_bytes is used only for
     * glyph_size in next_glyph_buf() and memory is allocated large enough to
     * store each glyph regardless of the value of xfont->glyph_width_bytes.
     * So xfont->width_full is not used here.
     */
    xfont->glyph_width_bytes = xfont->width * 3;
  } else {
    xfont->glyph_width_bytes = (xfont->width_full + 7) / 8;
  }

#if 0
  bl_debug_printf("w %d %d h %d a %d (%d bytes/glyph line)\n", xfont->width, xfont->width_full,
                  xfont->height, xfont->ascent, xfont->glyph_width_bytes);
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
  xfont->num_glyph_bufs = 0;
  xfont->glyph_buf_left = 0;
  memset(xfont->glyph_indeces, 0, xfont->num_indeces * sizeof(u_int32_t));
}

static void enable_position_info_by_glyph(XFontStruct *xfont) {
  /*
   * If xfont->is_aa is true, xfont->glyph_width_bytes is used only for
   * glyph_size in next_glyph_buf() and memory is allocated large enough to
   * store each glyph regardless of the value of xfont->glyph_width_bytes.
   * So xfont->width_full is not used here.
   */
  xfont->glyph_width_bytes = xfont->width * 3;
  xfont->has_each_glyph_width_info = 1;
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

static u_char *next_glyph_buf(XFontStruct *xfont, u_int code, u_int size) {
  u_char *glyph;
  u_int glyph_buf_size = GLYPH_TABLE_SIZE *
                         (GLYPH_HEADER_SIZE_MIN + xfont->glyph_width_bytes * xfont->height);

  if (glyph_buf_size > MAX_OFF + 1) {
    glyph_buf_size = MAX_OFF + 1;
  }

  if (xfont->glyph_buf_left < size) {
    if (xfont->num_glyph_bufs >= MAX_GLYPH_TABLES) {
      bl_msg_printf("Unable to show U+%x (glyph cache is full)\n", code);

      return NULL;
    }

    if (!(((u_char**)xfont->glyphs)[xfont->num_glyph_bufs] = calloc(1, glyph_buf_size))) {
      return NULL;
    }
    xfont->num_glyph_bufs++;
    xfont->glyph_buf_left = glyph_buf_size;
  }

  glyph = ((u_char**)xfont->glyphs)[xfont->num_glyph_bufs - 1] +
          (glyph_buf_size - xfont->glyph_buf_left);

  ((u_int32_t*)xfont->glyph_indeces)[code] =
    ((xfont->num_glyph_bufs - 1) << 23) + (glyph_buf_size - xfont->glyph_buf_left);

#if 0
  bl_debug_printf("New glyph %p id %x (=%x<<23+%x) size %d\n",
                  glyph, ((u_int32_t*)xfont->glyph_indeces)[code], xfont->num_glyph_bufs - 1,
                  glyph_buf_size - xfont->glyph_buf_left, size);
#endif

  xfont->glyph_buf_left -= size;

  return glyph;
}

static u_char *get_ft_bitmap_intern(XFontStruct *xfont, u_int32_t code /* glyph index */,
                                    u_int32_t ch) {
  int idx;
  u_char *glyph;

  if (code >= xfont->num_indeces) {
    u_int32_t *indeces;

    if (!(indeces = realloc(xfont->glyph_indeces, sizeof(u_int32_t) * (code + 1)))) {
      return NULL;
    }
    memset(indeces + xfont->num_indeces, 0,
           sizeof(u_int32_t) * (code + 1 - xfont->num_indeces));
    xfont->num_indeces = code + 1;
    xfont->glyph_indeces = indeces;
  }

  if (!(idx = ((u_int32_t*)xfont->glyph_indeces)[code])) {
    FT_Face face;
    int y;
    u_char *src;
    u_char *dst;
    int left_pitch;
    int pitch;
    int rows;
    int32_t format;
    int count;

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

#if 0
    bl_debug_printf("%x %c w %d %d(%d) h %d(%d) at %d %d\n", code, code, face->glyph->bitmap.width,
                    face->glyph->bitmap.pitch, xfont->glyph_width_bytes, face->glyph->bitmap.rows,
                    xfont->height, face->glyph->bitmap_left, face->glyph->bitmap_top);
#endif

    if (xfont->format & FONT_ROTATED) {
      if (!(format & FONT_ROTATED) || face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
        /* Glyph isn't rotated. */

        u_int w;

        if (xfont->is_aa) {
          w = face->glyph->bitmap.width / 3;
        } else {
          w = face->glyph->bitmap.width;
        }

        if (is_right_aligned_char(ch) && xfont->width > w + face->glyph->bitmap_left) {
          /* XXX Hack for vertical kutouten and sokuon */
          left_pitch += (xfont->width - w - face->glyph->bitmap_left - 1);
        } else if (face->glyph->bitmap_left + w <= xfont->width / 2) {
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
      y = 0;

      if ((rows = face->glyph->bitmap.rows) > xfont->height) {
        rows = xfont->height;
      }
    }

    if (face->glyph->bitmap_left > 0) {
      left_pitch += face->glyph->bitmap_left;
    }

    src = face->glyph->bitmap.buffer;

    if (xfont->is_aa) {
      left_pitch *= 3;

      if (xfont->has_each_glyph_width_info) {
        /* Storing glyph width position info. (ISCII or ISO10646_UCS4_1_V) */
        u_int tmp;

        pitch = face->glyph->bitmap.width; /* XXX Not face->glyph->bitmap.pitch */
        if (!(glyph = next_glyph_buf(xfont, code,
                                     GLYPH_HEADER_SIZE_MAX + (pitch + left_pitch) * (y + rows)))) {
          return NULL;
        }

        tmp = (face->glyph->advance.x >> 6);
        GLYPH_ADV(glyph) = BL_MIN(255, tmp);

        tmp = (pitch + left_pitch) / 3;
        GLYPH_WIDTH(glyph) = BL_MIN(255, tmp);

        if (face->glyph->bitmap_left < 0) {
          tmp = -face->glyph->bitmap_left;
          GLYPH_RETREAT(glyph) = BL_MIN(255, tmp);
        } else {
          GLYPH_RETREAT(glyph) = 0;
        }

        if (GLYPH_ADV(glyph) == 0 &&
            GLYPH_WIDTH(glyph) > /* GLYPH_ADV(glyph) + */ GLYPH_RETREAT(glyph)) {
          GLYPH_RETREAT(glyph) = GLYPH_WIDTH(glyph) /* - GLYPH_ADV(glyph) */;
        }

        dst = glyph + GLYPH_HEADER_SIZE_MAX + (pitch + left_pitch) * y;

#if 0
        bl_debug_printf("%x %c A %d R %d W %d-> A %d W %d R %d\n", code, code,
                        face->glyph->advance.x >> 6, face->glyph->bitmap_left,
                        face->glyph->bitmap.width, GLYPH_ADV(glyph), GLYPH_WIDTH(glyph),
                        GLYPH_RETREAT(glyph));
#endif
        for (count = 0; count < rows; count++) {
          memcpy(dst + left_pitch, src, pitch);

          src += face->glyph->bitmap.pitch;
          dst += (pitch + left_pitch);
        }
      } else {
        pitch = face->glyph->bitmap.pitch;

        if (face->glyph->bitmap_left < 0 && left_pitch == 0 &&
            !(xfont->format & FONT_ROTATED)) {
          /* For 'A' and 'W' (bitmap_left can be -1) of Inconsolata font */
          int left_pitch_minus = face->glyph->bitmap_left * 3;

          if (pitch + left_pitch_minus > 0) {
            src += -left_pitch_minus;
            pitch += left_pitch_minus;

            if (!(glyph = next_glyph_buf(xfont, code,
                                         GLYPH_HEADER_SIZE_MIN + pitch * (rows + y)))) {
              return NULL;
            }
            dst = glyph + GLYPH_HEADER_SIZE_MIN + (pitch * y);

            for (count = 0; count < rows; count++) {
              memcpy(dst, src, pitch);

              src += face->glyph->bitmap.pitch;
              dst += pitch;
            }
          } else {
            return NULL;
          }
        } else {
          if (!(glyph = next_glyph_buf(xfont, code,
                                       GLYPH_HEADER_SIZE_MIN +
                                       (left_pitch + pitch) * (rows + y)))) {
            return NULL;
          }

          dst = glyph + GLYPH_HEADER_SIZE_MIN + (left_pitch + pitch) * y;

          for (count = 0; count < rows; count++) {
            memcpy(dst + left_pitch, src, pitch);

            src += face->glyph->bitmap.pitch;
            dst += (left_pitch + pitch);
          }
        }
      }
    } else {
      pitch = face->glyph->bitmap.pitch;

      if (!(glyph = next_glyph_buf(xfont, code,
                                   GLYPH_HEADER_SIZE_MIN +
                                   ((left_pitch + 7) / 8 + pitch) * (rows + y)))) {
        return NULL;
      }

      if (left_pitch == 0) {
        dst = glyph + GLYPH_HEADER_SIZE_MIN + pitch * y;

        for (count = 0; count < rows; count++) {
          memcpy(dst, src, pitch);
          src += face->glyph->bitmap.pitch;
          dst += pitch;
        }
      } else {
        int shift;
        int x;

        if ((shift = left_pitch / 8) > 0) {
          left_pitch -= (shift * 8);
        }

        dst = glyph + GLYPH_HEADER_SIZE_MIN + (shift + (left_pitch > 0 ? 1 : 0) + pitch) * y;

        /* XXX TODO: Support face->glyph->bitmap_left < 0 */
        for (count = 0; count < rows; count++) {
          dst += shift;

          dst[0] = (src[0] >> left_pitch);
          for (x = 1; x < pitch; x++) {
            dst[x] = (src[x - 1] << (8 - left_pitch)) | (src[x] >> left_pitch);
          }
          dst[x] = (src[x - 1] << (8 - left_pitch));

          src += face->glyph->bitmap.pitch;
          dst += (x + 1);
        }

        left_pitch = shift + (left_pitch > 0 ? 1 : 0);
      }
    }

    /* left_pitch + pitch is stored in 16bits */
    glyph[0] = (left_pitch + pitch) >> 8;
    glyph[1] = (left_pitch + pitch) & 0xff;

    /* Storing glyph height position info. */
    if (face->glyph->bitmap_top < 0 || (xfont->format & FONT_ROTATED)) {
      glyph[2] = 0;
    } else {
      glyph[2] = BL_MIN(255, face->glyph->bitmap_top + y);
    }

    glyph[3] = BL_MIN(255, rows + y);
  } else {
    glyph = ((u_char**)xfont->glyphs)[SEG(idx)] + OFF(idx);
#if 0
    bl_debug_printf("Get cached glyph %p idx %x (seg %x off %x)\n", glyph, idx, SEG(idx), OFF(idx));
#endif
  }

  return glyph;
}

static int load_xfont(XFontStruct *xfont, const char *file_path, int32_t format,
                      ef_charset_t cs, int is_aa) {
  if (!is_pcf(file_path)) {
    return load_ft(xfont, file_path, format, is_aa, 0, 0);
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

static void compl_xfonts_destroy(XFontStruct **xfonts) {
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

  p = font_name + strlen(font_name) - 1;
  if ('0' <= *p && *p <= '9') {
    do {
      p--;
    } while ('0' <= *p && *p <= '9');

    if (*p == ':' && bl_str_to_uint(percent, p + 1)) {
      /* Parsing ":[Percentage]" */
      *p = '\0';
    }
  }

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

        for (count = 0; count < BL_ARRAY_SIZE(styles); count++) {
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

  if (family && (pattern = FcNameParse(family))) {
    /* do nothing */
  } else if ((pattern = FcPatternCreate())) {
    if (family) {
      FcPatternAddString(pattern, FC_FAMILY, family);
    }
  } else {
    return NULL;
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
    char *p;

    if ((p = alloca(strlen(fontname) + 1))) {
      parse_fc_font_name(&family, is_bold, is_italic, percent, strcpy(p, fontname));
    }
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

      /*
       * XXX
       * FcFontList() is not used.
       * If you want to match complementary fonts, add <match target="pattern">...</match>
       * to ~/.fonts.conf
       */
#if 0
      {
        FcPattern *pat = FcPatternCreate();
        FcObjectSet *objset = FcObjectSetBuild(FC_FAMILY, NULL);
        FcFontSet *fontset = FcFontList(NULL, pat, objset);
        FcValue val;
        int count;

        for (count = 0; count < fontset->nfont; count++) {
          FcPatternGet(fontset->fonts[count], FC_FAMILY, 0, &val);

          if (!is_same_family(compl_pattern, val.u.s)) {
            FcPatternAdd(compl_pattern, FC_FAMILY, val, FcTrue /* append */);
            num_fc_files++;
#if 0
            bl_debug_printf("Add Font List %s [%]\n", val.u.s);
#endif
          }
        }
      }
#endif
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
                             int get_space_glyph, XFontStruct **compl_xfont) {
#ifdef USE_FONTCONFIG
  u_int count;
#endif
  u_char *bitmap;
  u_int32_t code;

  if (!use_ot_layout) {
    /* char => glyph index */
    if (!get_space_glyph && ch == 0x20) {
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
  /* Complementary glyphs are searched only if xfont->face && !use_ot_layout. */
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
        if (!(compl = get_cached_xfont(fc_files[count], xfont->format, xfont->is_aa))) {
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
          else if (!load_ft(compl, fc_files[count], xfont->format, xfont->is_aa,
                            xfont->height, xfont->ascent)) {
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

      if (xfont->has_each_glyph_width_info && !compl->has_each_glyph_width_info) {
        clear_glyph_cache_ft(compl);
        enable_position_info_by_glyph(compl);
      }

      if ((code = get_glyph_index(compl->face, ch)) > 0) {
        if ((bitmap = get_ft_bitmap_intern(compl, code, ch))) {
          if (compl_xfont) {
            *compl_xfont = compl;
          }
#ifdef __DEBUG
          bl_debug_printf("Use %s font to show U+%x\n", compl->file, ch);
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

#define load_xfont(xfont, file_path, format, cs, is_aa) load_pcf(xfont, file_path)
#define unload_xfont(xfont) unload_pcf(xfont)

#endif /* USE_FREETYPE */

static XFontStruct *get_cached_xfont(const char *file, int32_t format, int is_aa) {
  u_int count;

  for (count = 0; count < num_xfonts; count++) {
    if (strcmp(xfonts[count]->file, file) == 0
#ifdef USE_FREETYPE
        && (xfonts[count]->face == NULL || xfonts[count]->format == format)
        && xfonts[count]->is_aa == is_aa
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
    compl_xfonts_destroy(xfont->compl_xfonts);
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
                       u_int col_width, int use_medium_for_bold, int letter_space) {
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
  int is_aa;

  cs = FONT_CS(id);

#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  if (use_fontconfig) {
    FcPattern *pattern;
    FcValue val;
    int is_bold;
    int is_italic;
    char *p;

#ifdef USE_WIN32API
    init_fontconfig();
#endif

    if (!(pattern = parse_font_name(fontname, &is_bold, &is_italic, &percent, cs))) {
      return NULL;
    }

    if (FcPatternGet(pattern, FC_FILE, 0, &val) != FcResultMatch) {
      FcPatternDestroy(pattern);

      return NULL;
    }

    if ((font_file = alloca(strlen(val.u.s) + 1))) {
      strcpy(font_file, val.u.s);
    }

    /* For 'format = fontsize | (id & (FONT_BOLD | FONT_ITALIC))' below */
    if ((p = strcasestr(font_file, "bold")) &&
        strchr(p, '/') == NULL && strchr(p, '\\') == NULL) {
      id &= ~FONT_BOLD; /* XXX e.g. Inconsolata-Bold.ttf => Don't call FT_Outline_Embolden(). */
    } else if (is_bold) {
      id |= FONT_BOLD;
    }
    if ((p = strcasestr(font_file, "italic")) &&
        strchr(p, '/') == NULL && strchr(p, '\\') == NULL) {
      id &= ~FONT_ITALIC; /* XXX */
    } else if (is_italic) {
      id |= FONT_ITALIC;
    }

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
              if ((cand = alloca(strlen(entry->d_name) + 1))) {
                strcpy(cand, entry->d_name);
              }
            } else if (strcasestr(entry->d_name, "Mono")) {
              if ((cand = alloca(strlen(entry->d_name) + 1))) {
                strcpy(cand, entry->d_name);
              }
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

    if (!(percent_str = alloca(strlen(fontname) + 1))) {
      return NULL;
    }
    strcpy(percent_str, fontname);

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

  if (!(font_present & FONT_NOAA) && display->bytes_per_pixel > 1) {
    is_aa = 1;
  } else {
    is_aa = 0;
  }

#ifdef USE_FREETYPE
  if ((font->xfont = get_cached_xfont(decsp_id ? decsp_id : font_file, format, is_aa)))
#else
  if ((font->xfont = get_cached_xfont(decsp_id ? decsp_id : font_file, 0, is_aa)))
#endif
  {
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
  } else if (!load_xfont(font->xfont, font_file, format, cs, is_aa)) {
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
  if (font->xfont->has_each_glyph_width_info) {
    font->is_proportional = 1;
  }

  if (IS_ISCII(cs) && font->xfont->is_aa &&
      (font->xfont->ref_count == 1 || font->xfont->has_each_glyph_width_info)) {
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
    if (font->xfont->face) {
      font->is_proportional = 1;

      if (font->xfont->ref_count == 1) {
        enable_position_info_by_glyph(font->xfont);
      } else if (!font->xfont->has_each_glyph_width_info) {
        clear_glyph_cache_ft(font->xfont);
        enable_position_info_by_glyph(font->xfont);
      }
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

    if (!font->is_var_col_width) {
      u_int ch_width;

      if (percent > 0) {
        if (font->is_vertical) {
          /* The width of full and half character font is the same. */
          letter_space *= 2;
          ch_width = DIVIDE_ROUNDING(fontsize * percent, 100);
        } else {
          ch_width = DIVIDE_ROUNDING(fontsize * percent, 200);
        }
      } else {
        ch_width = font->width;

        if (font->is_vertical) {
          /* The width of full and half character font is the same. */
          letter_space *= 2;
#ifdef USE_FREETYPE
          if (!font->xfont->face || !(font->xfont->format & FONT_ROTATED))
#endif
          {
            ch_width *= 2;
          }
        }
      }

      if (letter_space > 0 || ch_width > -letter_space) {
        ch_width += letter_space;
      }

      if (font->width != ch_width) {
        if (font->width < ch_width) {
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
          font->x_off = (ch_width - font->width) / 2;
        }

        font->width = ch_width;
      }
    }
  } else {
    /* not a standard(usascii) font */

    /*
     * XXX hack
     * forcibly conforming non standard font width to standard font width.
     */

    if (font->is_vertical) {
      /*
       * The width of full and half character font is the same.
       * is_var_col_width is always false if is_vertical is true.
       */
      if (font->width != col_width) {
#ifdef DEBUG
        bl_msg_printf("Font(id %x) width(%d) doesn't fit column width(%d).\n",
                      font->id, font->width, col_width);
#endif

        if (font->width < col_width) {
          font->x_off = (col_width - font->width) / 2;
        }

        font->width = col_width;
      }
    } else if (!font->is_var_col_width) {
      if (font->width != col_width * cols) {
#ifdef DEBUG
        bl_msg_printf("Font(id %x) width(%d) doesn't fit column width(%d).\n",
                      font->id, font->width, col_width * cols);
#endif

        if (font->width < col_width * cols) {
          font->x_off = (col_width * cols - font->width) / 2;
        }

        font->width = col_width * cols;
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

  if (
#ifdef USE_FREETYPE
      !(font->xfont->format & FONT_ROTATED) &&
#endif
      font->ascent == 0) {
    /* XXX this may be inaccurate. */
    font->ascent = fontsize;
  }

  bl_msg_printf("Load %s (id %x%s)\n", font->xfont->file, font->id,
                font->xfont->ref_count > 1 ? ", cached" : "");
#ifdef DEBUG
  bl_debug_printf("=> Max %d bytes/glyph line, height %d, CURRENT NUM OF XFONTS %d\n",
                  font->xfont->glyph_width_bytes, font->xfont->height, num_xfonts);
#endif

#ifdef DEBUG
  ui_font_dump(font);
#endif

  return font;
}

void ui_font_destroy(ui_font_t *font) {
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
      if (font->ot_font_not_found) {
        return 0;
      }

      if (!(font->ot_font = otl_open(font->xfont->face))) {
        font->ot_font_not_found = 1;

        return 0;
      }

      if (!font->xfont->has_each_glyph_width_info) {
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

u_int ui_convert_text_to_glyphs(ui_font_t *font, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                u_int32_t *noshape_glyphs, u_int32_t *src, u_int src_len,
                                const char *script, const char *features) {
#ifdef USE_FREETYPE
  return otl_convert_text_to_glyphs(font->ot_font, shape_glyphs, num_shape_glyphs,
                                    xoffsets, yoffsets, advances, noshape_glyphs,
                                    src, src_len, script, features,
                                    FONTSIZE_IN_FORMAT(font->xfont->format) *
                                    (font->size_attr >= DOUBLE_WIDTH ? 2 : 1));
#else
  return 0;
#endif
}
#endif

u_int ui_calculate_char_width(ui_font_t *font, u_int32_t ch, ef_charset_t cs, int is_awidth,
                              int *draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

#if defined(USE_FREETYPE)
  if (font->xfont->is_aa && font->is_proportional && font->is_var_col_width) {
    u_char *glyph;

#ifdef USE_OT_LAYOUT
    if ((glyph = get_ft_bitmap(font->xfont, ch,
                               (font->use_ot_layout /* && font->ot_font */),
                               1 /* is_proportional && is_var_col_width (See draw_string()) */,
                               NULL)))
#else
    if ((glyph = get_ft_bitmap(font->xfont, ch, 0,
                               1 /* is_proportional && is_var_col_width (See draw_string()) */,
                               NULL)))
#endif
    {
      return GLYPH_ADV(glyph);
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

#ifdef USE_FREETYPE
/*
 * Returned bitmaps have following headers.
 * xfont->is_aa && xfont->has_each_glyph_width_info
 * -> 3 bytes (1st byte: Adv, 2nd byte: Retreat, 3rd byte: Width)
 *
 * xfont->is_aa && !xfont->has_each_glyph_width_info
 * -> 4 bytes (1st and 2nd bytes: Pitch, 3rd and 4th bytes: Width (Pitch / 3))
 *
 * !xfont->is_aa
 * -> No header
 */
int ui_modify_bitmaps(XFontStruct *xfont, u_char **bitmaps, u_int num,
                      u_int *height, u_int *ascent) {
  static u_char *new_bitmaps;
  static size_t new_bitmaps_len;
  u_int line_width_bytes;
  u_int count;
  u_int a;
  u_int h;
  u_int dst_pitch;
  u_char *src;
  u_char *dst;
  u_int len;

  if (!xfont->face) {
    return 1;
  }

  line_width_bytes = 0;
  for (count = 0; count < num; count++) {
    if (bitmaps[count] == NULL) {
      continue;
    }

    a = bitmaps[count][2];
    h = bitmaps[count][3];

    if (*ascent < a) {
      *height += (a - *ascent);
      *ascent = a;
    }

#if 0
    if (h > a && *height - *ascent < h - a) {
      *height += ((h - a) - (*height - *ascent));
    }
#else
    /* Same as above */
    if (*height + a < h + *ascent) {
      *height += ((h + *ascent) - (*height + a));
    }
#endif

    if (xfont->is_aa) {
      if (xfont->has_each_glyph_width_info) {
        line_width_bytes += (GLYPH_HEADER_SIZE_MAX - GLYPH_HEADER_SIZE_MIN +
                             (GLYPH_WIDTH(bitmaps[count]) * 3));
      } else {
        line_width_bytes += (GLYPH_HEADER_SIZE_MIN +
                             ((bitmaps[count][0] << 8) | bitmaps[count][1]));
      }
    } else {
      line_width_bytes += xfont->glyph_width_bytes;
    }
  }

  if (line_width_bytes == 0) {
    return 1;
  }

  len = line_width_bytes * (*height);

  if (new_bitmaps_len < len) {
    void *p;

    if ((p = realloc(new_bitmaps, len)) == NULL) {
      return 0;
    }

    new_bitmaps = p;
    new_bitmaps_len = len;
  }

  dst = new_bitmaps;

  if (xfont->is_aa) {
    for (count = 0; count < num; count++) {
      u_char *orig_bitmap = bitmaps[count];

      if (orig_bitmap == NULL) {
        continue;
      }

      a = orig_bitmap[2];
      h = orig_bitmap[3];

      bitmaps[count] = dst;

      if (xfont->has_each_glyph_width_info) {
        dst_pitch = GLYPH_WIDTH(orig_bitmap) * 3;
        src = orig_bitmap + GLYPH_HEADER_SIZE_MAX;

        /* GLYPH_HEADER_SIZE_MIN is not copied */
        memcpy(dst, orig_bitmap + GLYPH_HEADER_SIZE_MIN,
               GLYPH_HEADER_SIZE_MAX - GLYPH_HEADER_SIZE_MIN);
        dst += (GLYPH_HEADER_SIZE_MAX - GLYPH_HEADER_SIZE_MIN);
      } else {
        dst_pitch = (orig_bitmap[0] << 8) | orig_bitmap[1];

        ((u_int16_t*)dst)[0] = dst_pitch;
        ((u_int16_t*)dst)[1] = dst_pitch / 3;
        dst += GLYPH_HEADER_SIZE_MIN;

        src = orig_bitmap + GLYPH_HEADER_SIZE_MIN;
      }

      if (*ascent > a) {
        len = (*ascent - a) * dst_pitch;
        dst = memset(dst, 0, len) + len;
      }

      len = dst_pitch * h;
      dst = memcpy(dst, src, len) + len;

      len = (*height - h - (*ascent - a)) * dst_pitch;
      dst = memset(dst, 0, len) + len;
    }
  } else {
    dst_pitch = xfont->glyph_width_bytes;

    for (count = 0; count < num; count++) {
      int y;
      u_int copy_len;
      u_int src_pitch;
      u_char *orig_bitmap = bitmaps[count];

      if (orig_bitmap == NULL) {
        continue;
      }

      a = orig_bitmap[2];
      h = orig_bitmap[3];

      bitmaps[count] = dst;

      /* GLYPH_HEADER_SIZE_MIN is not copied */
      src = orig_bitmap + GLYPH_HEADER_SIZE_MIN;

      if (*ascent > a) {
        len = (*ascent - a) * dst_pitch;
        dst = memset(dst, 0, len) + len;
      }

      src_pitch = (orig_bitmap[0] << 8) | orig_bitmap[1];
      copy_len = BL_MIN(src_pitch, dst_pitch);

      for (y = 0; y < h; y++) {
        memcpy(dst, src, copy_len);
        memset(dst + copy_len, 0, dst_pitch - copy_len);
        src += src_pitch;
        dst += dst_pitch;
      }

      len = (*height - h - (*ascent - a)) * dst_pitch;
      dst = memset(dst, 0, len) + len;
    }
  }

  return 1;
}
#else
int ui_modify_bitmaps(XFontStruct *xfont, u_char **bitmaps, u_int num,
                      u_int *height, u_int *ascent) {
  /* XXX Not implemented yet. */
  return 1;
}
#endif

u_char *ui_get_bitmap(XFontStruct *xfont, u_char *ch, size_t len, int use_ot_layout,
                      int get_space_glyph, XFontStruct **compl_xfont) {
  size_t ch_idx;
  int16_t glyph_idx;
  int32_t glyph_offset;

#ifdef USE_FREETYPE
  if (xfont->face) {
    return get_ft_bitmap(xfont, ef_bytes_to_int(ch, len), use_ot_layout, get_space_glyph,
                         compl_xfont);
  } else
#endif
  if (len == 1) {
    if (!get_space_glyph && ch[0] == 0x20) {
      return NULL;
    }

    ch_idx = ch[0] - xfont->min_char_or_byte2;
  } else if (len == 2) {
    ch_idx =
        (ch[0] - xfont->min_byte1) * (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) +
        ch[1] - xfont->min_char_or_byte2;
  } else /* if( len == 4) */ {
    ch_idx = (ch[1] * 0x100 + ch[2] - xfont->min_byte1) *
                 (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) +
             ch[3] - xfont->min_char_or_byte2;
  }

  if (ch_idx >= (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1)*(xfont->max_byte1 -
                                                                           xfont->min_byte1 + 1) ||
      (glyph_idx = ((int16_t*)xfont->glyph_indeces)[ch_idx]) == -1) {
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
