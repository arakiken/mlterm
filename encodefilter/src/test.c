/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifdef BL_DEBUG
#include <assert.h>
#include <pobl/bl_debug.h>

#ifndef REMOVE_MAPPING_TABLE

#include "ef_ucs4_jisx0208.h"
#include "ef_ucs4_jisx0212.h"
#include "ef_ucs4_jisx0213.h"
#include "ef_ucs4_big5.h"
#include "ef_ucs4_gbk.h"
#include "ef_ucs4_cns11643.h"
#include "ef_ucs4_uhc.h"
#include "ef_ucs4_iso8859.h"
#include "ef_ucs4_georgian_ps.h"
#include "ef_ucs4_cp125x.h"
#include "ef_ucs4_koi8.h"
#include "ef_ucs4_tcvn5712_1.h"
#include "ef_ucs4_viscii.h"
#include "ef_ko_kr_map.h"

#define TEST_MAP_UCS4(srcname, dstname, srccode, dstcode) \
  { \
    ef_char_t ch; \
    if (!ef_map_##srcname##_to_##dstname(&ch, srccode)) { \
      bl_msg_printf("Not support conversion from " #srcname " to " #dstname ".\n"); \
    } else { \
      assert(ef_char_to_int(&ch) == dstcode); \
    } \
  }

#define TEST_MAP_NON_UCS4(srcname, dstname, srccode, src_bytes, dstcode) \
  { \
    ef_char_t src; /* src.cs is not set for now. */ \
    ef_char_t dst; \
    ef_int_to_bytes(src.ch, src_bytes, srccode); \
    src.size = src_bytes; \
    if (!ef_map_##srcname##_to_##dstname(&dst, &src)) { \
      bl_msg_printf("Not support conversion from " #srcname " to " #dstname ".\n"); \
    } else { \
      assert(ef_char_to_int(&dst) == dstcode); \
    } \
  }


#if 0
#define OUTPUT_MAPPING
#endif

#ifdef OUTPUT_MAPPING

#include <stdio.h>

#define OUTPUT_UCS4(srcname, dstname, start, end, fp)   \
  { \
    ef_char_t ch; \
    int code; \
    for (code = start; code <= end; code++) { \
      if (ef_map_##srcname##_to_##dstname(&ch, code) && code != ef_char_to_int(&ch)) { \
        fprintf(fp, "%x -> %x\n", code, ef_char_to_int(&ch)); \
      } \
    } \
  }

#define OUTPUT_NON_UCS4(srcname, dstname, start, end, fp)   \
  { \
    ef_char_t dst; \
    int code; \
    for (code = start; code <= end; code++) { \
      ef_char_t src; \
      ef_int_to_bytes(src.ch, 4, code); \
      if (ef_map_##srcname##_to_##dstname(&dst, &src) && code != ef_char_to_int(&dst)) { \
        fprintf(fp, "%x -> %x\n", code, ef_char_to_int(&dst)); \
      } \
    } \
  }

#endif

/* --- static functions --- */

static void TEST_mapping(void) {
#ifdef OUTPUT_MAPPING
  FILE *fp;
  int i;
#endif

  TEST_MAP_UCS4(jisx0208_1983, ucs4, 0x224f, 0x2200);
  TEST_MAP_UCS4(ucs4, jisx0208_1983, 0x2200, 0x224f);

  TEST_MAP_UCS4(jisx0212_1990, ucs4, 0x3321, 0x51c8);
  TEST_MAP_UCS4(ucs4, jisx0212_1990, 0x51c8, 0x3321);

  TEST_MAP_UCS4(jisx0213_2000_1, ucs4, 0x216e, 0x2103);
  TEST_MAP_UCS4(ucs4, jisx0213_2000_1, 0x2103, 0x216e);

  TEST_MAP_UCS4(jisx0213_2000_2, ucs4, 0x2c46, 0x3917);
  TEST_MAP_UCS4(ucs4, jisx0213_2000_2, 0x3917, 0x2c46);

  TEST_MAP_UCS4(ucs4, big5, 0x2103, 0xa24a);
  TEST_MAP_UCS4(big5, ucs4, 0xa24a, 0x2103);

#ifdef OUTPUT_MAPPING
  fp = fopen("big52ucs4.log", "w");
  for (i = 0x81; i <= 0xfe; i++) {
    OUTPUT_UCS4(big5, ucs4, i * 0x100 + 0x40, i * 0x100 + 0x7e, fp);
    OUTPUT_UCS4(big5, ucs4, i * 0x100 + 0xa1, i * 0x100 + 0xfe, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, hkscs, 0x2116, 0xc8d2);
  TEST_MAP_UCS4(hkscs, ucs4, 0xc8d2, 0x2116);

#ifdef OUTPUT_MAPPING
  fp = fopen("hkscs2ucs4.log", "w");
  for (i = 0x81; i <= 0xa0; i++) {
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0x40, i * 0x100 + 0x7e, fp);
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0xa1, i * 0x100 + 0xfe, fp);
  }
  OUTPUT_UCS4(hkscs, ucs4, 0xc6a1, 0xc6fe, fp);
  for (i = 0xc7; i <= 0xc8; i++) {
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0x40, i * 0x100 + 0x7e, fp);
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0xa1, i * 0x100 + 0xfe, fp);
  }
  OUTPUT_UCS4(hkscs, ucs4, 0xf9d6, 0xf9fe, fp);
  for (i = 0xfa; i <= 0xfe; i++) {
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0x40, i * 0x100 + 0x7e, fp);
    OUTPUT_UCS4(hkscs, ucs4, i * 0x100 + 0xa1, i * 0x100 + 0xfe, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, gbk, 0x2010, 0xa95c);
  TEST_MAP_UCS4(gbk, ucs4, 0xa95c, 0x2010);

#ifdef OUTPUT_MAPPING
  fp = fopen("gbk2ucs4.log", "w");
  for (i = 0x81; i <= 0xfe; i++) {
    OUTPUT_UCS4(uhc, ucs4, i * 0x100 + 0x40, i * 0x100 + 0x7e, fp);
    OUTPUT_UCS4(uhc, ucs4, i * 0x100 + 0x80, i * 0x100 + 0xfe, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, cns11643_1992_1, 0xfe3e, 0x2151);
  TEST_MAP_UCS4(cns11643_1992_1, ucs4, 0x2151, 0xfe3e);

#ifdef OUTPUT_MAPPING
  fp = fopen("cns11643_1992_12ucs4.log", "w");
  for (i = 0x21; i <= 0x7e; i++) {
    OUTPUT_UCS4(cns11643_1992_1, ucs4, i * 0x100 + 0x21, i * 0x100 + 0x7e, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, cns11643_1992_2, 0x4e93, 0x2131);
  TEST_MAP_UCS4(cns11643_1992_2, ucs4, 0x2131, 0x4e93);

#ifdef OUTPUT_MAPPING
  fp = fopen("cns11643_1992_22ucs4.log", "w");
  for (i = 0x21; i <= 0x7e; i++) {
    OUTPUT_UCS4(cns11643_1992_2, ucs4, i * 0x100 + 0x21, i * 0x100 + 0x7e, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, cns11643_1992_3, 0x51e4, 0x2151);
  TEST_MAP_UCS4(cns11643_1992_3, ucs4, 0x2151, 0x51e4);

#ifdef OUTPUT_MAPPING
  fp = fopen("cns11643_1992_32ucs4.log", "w");
  for (i = 0x21; i <= 0x7e; i++) {
    OUTPUT_UCS4(cns11643_1992_3, ucs4, i * 0x100 + 0x21, i * 0x100 + 0x7e, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, uhc, 0xac26, 0x8151);
  TEST_MAP_UCS4(uhc, ucs4, 0x8151, 0xac26);

#ifdef OUTPUT_MAPPING
  fp = fopen("uhc2ucs4.log", "w");
  for (i = 0x81; i <= 0xfe; i++) {
    OUTPUT_UCS4(uhc, ucs4, i * 0x100 + 0x41, i * 0x100 + 0x5a, fp);
    OUTPUT_UCS4(uhc, ucs4, i * 0x100 + 0x61, i * 0x100 + 0x7a, fp);
    OUTPUT_UCS4(uhc, ucs4, i * 0x100 + 0x81, i * 0x100 + 0xfe, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_NON_UCS4(johab, uhc, 0x8861, 2, 0xb0a1);
  TEST_MAP_NON_UCS4(uhc, johab, 0xb0a1, 2, 0x8861);

#ifdef OUTPUT_MAPPING
  fp = fopen("johab2uhc.log", "w");
  for (i = 0x88; i <= 0xd3; i++) {
    OUTPUT_NON_UCS4(johab, uhc, i * 0x100 + 0x41, i * 0x100 + 0xfd, fp);
  }
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_2_r, 0x102, 0x43); /* 0x43|0x80=0xc3 */
  TEST_MAP_UCS4(iso8859_2_r, ucs4, 0x43, 0x102);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_2_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_2_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_3_r, 0x2d8, 0x22); /* 0x23|0x80=0xa3 */
  TEST_MAP_UCS4(iso8859_3_r, ucs4, 0x22, 0x2d8);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_3_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_3_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_4_r, 0x100, 0x40); /* 0x40|0x80=0xc0 */
  TEST_MAP_UCS4(iso8859_4_r, ucs4, 0x40, 0x100);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_4_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_4_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_10_r, 0x110, 0x29); /* 0x29|0x80=0xa9 */
  TEST_MAP_UCS4(iso8859_10_r, ucs4, 0x29, 0x110);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_10_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_10_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_13_r, 0x100, 0x42); /* 0x42|0x80=0xc2 */
  TEST_MAP_UCS4(iso8859_13_r, ucs4, 0x42, 0x100);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_13_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_13_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_14_r, 0x1e02, 0x21); /* 0x21|0x80=0xa1 */
  TEST_MAP_UCS4(iso8859_14_r, ucs4, 0x21, 0x1e02);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_14_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_14_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, iso8859_16_r, 0x201d, 0x35); /* 0x35|0x80=0xb5 */
  TEST_MAP_UCS4(iso8859_16_r, ucs4, 0x35, 0x201d);

#ifdef OUTPUT_MAPPING
  fp = fopen("iso8859_16_r2ucs4.log", "w");
  OUTPUT_UCS4(iso8859_16_r, ucs4, 0x20, 0x7f, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(ucs4, georgian_ps, 0x2c6, 0x88);
  TEST_MAP_UCS4(georgian_ps, ucs4, 0x88, 0x2c6);
  TEST_MAP_UCS4(ucs4, georgian_ps, 0x2c6, 0x88);
  TEST_MAP_UCS4(georgian_ps, ucs4, 0x88, 0x2c6);

#ifdef OUTPUT_MAPPING
  fp = fopen("georgian_ps2ucs4.log", "w");
  OUTPUT_UCS4(georgian_ps, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1250, ucs4, 0x21, 0x21);
  TEST_MAP_UCS4(cp1250, ucs4, 0x80, 0x20ac);
  TEST_MAP_UCS4(ucs4, cp1250, 0x20ac, 0x80);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12502ucs4.log", "w");
  OUTPUT_UCS4(cp1250, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1251, ucs4, 0x96, 0x2013);
  TEST_MAP_UCS4(ucs4, cp1251, 0x2013, 0x96);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12512ucs4.log", "w");
  OUTPUT_UCS4(cp1251, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1252, ucs4, 0x96, 0x2013);
  TEST_MAP_UCS4(ucs4, cp1252, 0x2013, 0x96);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12522ucs4.log", "w");
  OUTPUT_UCS4(cp1252, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1253, ucs4, 0xb4, 0x384);
  TEST_MAP_UCS4(ucs4, cp1253, 0x384, 0xb4);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12532ucs4.log", "w");
  OUTPUT_UCS4(cp1253, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1254, ucs4, 0xd0, 0x11e);
  TEST_MAP_UCS4(ucs4, cp1254, 0x11e, 0xd0);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12542ucs4.log", "w");
  OUTPUT_UCS4(cp1254, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1255, ucs4, 0x88, 0x2c6);
  TEST_MAP_UCS4(ucs4, cp1255, 0x2c6, 0x88);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12552ucs4.log", "w");
  OUTPUT_UCS4(cp1255, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1256, ucs4, 0xa1, 0x60c);
  TEST_MAP_UCS4(ucs4, cp1256, 0x60c, 0xa1);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12562ucs4.log", "w");
  OUTPUT_UCS4(cp1256, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(cp1257, ucs4, 0xd0, 0x160);
  TEST_MAP_UCS4(ucs4, cp1257, 0x160, 0xd0);

#ifdef OUTPUT_MAPPING
  fp = fopen("cp12572ucs4.log", "w");
  OUTPUT_UCS4(cp1257, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(koi8_r, ucs4, 0xa0, 0x2550);
  TEST_MAP_UCS4(ucs4, koi8_r, 0x2550, 0xa0);

#ifdef OUTPUT_MAPPING
  fp = fopen("koi8_r2ucs4.log", "w");
  OUTPUT_UCS4(koi8_r, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(koi8_u, ucs4, 0xa0, 0x2550);
  TEST_MAP_UCS4(ucs4, koi8_u, 0x2550, 0xa0);

#ifdef OUTPUT_MAPPING
  fp = fopen("koi8_u2ucs4.log", "w");
  OUTPUT_UCS4(koi8_u, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(koi8_t, ucs4, 0xa0, 0x2550);
  TEST_MAP_UCS4(ucs4, koi8_t, 0x2550, 0xa0);

#ifdef OUTPUT_MAPPING
  fp = fopen("koi8_t2ucs4.log", "w");
  OUTPUT_UCS4(koi8_t, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(tcvn5712_1_1993, ucs4, 0xb0, 0x300);
  TEST_MAP_UCS4(ucs4, tcvn5712_1_1993, 0x300, 0xb0);

#ifdef OUTPUT_MAPPING
  fp = fopen("tcvn5712_1_19932ucs4.log", "w");
  OUTPUT_UCS4(tcvn5712_1_1993, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  TEST_MAP_UCS4(viscii, ucs4, 0x80, 0x1ea0);
  TEST_MAP_UCS4(ucs4, viscii, 0x1ea0, 0x80);

#ifdef OUTPUT_MAPPING
  fp = fopen("viscii2ucs4.log", "w");
  OUTPUT_UCS4(viscii, ucs4, 0x80, 0xff, fp);
  fclose(fp);
#endif

  bl_msg_printf("PASS encodefilter mapping test.\n");
}

#endif /* REMOVE_MAPPING_TABLE */

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h>
#include "ef_iso2022jp_parser.h"

static void TEST_iso2022_parser(void) {
  ef_parser_t *parser = ef_iso2022jp_7_parser_new();
  const char *test[] = { "\x1b( @a", "\x1b(/@a" };
  ef_char_t ch;
  int count;

  for (count = 0; count < sizeof(test) / sizeof(test[0]); count++) {
    size_t len = strlen(test[count]);

    (*parser->init)(parser);
    (*parser->set_str)(parser, test[count], len);
    assert((*parser->next_char)(parser, &ch) == 1);
    assert(CS_INTERMEDIATE(ch.cs) == *(test[count] + len - 3));
    assert(CS94SB_FT(ch.cs) == *(test[count] + len - 2));
  }

  (*parser->destroy)(parser);

  bl_msg_printf("PASS encodefilter iso2022 parser test.\n");
}

/* --- global functions --- */

void TEST_encodefilter(void) {
#ifndef REMOVE_MAPPING_TABLE
  TEST_mapping();
#endif

  TEST_iso2022_parser();
}

#endif /* BL_DEBUG */
