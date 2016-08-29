/*
 *	$Id$
 */

#include "ui_font.h"

#include <stdio.h>
#include <fcntl.h>    /* open */
#include <unistd.h>   /* close */
#include <sys/mman.h> /* mmap */
#include <string.h>   /* memcmp */
#include <sys/stat.h> /* fstat */
#include <utime.h>    /* utime */

#include <pobl/bl_def.h> /* WORDS_BIGENDIAN */
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>    /* strdup */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_conf_io.h>/* bl_get_user_rc_path */
#include <pobl/bl_util.h>   /* TOINT32 */
#include <mef/ef_char.h>
#ifdef __ANDROID__
#include <dirent.h>
#endif

#ifdef USE_OT_LAYOUT
#include <otl.h>
#endif

#define DIVIDE_ROUNDING(a, b) (((int)((a)*10 + (b)*5)) / ((int)((b)*10)))
#define DIVIDE_ROUNDINGUP(a, b) (((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)))

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

/* ===== PCF ===== */

/* --- static variables --- */

static XFontStruct** xfonts;
static u_int num_of_xfonts;

/* --- static functions --- */

static int load_bitmaps(XFontStruct* xfont, u_char* p, size_t size, int is_be, int glyph_pad_type) {
  int32_t* offsets;
  int32_t bitmap_sizes[4];
  int32_t count;

  /* 0 -> byte , 1 -> short , 2 -> int */
  xfont->glyph_width_bytes = (glyph_pad_type == 2 ? 4 : (glyph_pad_type == 1 ? 2 : 1));

  xfont->num_of_glyphs = _TOINT32(p, is_be);
  p += 4;

  if (size < 8 + sizeof(*offsets) * xfont->num_of_glyphs) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " size %d is too small.\n", size);
#endif

    return 0;
  }

  if (!(xfont->glyph_offsets = malloc(sizeof(*offsets) * xfont->num_of_glyphs))) {
    return 0;
  }

#ifdef WORDS_BIGENDIAN
  if (is_be)
#else
  if (!is_be)
#endif
  {
    memcpy(xfont->glyph_offsets, p, sizeof(*offsets) * xfont->num_of_glyphs);
    p += (sizeof(*offsets) * xfont->num_of_glyphs);
  } else {
    for (count = 0; count < xfont->num_of_glyphs; count++) {
      xfont->glyph_offsets[count] = _TOINT32(p, is_be);
      p += 4;
    }
  }

  for (count = 0; count < 4; count++) {
    bitmap_sizes[count] = _TOINT32(p, is_be);
    p += 4;
  }

  if (size < 8 + sizeof(*offsets) * xfont->num_of_glyphs + 16 + bitmap_sizes[glyph_pad_type]) {
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
  bl_debug_printf(BL_DEBUG_TAG "GLYPH COUNT %d x WIDTH BYTE %d = SIZE %d\n", xfont->num_of_glyphs,
                  xfont->glyph_width_bytes, bitmap_sizes[glyph_pad_type]);

  {
    FILE* fp;

    p = xfont->glyphs;

    fp = fopen("log.txt", "w");

    for (count = 0; count < xfont->num_of_glyphs; count++) {
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

static int load_encodings(XFontStruct* xfont, u_char* p, size_t size, int is_be) {
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
    int16_t* p;

    p = xfont->glyph_indeces;

    for (count = xfont->min_char_or_byte2; count <= xfont->max_char_or_byte2; count++) {
      bl_msg_printf("%d %x\n", count, (int)*p);
      p++;
    }
  }
#endif

  return 1;
}

static int get_metrics(u_int8_t* width, u_int8_t* width_full, u_int8_t* height, u_int8_t* ascent,
                       u_char* p, size_t size, int is_be, int is_compressed) {
  int16_t num_of_metrics;

  /* XXX Proportional font is not considered. */

  if (is_compressed) {
    num_of_metrics = _TOINT16(p, is_be);
    p += 2;

    *width = p[2] - 0x80;
    *ascent = p[3] - 0x80;
    *height = *ascent + (p[4] - 0x80);

    if (num_of_metrics > 0x3000) {
      /* U+3000: Unicode ideographic space (Full width) */
      p += (5 * 0x3000);
      *width_full = p[2] - 0x80;
    } else {
      *width_full = 0;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " COMPRESSED METRICS %d %d %d %d %d\n", num_of_metrics, *width,
                    *width_full, *height, *ascent);
#endif
  } else {
    num_of_metrics = _TOINT32(p, is_be);
    p += 4;

    /* skip {left|right}_sided_bearing */
    p += 4;

    *width = _TOINT16(p, is_be);
    p += 2;

    *ascent = _TOINT16(p, is_be);
    p += 2;

    *height = *ascent + _TOINT16(p, is_be);

    if (num_of_metrics > 0x3000) {
      /* skip character_descent and character attributes */
      p += 4;

      /* U+3000: Unicode ideographic space (Full width) */
      p += (12 * 0x2999);

      /* skip {left|right}_sided_bearing */
      p += 4;

      *width_full = _TOINT16(p, is_be);
    } else {
      *width_full = 0;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " NOT COMPRESSED METRICS %d %d %d %d %d\n", num_of_metrics, *width,
                    *width_full, *height, *ascent);
#endif
  }

  return 1;
}

static char* gunzip(const char* file_path, struct stat* st) {
  size_t len;
  char* new_file_path;
  struct stat new_st;
  char* cmd;
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

static int load_pcf(XFontStruct* xfont, const char* file_path) {
  char* uzfile_path;
  int fd;
  struct stat st;
  u_char* pcf = NULL;
  u_char* p;
  int32_t num_of_tables;
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

  num_of_tables = _TOINT32(p, 0);
  p += 4;

  if (st.st_size <= 8 + 16 * num_of_tables) {
    goto end;
  }

  for (count = 0; count < num_of_tables; count++) {
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
    u_char* bitmap;
    int i;
    int j;

    if ((bitmap = ui_get_bitmap(xfont, ch, sizeof(ch) - 1))) {
      for (j = 0; j < xfont->height; j++) {
        u_char* line;

        ui_get_bitmap_line(xfont, bitmap, j, line);

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

static void unload_pcf(XFontStruct* xfont) {
  free(xfont->file);
  free(xfont->glyphs);
  free(xfont->glyph_offsets);
  free(xfont->glyph_indeces);
}

/* ===== FREETYPE ===== */

#ifdef USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

/* 0 - 511 */
#define SEG(idx) (((idx) >> 7) & 0x1ff)
/* 0 - 127 */
#define OFF(idx) ((idx)&0x7f)
/* +3 is for storing glyph position info. */
#define IS_PROPORTIONAL(xfont) \
  ((xfont)->glyph_size == (xfont)->glyph_width_bytes * (xfont)->height + 3)

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
  if (is_aa) {
    FT_Load_Glyph(face, code, FT_LOAD_NO_BITMAP);

    if (face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
      return 0;
    }
  } else {
    FT_Load_Glyph(face, code, 0);
  }

  if (format & FONT_ITALIC) {
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

  return 1;
}

static int load_ft(XFontStruct* xfont, const char* file_path, int32_t format, int is_aa) {
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
  }

  fontsize = (format & ~(FONT_BOLD | FONT_ITALIC));

  for (count = 0; count < num_of_xfonts; count++) {
    if (strcmp(xfonts[count]->file, file_path) == 0 &&
        (xfonts[count]->format & ~(FONT_BOLD | FONT_ITALIC)) == fontsize) {
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
  FT_Set_Pixel_Sizes(face, fontsize, fontsize);

  xfont->format = format;
  xfont->face = face;
  xfont->is_aa = is_aa;

  if (!load_glyph(face, format, get_glyph_index(face, 'W'), is_aa)) {
    bl_msg_printf("%s doesn't have outline glyphs.\n", file_path);

    goto error;
  }

  xfont->num_of_indeces = INITIAL_GLYPH_INDEX_TABLE_SIZE;

  if (!(xfont->file = strdup(file_path)) ||
      !(xfont->glyph_indeces = calloc(xfont->num_of_indeces, sizeof(u_int16_t))) ||
      !(xfont->glyphs = calloc(MAX_GLYPH_TABLES, sizeof(u_char*)))) {
    goto error;
  }

  face->generic.data = ((int)face->generic.data) + 1; /* ref_count */

  xfont->width_full =
      (face->max_advance_width * face->size->metrics.x_ppem + face->units_per_EM - 1) /
      face->units_per_EM;
  if (is_aa) {
    xfont->glyph_width_bytes = xfont->width_full * 3;
    xfont->width = face->glyph->bitmap.width / 3;
  } else {
    xfont->glyph_width_bytes = (xfont->width_full + 7) / 8;
    xfont->width = face->glyph->bitmap.width;
  }

  xfont->height = (face->max_advance_height * face->size->metrics.y_ppem + face->units_per_EM - 1) /
                  face->units_per_EM;
  xfont->ascent =
      (face->ascender * face->size->metrics.y_ppem + face->units_per_EM - 1) / face->units_per_EM;

  if (load_glyph(face, format, get_glyph_index(face, 'j'), is_aa)) {
    int descent;

    descent = face->glyph->bitmap.rows - face->glyph->bitmap_top;
    if (descent > xfont->height - xfont->ascent) {
      xfont->height = xfont->ascent + descent;
    }
  }

  if (is_aa) {
    xfont->glyph_size = xfont->glyph_width_bytes * xfont->height;
  } else {
    /* +1 is for the last 'dst[count] = ...' in get_ft_bitmap(). */
    xfont->glyph_size = xfont->glyph_width_bytes * xfont->height + 1;
  }

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

static void clear_glyph_cache_ft(XFontStruct* xfont) {
  int count;

  for (count = 0; ((u_char**)xfont->glyphs)[count]; count++) {
    free(((u_char**)xfont->glyphs)[count]);
  }

  memset(xfont->glyphs, 0, MAX_GLYPH_TABLES * sizeof(u_char*));
  xfont->num_of_glyphs = 0;
  memset(xfont->glyph_indeces, 0, xfont->num_of_indeces * sizeof(u_int16_t));
}

static void unload_ft(XFontStruct* xfont) {
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

  if (num_of_xfonts == 0 && library) {
    FT_Done_FreeType(library);
    library = NULL;
  }
}

static u_char* get_ft_bitmap(XFontStruct* xfont, u_int32_t code, int use_ot_layout) {
  u_int16_t* indeces;
  int idx;
  u_char** glyphs;
  u_char* glyph;

  if (!use_ot_layout) {
    if (code == 0x20) {
      return NULL;
    }

    code = get_glyph_index(xfont->face, code);
  }

  if (code >= xfont->num_of_indeces) {
    if (!(indeces = realloc(xfont->glyph_indeces, sizeof(u_int16_t) * (code + 1)))) {
      return NULL;
    }
    memset(indeces + xfont->num_of_indeces, 0,
           sizeof(u_int16_t) * (code + 1 - xfont->num_of_indeces));
    xfont->num_of_indeces = code + 1;
    xfont->glyph_indeces = indeces;
  } else {
    indeces = xfont->glyph_indeces;
  }

  glyphs = xfont->glyphs;

  if (!(idx = indeces[code])) {
    FT_Face face;
    int y;
    u_char* src;
    u_char* dst;
    int left_pitch;
    int pitch;
    int rows;

    if (xfont->num_of_glyphs >= GLYPH_TABLE_SIZE * MAX_GLYPH_TABLES - 1) {
      bl_msg_printf("Unable to show U+%x because glyph cache is full.\n", code);

      return NULL;
    }

    face = xfont->face;

    if (!load_glyph(face, xfont->format, code, xfont->is_aa)) {
      return NULL;
    }

    if (OFF(xfont->num_of_glyphs) == 0) {
      if (!(glyphs[SEG(xfont->num_of_glyphs)] = calloc(GLYPH_TABLE_SIZE, xfont->glyph_size))) {
        return NULL;
      }
    }

    idx = ++xfont->num_of_glyphs;

#if 0
    bl_debug_printf("%x %c w %d %d(%d) h %d(%d) at %d %d\n", code, code, face->glyph->bitmap.width,
                    face->glyph->bitmap.pitch, xfont->glyph_width_bytes, face->glyph->bitmap.rows,
                    xfont->height, face->glyph->bitmap_left, face->glyph->bitmap_top);
#endif

    indeces[code] = idx;

    if (xfont->is_aa) {
      if ((left_pitch = face->glyph->bitmap_left * 3) < 0) {
        left_pitch = 0;
      }

      if (face->glyph->bitmap.pitch < xfont->glyph_width_bytes) {
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

        /* XXX left_pitch is 7 at most. */
        if ((left_pitch = face->glyph->bitmap_left) > 7) {
          left_pitch = 7;
        } else if (left_pitch < 0) {
          left_pitch = 0;
        }
      } else {
        pitch = xfont->glyph_width_bytes;
        left_pitch = 0;
      }
    }

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

    glyph = glyphs[SEG(idx - 1)] + xfont->glyph_size * OFF(idx - 1);

    src = face->glyph->bitmap.buffer;
    dst = glyph + (xfont->glyph_width_bytes * y);

    if (xfont->is_aa) {
      for (y = 0; y < rows; y++) {
        memcpy(dst + left_pitch, src, pitch);

        src += face->glyph->bitmap.pitch;
        dst += xfont->glyph_width_bytes;
      }

      if (IS_PROPORTIONAL(xfont)) {
        /* Storing glyph position info. (ISCII or ISO10646_UCS4_1_V) */

        dst = glyph + xfont->glyph_size - 3;

        dst[0] = (face->glyph->advance.x >> 6);                /* advance */
        dst[2] = (face->glyph->bitmap.width + left_pitch) / 3; /* width */
        if (dst[2] > xfont->width_full) {
          dst[2] = xfont->width_full; /* == glyph_width_bytes / 3 */
        }

        if (face->glyph->bitmap_left < 0) {
          dst[1] = -face->glyph->bitmap_left; /* retreat */
        } else {
          dst[1] = 0;
        }

        if (dst[0] == 0 && dst[2] > dst[0] + dst[1]) {
          dst[1] = dst[2] - dst[0]; /* retreat */
        }

#if 0
        bl_debug_printf("%x %c A %d R %d W %d-> A %d R %d W %d\n", code, code,
                        face->glyph->advance.x >> 6, face->glyph->bitmap_left,
                        face->glyph->bitmap.width, dst[0], dst[1], dst[2]);
#endif
      }
    } else {
      for (y = 0; y < rows; y++) {
        int count;

        if (left_pitch == 0) {
          memcpy(dst, src, pitch);
        } else {
          dst += (left_pitch / 8);
          dst[0] = (src[0] >> left_pitch);
          for (count = 1; count < pitch; count++) {
            dst[count] = (src[count - 1] << (8 - left_pitch)) | (src[count] >> left_pitch);
          }
          dst[count] = (src[count - 1] << (8 - left_pitch));
        }

        src += face->glyph->bitmap.pitch;
        dst += xfont->glyph_width_bytes;
      }
    }
  } else {
    glyph = glyphs[SEG(idx - 1)] + xfont->glyph_size * OFF(idx - 1);
  }

  return glyph;
}

static int load_xfont(XFontStruct* xfont, const char* file_path, int32_t format,
                      u_int bytes_per_pixel, ef_charset_t cs) {
  if ((cs == ISO10646_UCS4_1 || IS_ISCII(cs)) &&
      strcasecmp(file_path + strlen(file_path) - 6, "pcf.gz") != 0 &&
      strcasecmp(file_path + strlen(file_path) - 3, "pcf") != 0) {
    return load_ft(xfont, file_path, format, (bytes_per_pixel > 1));
  } else {
    return load_pcf(xfont, file_path);
  }
}

static void unload_xfont(XFontStruct* xfont) {
  if (xfont->face) {
    unload_ft(xfont);
  } else {
    unload_pcf(xfont);
  }
}

#else

#define load_xfont(xfont, file_path, format, bytes_per_pixel, cs) load_pcf(xfont, file_path)
#define unload_xfont(xfont) unload_pcf(xfont)

#endif /* USE_FREETYPE */

/* --- global functions --- */

int ui_compose_dec_special_font(void) {
  /* Do nothing for now in fb. */
  return 0;
}

ui_font_t* ui_font_new(Display* display, vt_font_t id, int size_attr, ui_type_engine_t type_engine,
                       ui_font_present_t font_present, const char* fontname, u_int fontsize,
                       u_int col_width, int use_medium_for_bold,
                       u_int letter_space /* Ignored for now. */
                       ) {
  ef_charset_t cs;
  char* font_file;
  u_int percent;
  ui_font_t* font;
  void* p;
  u_int count;
#ifdef USE_FREETYPE
  u_int format;
#endif

  cs = FONT_CS(id);

  if (!fontname) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Font file is not specified.\n");
#endif

    if (IS_ISO10646_UCS4(cs) || cs == ISO8859_1_R) {
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
        struct dirent* entry;
        const char* cand;

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

      if (id & FONT_BOLD) {
        use_medium_for_bold = 1;
      }
    } else {
      return NULL;
    }
  } else {
    char* percent_str;

    if (!(percent_str = bl_str_alloca_dup(fontname))) {
      return NULL;
    }

    font_file = bl_str_sep(&percent_str, ":");

    if (!percent_str || !bl_str_to_uint(&percent, percent_str)) {
      percent = 0;
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
  if (percent > 0) {
    format = DIVIDE_ROUNDING(fontsize * percent, 100) | (id & (FONT_BOLD | FONT_ITALIC));
  } else {
    format = fontsize | (id & (FONT_BOLD | FONT_ITALIC));
  }
#endif

  for (count = 0; count < num_of_xfonts; count++) {
    if (strcmp(xfonts[count]->file, font_file) == 0
#ifdef USE_FREETYPE
        && xfonts[count]->face && xfonts[count]->format == format
#endif
        ) {
      font->xfont = xfonts[count];
      xfonts[count]->ref_count++;

      goto xfont_loaded;
    }
  }

  if (!(font->xfont = calloc(1, sizeof(XFontStruct)))) {
    free(font);

    return NULL;
  }

  font->display = display;

  if (!load_xfont(font->xfont, font_file, format, display->bytes_per_pixel, cs)) {
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

  if (!(p = realloc(xfonts, sizeof(XFontStruct*) * (num_of_xfonts + 1)))) {
    unload_xfont(font->xfont);
    free(font->xfont);
    free(font);

    return NULL;
  }

  xfonts = p;
  xfonts[num_of_xfonts++] = font->xfont;
  font->xfont->ref_count = 1;

xfont_loaded:
  /* Following is almost the same processing as xlib. */

  font->id = id;

  if (font->id & FONT_FULLWIDTH) {
    font->cols = 2;
  } else {
    font->cols = 1;
  }

/*
 * font->is_var_col_width == false and font->is_proportional == true
 * is impossible on framebuffer.
 */
#ifdef USE_FREETYPE
  if (IS_ISCII(cs) && font->xfont->is_aa &&
      (font->xfont->ref_count == 1 || IS_PROPORTIONAL(font->xfont))) {
    /* Proportional glyph is available on ISCII alone for now. */
    font->is_var_col_width = font->is_proportional = 1;

    if (font->xfont->ref_count == 1) {
      init_iscii_ft(font->xfont->face);
      /* +3 is for storing glyph position info. */
      font->xfont->glyph_size += 3;
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
    if (cs == ISO10646_UCS4_1_V) {
      font->is_proportional = 1;

      if (font->xfont->ref_count == 1) {
        /* +3 is for storing glyph position info. */
        font->xfont->glyph_size += 3;
      } else if (!IS_PROPORTIONAL(font->xfont)) {
        clear_glyph_cache_ft(font->xfont);

        /* +3 is for storing glyph position info. */
        font->xfont->glyph_size += 3;
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

  if ((id & FONT_FULLWIDTH) && IS_ISO10646_UCS4(cs) && font->xfont->width_full > 0) {
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
      /*
       * !! Notice !!
       * The width of full and half character font is the same.
       */

      font->x_off = font->width / 2;
      font->width *= 2;
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
      if (font->width != col_width * font->cols) {
        bl_msg_printf(
            "Font(id %x) width(%d) is not matched with "
            "standard width(%d).\n",
            font->id, font->width, col_width * font->cols);

        if (!font->is_var_col_width) {
          if (font->width < col_width * font->cols) {
            font->x_off = (col_width * font->cols - font->width) / 2;
          }

          font->width = col_width * font->cols;
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
    font->width = DIVIDE_ROUNDINGUP(fontsize * font->cols, 2);
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
  bl_debug_printf(BL_DEBUG_TAG " %s font is loaded. => CURRENT NUM OF XFONTS %d\n", font_file,
                  num_of_xfonts);
#endif

#ifdef DEBUG
  ui_font_dump(font);
#endif

  return font;
}

int ui_font_delete(ui_font_t* font) {
  if (--font->xfont->ref_count == 0) {
    u_int count;

    for (count = 0; count < num_of_xfonts; count++) {
      if (xfonts[count] == font->xfont) {
        if (--num_of_xfonts > 0) {
          xfonts[count] = xfonts[num_of_xfonts];
        } else {
          free(xfonts);
          xfonts = NULL;
        }

        break;
      }
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " %s font is unloaded. => CURRENT NUM OF XFONTS %d\n",
                    font->xfont->file, num_of_xfonts);
#endif

    unload_xfont(font->xfont);
    free(font->xfont);
  }

#ifdef USE_OT_LAYOUT
  if (font->ot_font) {
    otl_close(font->ot_font);
  }
#endif

  free(font);

  return 1;
}

int ui_change_font_cols(ui_font_t* font, u_int cols /* 0 means default value */
                        ) {
  if (cols == 0) {
    if (font->id & FONT_FULLWIDTH) {
      font->cols = 2;
    } else {
      font->cols = 1;
    }
  } else {
    font->cols = cols;
  }

  return 1;
}

#ifdef USE_OT_LAYOUT

int ui_font_has_ot_layout_table(ui_font_t* font) {
  if (font->xfont->face) {
    if (!font->ot_font) {
      if (font->ot_font_not_found || !(font->ot_font = otl_open(font->xfont->face, 0))) {
        font->ot_font_not_found = 1;

        return 0;
      }
    }

    return 1;
  }

  return 0;
}

u_int ui_convert_text_to_glyphs(ui_font_t* font, u_int32_t* shaped, u_int shaped_len,
                                int8_t* offsets, u_int8_t* widths, u_int32_t* cmapped,
                                u_int32_t* src, u_int src_len, const char* script,
                                const char* features) {
  return otl_convert_text_to_glyphs(font->ot_font, shaped, shaped_len, offsets, widths, cmapped,
                                    src, src_len, script, features, 0);
}

#endif

u_int ui_calculate_char_width(ui_font_t* font, u_int32_t ch, ef_charset_t cs, int* draw_alone) {
  if (draw_alone) {
    *draw_alone = 0;
  }

#if defined(USE_FREETYPE)
  if (font->xfont->is_aa && font->is_proportional) {
    u_char* glyph;

    if ((glyph = get_ft_bitmap(font->xfont, ch,
#ifdef USE_OT_LAYOUT
                               (font->use_ot_layout /* && font->ot_font */)
#else
                               0
#endif
                                   ))) {
      return glyph[font->xfont->glyph_size - 3];
    }
  }
#endif

  return font->width;
}

/* Return written size */
size_t ui_convert_ucs4_to_utf16(u_char* dst, /* 4 bytes. Big endian. */
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

int ui_font_dump(ui_font_t* font) {
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

  return 1;
}

#endif

u_char* ui_get_bitmap(XFontStruct* xfont, u_char* ch, size_t len, int use_ot_layout) {
  size_t ch_idx;
  int16_t glyph_idx;
  int32_t glyph_offset;

#ifdef USE_FREETYPE
  if (xfont->face) {
    return get_ft_bitmap(xfont, ef_bytes_to_int(ch, len), use_ot_layout);
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
