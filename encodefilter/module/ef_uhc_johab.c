/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../src/ef_ko_kr_map.h"

#include "table/ef_johab_to_uhc.table"
#include "table/ef_uhc_to_johab.table"

#if 0
#define SELF_TEST
#endif

/* --- global functions --- */

int ef_map_johab_to_uhc(ef_char_t* uhc, ef_char_t* johab) {
  u_int16_t johab_code;
  u_int16_t c;

  johab_code = ef_char_to_int(johab);

  if ((c = CONV_JOHAB_TO_UHC(johab_code))) {
    ef_int_to_bytes(uhc->ch, 2, c);
    uhc->size = 2;
    uhc->cs = UHC;

    return 1;
  }

  return 0;
}

int ef_map_uhc_to_johab(ef_char_t* johab, ef_char_t* uhc) {
  u_int16_t uhc_code;
  u_int16_t c;

  uhc_code = ef_char_to_int(uhc);

  if ((c = CONV_UHC_TO_JOHAB(uhc_code))) {
    ef_int_to_bytes(johab->ch, 2, c);
    johab->size = 2;
    johab->cs = JOHAB;

    return 1;
  }

  return 0;
}

#ifdef SELF_TEST
int main(void) {
  ef_char_t src;
  ef_char_t dst;

  src.size = 2;
  src.cs = JOHAB;
  for (src.ch[0] = 0x80; src.ch[0] <= 0xdf; src.ch[0]++) {
    int i;
    for (i = 0; i < 0xff; i++) {
      src.ch[1] = i;

      if (!ef_map_johab_to_uhc(&dst, &src)) {
        dst.ch[0] = '\0';
        dst.ch[1] = '\0';
      }

      printf("JOHAB %.2x%.2x => UHC %.2x%.2x\n", src.ch[0], src.ch[1], dst.ch[0], dst.ch[1]);
    }
  }

  src.cs = UHC;
  for (src.ch[0] = 0xb0; src.ch[0] <= 0xc8; src.ch[0]++) {
    int i;
    for (i = 0; i < 0xff; i++) {
      src.ch[1] = i;
      src.cs = JOHAB;

      if (!ef_map_uhc_to_johab(&dst, &src)) {
        dst.ch[0] = '\0';
        dst.ch[1] = '\0';
      }

      printf("UHC %.2x%.2x => JOHAB %.2x%.2x\n", src.ch[0], src.ch[1], dst.ch[0], dst.ch[1]);
    }
  }
}
#endif
