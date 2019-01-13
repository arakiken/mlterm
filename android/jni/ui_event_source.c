/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_event_source.h"

#include <stdio.h> /* sprintf */
#include <time.h> /* clock */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>
#include <pobl/bl_conf_io.h>
#include <vt_char_encoding.h>
#include <vt_term_manager.h>

#include "ui_display.h"
#include "ui_window.h"
#include "ui_screen.h"

#define UIWINDOW_OF(term) \
  ((term)->parser->xterm_listener ? (term)->parser->xterm_listener->self : NULL)

/* --- static variables --- */

static ef_parser_t *utf8_parser;
/* main and native activity threads changes commit_text/preedit_text from at the
 * same time. */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static char *cur_preedit_text;
static int connect_to_ssh_server;

/* --- static functions --- */

static vt_term_t *get_current_term(void) {
  vt_term_t **terms;
  u_int num_terms;
  u_int count;

  num_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_terms; count++) {
    ui_window_t *win;

    if (vt_term_is_attached(terms[count]) && (win = UIWINDOW_OF(terms[count])) && win->is_focused) {
      return terms[count];
    }
  }

  return NULL;
}

static void update_ime_text(vt_term_t *term, char *preedit_text, char *commit_text) {
  u_char buf[128];
  size_t len;
  ui_window_t *win;

  if (vt_term_is_backscrolling(term)) {
    return;
  }

  if (preedit_text && cur_preedit_text && strcmp(preedit_text, cur_preedit_text) == 0) {
    return;
  }

  vt_term_set_config(term, "use_local_echo", "false");

  if ((win = UIWINDOW_OF(term))) {
    /* XXX If IM plugin is supported, see update_ime_text() in cocoa.m. */
    ((ui_screen_t*)win)->is_preediting = 0;
  }

  if (!utf8_parser && !(utf8_parser = vt_char_encoding_parser_new(VT_UTF8))) {
    return;
  }

  (*utf8_parser->init)(utf8_parser);

  if (preedit_text) {
    if (*preedit_text == '\0') {
      preedit_text = NULL;
    } else {
      if (win) {
        /* XXX If IM plugin is supported, see update_ime_text() in cocoa.m. */
        ((ui_screen_t*)win)->is_preediting = 1; /* Hide cursor (stop blinking) */
      }

      vt_term_set_config(term, "use_local_echo", "true");

      (*utf8_parser->set_str)(utf8_parser, preedit_text, strlen(preedit_text));
      while (!utf8_parser->is_eos &&
             (len = vt_term_convert_to(term, buf, sizeof(buf), utf8_parser)) > 0) {
        vt_term_preedit(term, buf, len);
      }
    }

    if (win) {
      ui_window_update(win, 3);
    }
  } else /* if (commit_text) */
  {
    (*utf8_parser->set_str)(utf8_parser, commit_text, strlen(commit_text));
    while (!utf8_parser->is_eos &&
           (len = vt_term_convert_to(term, buf, sizeof(buf), utf8_parser)) > 0) {
      vt_term_write(term, buf, len);
    }
  }

  free(cur_preedit_text);
  cur_preedit_text = preedit_text ? strdup(preedit_text) : NULL;
}

static void update_ime_text_on_active_term(char *preedit_text, char *commit_text) {
  vt_term_t *term;

  if ((term = get_current_term())) {
    update_ime_text(term, preedit_text, commit_text);
  }
}

static void ALooper_removeFds(ALooper *looper, int *fds, u_int num_fds) {
  u_int count;

  for (count = 0; count < num_fds; count++) {
    ALooper_removeFd(looper, fds[count]);
  }
}

static int need_resize(u_int cur_width,  /* contains scrollbar width and margin area */
                       u_int cur_height, /* contains margin area */
                       u_int new_width,  /* contains scrollbar width and margin area */
                       u_int new_height  /* contains margin area */
                       ) {
  vt_term_t **terms;
  u_int num_terms;
  u_int count;

  num_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_terms; count++) {
    ui_window_t *win;

    if (vt_term_is_attached(terms[count]) && (win = UIWINDOW_OF(terms[count])) && win->is_focused) {
      if (cur_height > new_height) {
        u_int line_height;

        if (vt_term_get_vertical_mode(terms[count])) {
          return 0;
        }

        /* XXX */
        line_height = win->height / vt_term_get_rows(terms[count]);
        if (line_height > 1 &&
            win->height < ((vt_term_get_rows(terms[count]) + 1) * (line_height - 1))) {
          line_height--;
        }

        if (new_height <= win->y + line_height ||
            (vt_term_cursor_row(terms[count]) + 1) * line_height + win->y + win->vmargin <=
            new_height) {
          return 0;
        }
      }

      break;
    }
  }

  if (cur_width > new_width) {
    return 0;
  } else {
    if (count < num_terms) {
      vt_term_set_config(terms[count], "use_local_echo", "false");
    }

    return 1;
  }
}

/* --- global functions --- */

void ui_event_source_init(void) {}

void ui_event_source_final(void) {}

int ui_event_source_process(void) {
  ALooper *looper;
  int ident;
  int events;
  struct android_poll_source *source;
  vt_term_t **terms;
  u_int num_terms;
  u_int count;
  static u_int prev_num_terms;
  static int *fds;
  static u_int32_t prev_msec;

  looper = ALooper_forThread();

  if ((num_terms = vt_get_all_terms(&terms)) == 0) {
    ui_display_final();
    free(fds);
    fds = NULL;
  } else {
    void *p;

    if (prev_num_terms != num_terms && (p = realloc(fds, sizeof(int) * num_terms))) {
      fds = p;
    }

    for (count = 0; count < num_terms; count++) {
      fds[count] = vt_term_get_master_fd(terms[count]);
      ALooper_addFd(looper, fds[count], 1000 + fds[count], ALOOPER_EVENT_INPUT, NULL, NULL);
    }
  }

  prev_num_terms = num_terms;

  /*
   * Read all pending events.
   * Don't block ALooper_pollAll because commit_text or preedit_text can
   * be changed from main thread.
   */
  ident = ALooper_pollAll(100, /* milisec. -1 blocks forever waiting for events */
                          NULL, &events, (void **)&source);

  pthread_mutex_lock(&mutex);

  if (ident >= 0) {
    struct timespec tm;

    if (!ui_display_process_event(source, ident)) {
      ALooper_removeFds(looper, fds, num_terms);
      prev_num_terms = 0;
      free(fds);
      fds = NULL;

      pthread_mutex_unlock(&mutex);

      return 0;
    }

    for (count = 0; count < num_terms; count++) {
      if (vt_term_get_master_fd(terms[count]) + 1000 == ident) {
        ui_window_t *win;

        if (cur_preedit_text && (win = UIWINDOW_OF(terms[count])) && win->is_focused) {
          vt_term_set_config(terms[count], "use_local_echo", "false");
        }

        vt_term_parse_vt100_sequence(terms[count]);

        if (cur_preedit_text && win && win->is_focused) {
          char *preedit_text;

          preedit_text = cur_preedit_text;
          cur_preedit_text = NULL;
          update_ime_text(terms[count], preedit_text, NULL);
        }

        /*
         * Don't break here because some terms can have
         * the same master fd.
         */
      }
    }

    /* ALooper_pollAll() always returns in less than 100ms on some systems. */
    if (clock_gettime(CLOCK_MONOTONIC, &tm) == 0) {
      u_int32_t now_msec = tm.tv_sec * 1000 + tm.tv_nsec / 1000000;

      if (now_msec < prev_msec || now_msec > prev_msec + 100) {
        ui_display_idling(NULL);
        prev_msec = now_msec;
      }
    }
  } else {
    ui_display_idling(NULL);
    prev_msec += 100;
  }

  if (num_terms > 0) {
    if (connect_to_ssh_server) {
      vt_term_t *term = get_current_term();
      if (term && term->parser->config_listener) {
        char cmd[] = "mlclientx --dialog";
        (*term->parser->config_listener->exec)(term->parser->config_listener->self, cmd);
      }
      connect_to_ssh_server = 0;
    }

    ALooper_removeFds(looper, fds, num_terms);
  }

  vt_close_dead_terms();

  ui_close_dead_screens();

  ui_display_unlock();

  pthread_mutex_unlock(&mutex);

  return 1;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int ui_event_source_add_fd(int fd, void (*handler)(void)) { return 0; }

void ui_event_source_remove_fd(int fd) {}

void Java_mlterm_native_1activity_MLActivity_visibleFrameChanged(JNIEnv *env, jobject this,
                                                                 jint yoffset, jint width,
                                                                 jint height) {
  pthread_mutex_lock(&mutex);

  ui_display_resize(yoffset, width, height, cur_preedit_text ? need_resize : NULL);

  pthread_mutex_unlock(&mutex);
}

void Java_mlterm_native_1activity_MLActivity_commitTextLock(JNIEnv *env, jobject this,
                                                            jstring jstr) {
  char *str;

  pthread_mutex_lock(&mutex);

  str = (*env)->GetStringUTFChars(env, jstr, NULL);
  update_ime_text_on_active_term(NULL, str);
  (*env)->ReleaseStringUTFChars(env, jstr, str);

  pthread_mutex_unlock(&mutex);
}

/* Called in the native activity thread for copy&paste. */
void Java_mlterm_native_1activity_MLActivity_commitTextNoLock(JNIEnv *env, jobject this,
                                                              jstring jstr) {
  char *str;

  str = (*env)->GetStringUTFChars(env, jstr, NULL);
  update_ime_text_on_active_term(NULL, str);
  (*env)->ReleaseStringUTFChars(env, jstr, str);
}

void Java_mlterm_native_1activity_MLActivity_preeditText(JNIEnv *env, jobject this, jstring jstr) {
  char *str;

  pthread_mutex_lock(&mutex);

  str = (*env)->GetStringUTFChars(env, jstr, NULL);
  update_ime_text_on_active_term(str, NULL);
  (*env)->ReleaseStringUTFChars(env, jstr, str);

  pthread_mutex_unlock(&mutex);
}

void Java_mlterm_native_1activity_MLActivity_updateScreen(JNIEnv *env, jobject this) {
  pthread_mutex_lock(&mutex);
  ui_display_update_all();
  pthread_mutex_unlock(&mutex);
}

void Java_mlterm_native_1activity_MLActivity_execCommand(JNIEnv *env, jobject this,
                                                         jstring jcmd) {
  vt_term_t *term;

  if ((term = get_current_term()) && term->parser->config_listener) {
    char *cmd = (*env)->GetStringUTFChars(env, jcmd, NULL);
    char *p;

    if (strcmp(cmd, "ssh") == 0) {
      connect_to_ssh_server = 1;
    } else if ((p = alloca(strlen(cmd) + 1))) {
      /*
       * pthread_mutex_lock() shoule be called here because native activity thread
       * (which reads pty) works.
       */
      pthread_mutex_lock(&mutex);
      (*term->parser->config_listener->exec)(term->parser->config_listener->self,
                                             strcpy(p, cmd));
      pthread_mutex_unlock(&mutex);
    }

    (*env)->ReleaseStringUTFChars(env, jcmd, cmd);
  }
}

void Java_mlterm_native_1activity_MLActivity_resumeNative(JNIEnv *env, jobject this) {
  vt_term_t *term;
  if ((term = get_current_term())) {
    ui_window_set_mapped_flag(ui_get_root_window(UIWINDOW_OF(term)), 1);
  }
}

void Java_mlterm_native_1activity_MLActivity_pauseNative(JNIEnv *env, jobject this) {
  vt_term_t *term;
  if ((term = get_current_term())) {
    ui_window_set_mapped_flag(ui_get_root_window(UIWINDOW_OF(term)), 0);
  }
}

jboolean Java_mlterm_native_1activity_MLActivity_isSSH(JNIEnv *env, jobject this) {
#ifdef USE_LIBSSH2
  vt_term_t *term;
  if ((term = get_current_term())) {
    if (vt_term_get_pty_mode(term) == PTY_SSH) {
      return JNI_TRUE;
    }
  }
#endif

  return JNI_FALSE;
}

void Java_mlterm_native_1activity_MLPreferenceActivity_setConfig(JNIEnv *env, jobject this,
                                                                 jstring jkey, jstring jval) {
  vt_term_t *term;

  if ((term = get_current_term()) && term->parser->config_listener) {
    char *key = (*env)->GetStringUTFChars(env, jkey, NULL);
    char *val = (*env)->GetStringUTFChars(env, jval, NULL);
    char *val2;
    char *path;

    /* XXX */
    if (strcmp(key, "col_size_of_width_a") == 0) {
      if (strcmp(val, "true") == 0) {
        val2 = "2";
      } else {
        val2 = "1";
      }
    } else {
      val2 = val;
    }

#if defined(__ANDROID__) && defined(USE_LIBSSH2)
    if (strcmp(key, "start_with_local_pty") == 0) {
      start_with_local_pty = (strcmp(val2, "true") == 0);
    } else
#endif
    {
      /*
       * This function is called while main activity stops (preference activity is active).
       * But pthread_mutex_lock() shoule be called here because native activity thread
       * (which reads pty) works.
       */
      pthread_mutex_lock(&mutex);
      (*term->parser->config_listener->set)(term->parser->config_listener->self, NULL, key, val2);
      pthread_mutex_unlock(&mutex);
    }

    /* Following is the same processing as config_protocol_set() in vt_parser.c */

    if (/* XXX */ strcmp(key, "xim") != 0 && (path = bl_get_user_rc_path("mlterm/main"))) {
      bl_conf_write_t *conf;

      conf = bl_conf_write_open(path);
      free(path);

      if (conf) {
        bl_conf_io_write(conf, key, val2);
        bl_conf_write_close(conf);

        (*term->parser->config_listener->saved)();
      }
    }

    (*env)->ReleaseStringUTFChars(env, jkey, key);
    (*env)->ReleaseStringUTFChars(env, jval, val);
  }
}

/*
 * This is called while main activity stops (preference activity is active), so
 * pthread_mutex_lock() is not called.
 */
void Java_mlterm_native_1activity_MLPreferenceActivity_setDefaultFontConfig(JNIEnv *env,
                                                                            jobject this,
                                                                            jstring jfont) {
  vt_term_t *term;

  if ((term = get_current_term()) && term->parser->config_listener) {
    char *font = (*env)->GetStringUTFChars(env, jfont, NULL);

    (*term->parser->config_listener->set_font)(term->parser->config_listener->self, NULL,
                                               "USASCII", font, 1);
    (*term->parser->config_listener->set_font)(term->parser->config_listener->self, NULL,
                                               "DEFAULT", font, 1);

    (*env)->ReleaseStringUTFChars(env, jfont, font);
  }
}

/* See vt_pty_intern.h */
char *android_config_response;

jboolean Java_mlterm_native_1activity_MLPreferenceActivity_getBoolConfig(JNIEnv *env, jobject this,
                                                                         jstring jkey) {
  vt_term_t *term;

  if ((term = get_current_term())) {
    char *key = (*env)->GetStringUTFChars(env, jkey, NULL);
    size_t key_len = strlen(key);
    (*term->parser->config_listener->get)(term->parser->config_listener->self,
                                          NULL, key, 1);
    (*env)->ReleaseStringUTFChars(env, jkey, key);
    if (android_config_response) {
      /* XXX */
      char *true = strstr(android_config_response, "col_size_of_width_a") ? "2" : "true";
      int flag = (strncmp(android_config_response + 1 + key_len + 1, true, strlen(true)) == 0);

      free(android_config_response);
      android_config_response = NULL;

      return flag;
    }
  }

  return 0;
}

jstring Java_mlterm_native_1activity_MLPreferenceActivity_getStrConfig(JNIEnv *env, jobject this,
                                                                       jstring jkey) {
  vt_term_t *term;

  if ((term = get_current_term())) {
    char *key = (*env)->GetStringUTFChars(env, jkey, NULL);
    size_t key_len = strlen(key);
    jstring jval = NULL;

    (*term->parser->config_listener->get)(term->parser->config_listener->self,
                                          NULL, key, 1);

    if (android_config_response) {
      if (strncmp(android_config_response + 1, key, key_len) == 0) {
        /* \n -> \0 */
        android_config_response[strlen(android_config_response) - 1] = '\0';
        jval = (*env)->NewStringUTF(env, android_config_response + 1 + key_len + 1);
      }

      free(android_config_response);
      android_config_response = NULL;
    }

    (*env)->ReleaseStringUTFChars(env, jkey, key);

    return jval;
  }

  return NULL;
}

jstring Java_mlterm_native_1activity_MLPreferenceActivity_getDefaultFontConfig(JNIEnv *env,
                                                                               jobject this) {
  vt_term_t *term;

  if ((term = get_current_term())) {
    (*term->parser->config_listener->get_font)(term->parser->config_listener->self,
                                               NULL, "USASCII", 1);

    if (android_config_response) {
      char *p;
      if ((p = strrchr(android_config_response, '='))) {
        jstring jfont;
        /* \n -> \0 */
        p[strlen(p) - 1] = '\0';
        if (*p) {
          jfont = (*env)->NewStringUTF(env, p + 1);
        } else {
          jfont = NULL;
        }

        free(android_config_response);
        android_config_response = NULL;

        return jfont;
      }
    }
  }

  return NULL;
}
