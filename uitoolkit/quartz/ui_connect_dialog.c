/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/* Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined. */
#ifdef USE_LIBSSH2

#include "../ui_connect_dialog.h"

#include <stdio.h>
#include <pobl/bl_str.h> /* strdup */
#include <pobl/bl_mem.h> /* alloca */
#include "cocoa.h"

/* --- global functions --- */

int ui_connect_dialog(char **uri,      /* Should be free'ed by those who call this. */
                      char **pass,     /* Same as uri. If pass is not input, "" is set. */
                      char **exec_cmd, /* Same as uri. If exec_cmd is not input, NULL is set. */
                      char **privkey,  /* in/out */
                      int *x11_fwd,    /* in/out */
                      char *display_name, Window parent_window,
                      char *def_server /* (<user>@)(<proto>:)<server address>(:<encoding>). */
                      ) {
  char *msg;

  if (!(*uri = strdup(def_server))) {
    return 0;
  }

  if ((msg = alloca(19 + strlen(*uri) + 1))) {
    sprintf(msg, "Enter password for %s", *uri);

    if ((*pass = cocoa_dialog_password(msg))) {
      *exec_cmd = NULL;

      return 1;
    }
  }

  free(*uri);

  return 0;
}

#endif
