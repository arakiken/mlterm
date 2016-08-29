/*
 *	$Id$
 */

#include "vt_iscii.h"

#include <string.h> /* memcpy */
#include "vt_ctl_loader.h"

/* --- global functions --- */

#ifndef NO_DYNAMIC_LOAD_CTL

#ifdef __APPLE__
vt_isciikey_state_t vt_isciikey_state_new(int) __attribute__((weak));
int vt_isciikey_state_delete(vt_isciikey_state_t) __attribute__((weak));
size_t vt_convert_ascii_to_iscii(vt_isciikey_state_t, u_char*, size_t, u_char*, size_t)
    __attribute__((weak));
#endif

vt_isciikey_state_t vt_isciikey_state_new(int is_inscript) {
  vt_isciikey_state_t (*func)(int);

  if (!(func = vt_load_ctl_iscii_func(VT_ISCIIKEY_STATE_NEW))) {
    return NULL;
  }

  return (*func)(is_inscript);
}

int vt_isciikey_state_delete(vt_isciikey_state_t state) {
  int (*func)(vt_isciikey_state_t);

  if (!(func = vt_load_ctl_iscii_func(VT_ISCIIKEY_STATE_DELETE))) {
    return 0;
  }

  return (*func)(state);
}

size_t vt_convert_ascii_to_iscii(vt_isciikey_state_t state, u_char* iscii, size_t iscii_len,
                                 u_char* ascii, size_t ascii_len) {
  int (*func)(vt_isciikey_state_t, u_char*, size_t, u_char*, size_t);

  if (!(func = vt_load_ctl_iscii_func(VT_CONVERT_ASCII_TO_ISCII))) {
    return 0;
  }

  return (*func)(state, iscii, iscii_len, ascii, ascii_len);
}

#else

#ifdef USE_IND
#include "libctl/vt_iscii.c"
#else

/*
 * Dummy functions are necessary for x_im.c
 */

vt_isciikey_state_t vt_isciikey_state_new(int is_inscript) { return NULL; }

int vt_isciikey_state_delete(vt_isciikey_state_t state) { return 0; }

size_t vt_convert_ascii_to_iscii(vt_isciikey_state_t state, u_char* iscii, size_t iscii_len,
                                 u_char* ascii, size_t ascii_len) {
  return 0;
}

#endif
#endif
