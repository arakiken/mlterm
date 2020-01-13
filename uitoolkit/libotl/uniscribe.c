/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <usp10.h>
#include <stdio.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_str.h>

#if 0
#define __DEBUG
#endif

#define OT_TAG(c1,c2,c3,c4) \
  ((((uint8_t)(c4)) << 24) | (((uint8_t)(c3)) << 16) | (((uint8_t)(c2)) << 8) | ((uint8_t)(c1)))

struct uniscribe {
  HDC dc;
  HFONT font;
  SCRIPT_CACHE cache;
};

/* --- static functions --- */

static size_t convert_ucs4_to_utf16(u_int16_t *dst /* 4 bytes */, u_int32_t src) {
  if (src < 0x10000) {
    *dst = src;

    return 1;
  } else if (src < 0x110000) {
    /* surrogate pair */

    src -= 0x10000;

    dst[0] = ((src & 0xfffc0000) >> 10) + 0xd800 + ((src & 0x3fc00) >> 10);
		dst[1] = (src & 0x300) + 0xdc00 + (src & 0xff);

		return 2;
  }

  return 0;
}


/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void *otl_open(void *obj, u_int size) {
  struct uniscribe *us;

  if ((us = malloc(sizeof(*us))) == NULL) {
    return NULL;
  }

  us->dc = obj;
  us->font = GetCurrentObject(obj, OBJ_FONT);
  us->cache = NULL;

  return us;
}

#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
void otl_close(void *obj) {
  free(obj);
}

/*
 * shape_glyphs != NULL && noshape_glyphs != NULL && src != NULL:
 *   noshape_glyphs -> shape_glyphs (called from vt_ot_layout.c)
 * shape_glyphs == NULL: src -> noshape_glyphs (called from vt_ot_layout.c)
 * cmaped == NULL: src -> shape_glyphs (Not used for now)
 *
 * num_shape_glyphs should be greater than src_len.
 */
#ifdef NO_DYNAMIC_LOAD_OTL
static
#endif
u_int otl_convert_text_to_glyphs(void *obj, u_int32_t *shape_glyphs, u_int num_shape_glyphs,
                                 int8_t *xoffsets, int8_t *yoffsets, u_int8_t *advances,
                                 u_int32_t *cmapped, u_int32_t *src, u_int src_len,
                                 const char *script, const char *features, u_int fontsize) {
  struct uniscribe *us = obj;
  WCHAR *utf16_src;
  WORD *glyphs;
  u_int count;

  if ((utf16_src = alloca(sizeof(WCHAR) * src_len * 2)) == NULL) {
    return -1;
  } else {
    WORD *p = utf16_src;

    for (count = 0; count < src_len; count++) {
      p += convert_ucs4_to_utf16(p, src[count]);
    }

    src_len = p - utf16_src;
  }

  if ((glyphs = alloca(sizeof(WORD) * src_len)) == NULL) {
    return -1;
  }

  if (shape_glyphs == NULL) {
		/* xoffsets, yoffsets and advances are always NULL. */

    GetGlyphIndicesW(us->dc, utf16_src, src_len, glyphs, 0 /* GGI_MARK_NONEXISTING_GLYPHS */);

    for (count = 0; count < src_len; count++) {
      cmapped[count] = glyphs[count];
    }

    return src_len;
  } else {
    HFONT orig_font = NULL;
    SCRIPT_ITEM *items;
    OPENTYPE_TAG *tags;
    u_int maxitems;
    int nitems;

    static const char *prev_features = NULL;
    static TEXTRANGE_PROPERTIES tp = { NULL, 0 };
    TEXTRANGE_PROPERTIES *tprop = &tp;
    WORD *logclust;
    SCRIPT_CHARPROP *cprop;
		u_int num_filled_glyphs;
    SCRIPT_GLYPHPROP *gprop;
    int nglyphs;
    int range[1];
		int *advances32;
		GOFFSET *offsets;
		ABC *abcs;

    if (features != prev_features) {
      OPENTYPE_FEATURE_RECORD *frec;

      if ((frec = malloc((bl_count_char_in_str(features, ',') + 1) * sizeof(*frec)))) {
        char *str;

        if ((str = alloca(strlen(features) + 1))) {
          char *p;

          strcpy(str, features);
          count = 0;
          while ((p = bl_str_sep(&str, ","))) {
            if (strlen(p) == 4) {
#ifdef __DEBUG
              bl_debug_printf(BL_DEBUG_TAG " feature: %s\n", p);
#endif
              frec[count].tagFeature = OT_TAG(p[0], p[1], p[2], p[3]);
              frec[count].lParameter = 1;
              count++;
            }
          }

          free(tp.potfRecords);
          tp.potfRecords = frec;
          tp.cotfRecords = count;

          prev_features = features;
        } else {
          free(frec);
        }
      }
    }

    maxitems = src_len + 1; /* cMaxItems < 2 => E_INVALIDARG */
    while (1) {
      HRESULT result;

      /* The buffer should be (cMaxItems + 1) * sizeof(SCRIPT_ITEM) bytes in length */
      if (!(items = alloca(sizeof(SCRIPT_ITEM) * maxitems + 1)) ||
          !(tags = alloca(sizeof(OPENTYPE_TAG) * maxitems))) {
        goto error;
      }

      result = ScriptItemizeOpenType(utf16_src, src_len, maxitems, NULL, NULL,
                                     items, tags, &nitems);
      if (result == E_OUTOFMEMORY) {
        maxitems += 100;
#ifdef DEBUG
        bl_debug_printf(BL_DEBUG_TAG " ScriptItemizeOpenType() returns E_OUTOFMOMERY\n");
#endif
      } else if (result == 0) {
        break;
      } else {
        goto error;
      }
    }

#ifdef __DEBUG
    for (int i = 0; i < nitems; i++) {
      bl_msg_printf("item(%d): Pos %d-%d tag %c%c%c%c\n", i, items[i].iCharPos,
                    items[i + 1].iCharPos - 1,
                    tags[i] & 0xff, (tags[i] >> 8) & 0xff, (tags[i] >> 16) & 0xff,
                    (tags[i] >> 24) & 0xff);
    }
#endif

    if (!(logclust = alloca(sizeof(WORD) * num_shape_glyphs)) ||
        !(cprop = alloca(sizeof(SCRIPT_CHARPROP) * num_shape_glyphs)) ||
        !(gprop = alloca(sizeof(SCRIPT_GLYPHPROP) * num_shape_glyphs))) {
      goto error;
    }
		num_filled_glyphs = 0;

		if (xoffsets /* && yoffsets && advances */) {
			if (!(advances32 = alloca(sizeof(int) * num_shape_glyphs)) ||
					!(offsets = alloca(sizeof(GOFFSET) * num_shape_glyphs)) ||
					!(abcs = alloca(sizeof(ABC) * num_shape_glyphs))) {
				goto error;
			}
		}

    for (count = 0; count < nitems; count++) {
      HRESULT result;
      HDC dc = NULL;

      range[0] = items[count + 1].iCharPos - items[count].iCharPos;

      while (1) {
        result = ScriptShapeOpenType(dc, &us->cache, &items[count].a,
                                     tags[count], 0 /* OT_TAG('R', 'O', 'M', ' ') */,
                                     range, &tprop, 1, utf16_src + items[count].iCharPos,
                                     range[0], num_shape_glyphs - num_filled_glyphs,
                                     logclust, cprop, glyphs + num_filled_glyphs,
                                     gprop, &nglyphs);
        if (result == E_PENDING) {
          orig_font = SelectObject(us->dc, us->font);
          dc = us->dc;
        } else {
          break;
        }
      }

      if (result == 0) {
				dc = NULL;
				while (xoffsets /* && yoffsets && advances */) {
					result = ScriptPlaceOpenType(dc, &us->cache, &items[count].a,
																			 tags[count], 0 /* OT_TAG('R', 'O', 'M', ' ') */,
																			 range, &tprop, 1, utf16_src + items[count].iCharPos,
																			 logclust, cprop, range[0], glyphs + num_filled_glyphs,
																			 gprop, nglyphs, advances32 + num_filled_glyphs,
																			 offsets + num_filled_glyphs, abcs + num_filled_glyphs);
					if (result == E_PENDING) {
						orig_font = SelectObject(us->dc, us->font);
						dc = us->dc;
					} else {
						if (result != 0) {
							memset(offsets + num_filled_glyphs, 0, sizeof(*offsets) * nglyphs);
              /* 0xff: dummy code which gets OTL_IS_COMB() to be false */
							memset(advances32 + num_filled_glyphs, 0xff, sizeof(*advances32) * nglyphs);
						}

						break;
					}
				}

        num_filled_glyphs += nglyphs;
      } else if (num_filled_glyphs <= num_shape_glyphs) {
#ifdef DEBUG
				int i;

        bl_debug_printf("ScriptShapeOpenType() fails with errcode %x.\n(", result);
        for (i = 0; i < range[0]; i++) {
          bl_msg_printf("%x ", utf16_src[items[count].iCharPos + i]);
        }
        bl_msg_printf(")\n");
#endif

        if (num_filled_glyphs + range[0] <= num_shape_glyphs) {
					range[0] = num_shape_glyphs - num_filled_glyphs;
				}

        GetGlyphIndicesW(us->dc, utf16_src + items[count].iCharPos, range[0],
												 glyphs + num_filled_glyphs, 0 /* GGI_MARK_NONEXISTING_GLYPHS */);
        num_filled_glyphs += range[0];
      }
    }

		if (xoffsets /* && yoffsets && advances */) {
			for (count = 0; count < num_filled_glyphs; count++) {
				shape_glyphs[count] = glyphs[count];
				xoffsets[count] = offsets[count].du;
				yoffsets[count] = offsets[count].dv;
				advances[count] = advances32[count];
			}
		} else {
			for (count = 0; count < num_filled_glyphs; count++) {
				shape_glyphs[count] = glyphs[count];
			}
		}

#ifdef __DEBUG
    bl_msg_printf("SRC: ");
    for (count = 0; count < src_len; count++) {
      bl_msg_printf("%x ", src[count]);
    }
    bl_msg_printf("\n");

    for (count = 0; count < num_filled_glyphs; count++) {
      bl_msg_printf("glyph %x", glyphs[count]);
      if (xoffsets /* && yoffsets && advances */) {
#if 1
        bl_msg_printf(" (x%d y%d a%d)", (int)offsets[count].du, (int)offsets[count].dv,
                        advances32[count]);
#else
        bl_msg_printf(" (x%d y%d a%d)", xoffsets[count], yoffsets[count], advances[count]);
#endif
      }
      bl_msg_printf("\n");
    }
#endif

    if (orig_font && us->font != orig_font) {
      SelectObject(us->dc, orig_font);
    }

    return num_filled_glyphs;

  error:
    if (orig_font && us->font != orig_font) {
      SelectObject(us->dc, orig_font);
    }

    return 0;
  }
}
