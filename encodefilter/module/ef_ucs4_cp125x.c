/*
 *	$Id$
 */

#include "../src/ef_ucs4_cp125x.h"

#include "table/ef_cp1250_to_ucs4.table"
#include "table/ef_cp1251_to_ucs4.table"
#include "table/ef_cp1252_to_ucs4.table"
#include "table/ef_cp1253_to_ucs4.table"
#include "table/ef_cp1254_to_ucs4.table"
#include "table/ef_cp1255_to_ucs4.table"
#include "table/ef_cp1256_to_ucs4.table"
#include "table/ef_cp1257_to_ucs4.table"
#include "table/ef_cp1258_to_ucs4.table"

#include "table/ef_ucs4_to_cp1250.table"
#include "table/ef_ucs4_to_cp1251.table"
#include "table/ef_ucs4_to_cp1252.table"
#include "table/ef_ucs4_to_cp1253.table"
#include "table/ef_ucs4_to_cp1254.table"
#include "table/ef_ucs4_to_cp1255.table"
#include "table/ef_ucs4_to_cp1256.table"
#include "table/ef_ucs4_to_cp1257.table"
#include "table/ef_ucs4_to_cp1258.table"

#if 0
#define SELF_TEST
#endif

/* --- global functions --- */

int ef_map_cp1250_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1250_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1251_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1251_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1252_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1252_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1253_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1253_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1254_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1254_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1255_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1255_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1256_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1256_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1257_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1257_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_cp1258_to_ucs4(ef_char_t* ucs4, u_int16_t cp_code) {
  u_int32_t c;

  if ((c = CONV_CP1258_TO_UCS4(cp_code))) {
    ef_int_to_bytes(ucs4->ch, 4, c);
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  } else if (0x20 <= cp_code && cp_code <= 0x7e) {
    ucs4->ch[0] = 0x0;
    ucs4->ch[1] = 0x0;
    ucs4->ch[2] = 0x0;
    ucs4->ch[3] = cp_code;
    ucs4->size = 4;
    ucs4->cs = ISO10646_UCS4_1;
    ucs4->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1250(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1250(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1250;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1251(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1251(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1251;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1252(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1252(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1252;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1253(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1253(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1253;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1254(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1254(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1254;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1255(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1255(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1255;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1256(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1256(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1256;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1257(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1257(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1257;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

int ef_map_ucs4_to_cp1258(ef_char_t* non_ucs, u_int32_t ucs4_code) {
  u_int8_t c;

  if ((c = CONV_UCS4_TO_CP1258(ucs4_code))) {
    non_ucs->ch[0] = c;
    non_ucs->size = 1;
    non_ucs->cs = CP1258;
    non_ucs->property = 0;

    return 1;
  }

  return 0;
}

#ifdef SELF_TEST
int main(void) {
  u_int32_t ucs;
  ef_char_t c;

  for (ucs = 0; ucs <= 0x10ffff; ucs++) {
    if (!ef_map_ucs4_to_cp1251(&c, ucs)) {
      c.ch[0] = '\0';
    }

    printf("UCS %x => CP1251 %x\n", ucs, c.ch[0]);
  }
}
#endif
