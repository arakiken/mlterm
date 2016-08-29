/*
 *	$Id$
 */

#include "bl_dlfcn.h"

#include <stdlib.h> /* atexit, realloc(Don't include bl_mem.h) */

#include "bl_types.h" /* u_int */

/* --- static variables --- */

static bl_dl_handle_t *handles;
static u_int num_of_handles;

/* --- global functions --- */

int bl_dl_close_at_exit(bl_dl_handle_t handle) {
  void *p;

  if (!(p = realloc(handles, sizeof(bl_dl_handle_t) * (num_of_handles + 1)))) {
    return 0;
  }

  handles = p;

#if 0
  if (num_of_handles == 0) {
    atexit(bl_dl_close_all);
  } else
#endif
  {
    u_int count;

    for (count = 0; count < num_of_handles; count++) {
      if (handles[count] == handle) {
        bl_dl_close(handle);

        return 1;
      }
    }
  }

  handles[num_of_handles++] = handle;

  return 1;
}

void bl_dl_close_all(void) {
  u_int count;

  /* Close from the last loaded library. */
  for (count = num_of_handles; count > 0; count--) {
    bl_dl_close(handles[count - 1]);
  }

  num_of_handles = 0;
  free(handles);
  handles = NULL;
}
