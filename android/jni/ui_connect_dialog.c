/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/* Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined. */
#ifdef USE_LIBSSH2

#include <stdio.h>
#include <pobl/bl_str.h>
#include <pobl/bl_mem.h>
#include <pobl/bl_debug.h>

#include "../ui_connect_dialog.h"

/* --- static variables --- */

static char *d_uri;
static char *d_pass;
static char *d_exec_cmd;

/* --- global functions --- */

int ui_connect_dialog(char **uri,      /* Should be free'ed by those who call this. */
                      char **pass,     /* Same as uri. If pass is not input, "" is set. */
                      char **exec_cmd, /* Same as uri. If exec_cmd is not input, NULL is set. */
                      int *x11_fwd,    /* in/out */
                      char *display_name, Window parent_window, char **sv_list,
                      char *def_server /* (<user>@)(<proto>:)<server address>(:<encoding>). */
                      ) {
  ui_display_show_dialog(def_server);

  *uri = d_uri;
  *pass = d_pass;
  *exec_cmd = d_exec_cmd;
  *x11_fwd = 0;

  d_uri = d_pass = d_exec_cmd = NULL;

  if (*uri) {
    return 1;
  } else {
    return 0;
  }
}

void Java_mlterm_native_1activity_MLActivity_dialogOkClicked(JNIEnv *env, jobject this,
                                                             jstring user, jstring serv,
                                                             jstring port, jstring encoding,
                                                             jstring pass, jstring exec_cmd) {
  const char *s, *u, *p, *e;
  size_t uri_len, len;

  s = (*env)->GetStringUTFChars(env, serv, NULL);
  if (*s == '\0') {
    (*env)->ReleaseStringUTFChars(env, serv, s);
    return;
  }
  u = (*env)->GetStringUTFChars(env, user, NULL);
  p = (*env)->GetStringUTFChars(env, port, NULL);
  e = (*env)->GetStringUTFChars(env, encoding, NULL);
  uri_len = strlen(s) + 1;
  if ((len = strlen(u)) > 0) {
    uri_len += (len + 1);
  }
  if ((len = strlen(p)) > 0) {
    uri_len += (len + 1);
  }
  if ((len = strlen(e)) > 0) {
    uri_len += (len + 1);
  }

  if ((d_uri = malloc(uri_len))) {
    *d_uri = '\0';
    if (*u) {
      strcat(d_uri, u);
      strcat(d_uri, "@");
    }
    strcat(d_uri, s);
    if (*p) {
      strcat(d_uri, ":");
      strcat(d_uri, p);
    }
    if (*e) {
      strcat(d_uri, ":");
      strcat(d_uri, e);
    }
  }

  (*env)->ReleaseStringUTFChars(env, serv, s);
  (*env)->ReleaseStringUTFChars(env, user, u);
  (*env)->ReleaseStringUTFChars(env, port, p);
  (*env)->ReleaseStringUTFChars(env, encoding, e);

  if (*s == '\0') {
    return ;
  }

  p = (*env)->GetStringUTFChars(env, pass, NULL);
  e = (*env)->GetStringUTFChars(env, exec_cmd, NULL);
  d_pass = strdup(p);
  if (*e != '\0') {
    d_exec_cmd = strdup(e);
  }
  (*env)->ReleaseStringUTFChars(env, pass, p);
  (*env)->ReleaseStringUTFChars(env, exec_cmd, e);

  bl_debug_printf( "%s %s %s\n", d_uri, d_pass, d_exec_cmd);
}

#else

#include  <jni.h>

void Java_mlterm_native_1activity_MLActivity_dialogOkClicked(JNIEnv *env, jobject this,
                                                             jstring user, jstring serv,
                                                             jstring port, jstring encoding,
                                                             jstring pass, jstring exec_cmd) {
}

#endif
