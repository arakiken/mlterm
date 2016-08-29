/*
 *	$Id$
 */

#include "ef_char.h"

#include <stdio.h>        /* NULL */
#include <pobl/bl_util.h> /* BE*DEC */

/* --- global functions --- */

u_char* ef_int_to_bytes(u_char* bytes, size_t len, u_int32_t int_ch) {
  if (len == 1) {
    bytes[0] = int_ch & 0xff;
  } else if (len == 2) {
    bytes[0] = (int_ch >> 8) & 0xff;
    bytes[1] = int_ch & 0xff;
  } else if (len == 4) {
    bytes[0] = (int_ch >> 24) & 0xff;
    bytes[1] = (int_ch >> 16) & 0xff;
    bytes[2] = (int_ch >> 8) & 0xff;
    bytes[3] = int_ch & 0xff;
  } else {
    return NULL;
  }

  return bytes;
}

u_int32_t ef_bytes_to_int(const u_char* bytes, size_t len) {
  switch (len) {
    case 1:
      return bytes[0];

    case 2:
      return BE16DEC(bytes);

    case 4:
      return BE32DEC(bytes);

    default:
      return 0;
  }
}
