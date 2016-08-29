/*
 *	$Id$
 */

#include <pobl/bl_str.h>

#include "ef_ucs4_jisx0201.h"

/*
 * XXX
 * jisx0201_kata msb bit is 0 in mef and is 94 sb charset(excluding space 0x20
 * and delete 0x7f).
 */

/* --- global functions --- */

int ef_map_jisx0201_kata_to_ucs4(ef_char_t* ucs4, u_int16_t jis) {
  if (!(0x21 <= jis && jis <= 0x5f)) {
    return 0;
  }

  ucs4->ch[0] = '\x00';
  ucs4->ch[1] = '\x00';
  ucs4->ch[2] = '\xff';
  ucs4->ch[3] = jis + 0x40;
  ucs4->size = 4;
  ucs4->cs = ISO10646_UCS4_1;
  ucs4->property = 0;

  return 1;
}

int ef_map_jisx0201_roman_to_ucs4(ef_char_t* ucs4, u_int16_t jis) {
  if (!(0x21 <= jis && jis <= 0x7e)) {
    return 0;
  }

  if (jis == 0x5c) {
    memcpy(ucs4->ch, "\x00\x00\x00\xa5", 4);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;
  } else if (jis == 0x7e) {
    memcpy(ucs4->ch, "\x00\x00\x20\x3e", 4);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;
  } else {
    ucs4->ch[0] = jis;
    ucs4->size = 1;
    ucs4->cs = US_ASCII;
    ucs4->property = 0;
  }

  return 1;
}

int ef_map_ucs4_to_jisx0201_kata(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  if (0xff61 <= ucs4_code && ucs4_code <= 0xff9f) {
    non_ucs->ch[0] = ucs4_code - 0xff40;
    non_ucs->size = 1;
    non_ucs->cs = JISX0201_KATA;
    non_ucs->property = 0;

    return 1;
  } else {
    return 0;
  }
}

int ef_map_ucs4_to_jisx0201_roman(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  if (!(0x21 <= ucs4_code && ucs4_code <= 0x7e)) {
    return 0;
  }

  if (ucs4_code == 0x00a5) {
    non_ucs->ch[0] = 0x5c;
  } else if (ucs4_code == 0x203E) {
    non_ucs->ch[0] = 0x7e;
  } else {
    non_ucs->ch[0] = ucs4_code;
  }

  non_ucs->size = 1;
  non_ucs->cs = JISX0201_ROMAN;
  non_ucs->property = 0;

  return 1;
}
