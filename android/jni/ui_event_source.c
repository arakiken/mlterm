/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_event_source.h"

#include <stdio.h> /* sprintf */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>
#include <vt_char_encoding.h>
#include <vt_term_manager.h>

#include "ui_display.h"
#include "ui_window.h"

#define XWINDOW_OF(term) \
  ((term)->parser->xterm_listener ? (term)->parser->xterm_listener->self : NULL)

/* --- static variables --- */

static ef_parser_t *utf8_parser;
/* main and native activity threads changes commit_text/preedit_text from at the
 * same time. */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static char *cur_preedit_text;

/* --- static functions --- */

static void update_ime_text(vt_term_t *term, char *preedit_text, char *commit_text) {
  u_char buf[128];
  size_t len;

  if (!utf8_parser && !(utf8_parser = vt_char_encoding_parser_new(VT_UTF8))) {
    return;
  }

  (*utf8_parser->init)(utf8_parser);

  vt_term_set_config(term, "use_local_echo", "false");

  if (preedit_text) {
    ui_window_t *win;

    if (*preedit_text == '\0') {
      preedit_text = NULL;
    } else {
      if (bl_compare_str(preedit_text, cur_preedit_text) == 0) {
        return;
      }

      vt_term_set_config(term, "use_local_echo", "true");

      (*utf8_parser->set_str)(utf8_parser, preedit_text, strlen(preedit_text));
      while (!utf8_parser->is_eos &&
             (len = vt_term_convert_to(term, buf, sizeof(buf), utf8_parser)) > 0) {
        vt_term_preedit(term, buf, len);
      }
    }

    if ((win = XWINDOW_OF(term))) {
      ui_window_update(win, 3);
    }
  } else /* if( commit_text) */
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
  vt_term_t **terms;
  u_int num_of_terms;
  u_int count;

  num_of_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_of_terms; count++) {
    ui_window_t *win;

    if (vt_term_is_attached(terms[count]) && (win = XWINDOW_OF(terms[count])) && win->is_focused) {
      update_ime_text(terms[count], preedit_text, commit_text);

      return;
    }
  }
}

static void ALooper_removeFds(ALooper *looper, int *fds, u_int num_of_fds) {
  u_int count;

  for (count = 0; count < num_of_fds; count++) {
    ALooper_removeFd(looper, fds[count]);
  }
}

static int need_resize(u_int cur_width,  /* contains scrollbar width and margin area */
                       u_int cur_height, /* contains margin area */
                       u_int new_width,  /* contains scrollbar width and margin area */
                       u_int new_height  /* contains margin area */
                       ) {
  vt_term_t **terms;
  u_int num_of_terms;
  u_int count;

  num_of_terms = vt_get_all_terms(&terms);

  for (count = 0; count < num_of_terms; count++) {
    ui_window_t *win;

    if (vt_term_is_attached(terms[count]) && (win = XWINDOW_OF(terms[count])) && win->is_focused) {
      if (cur_height > new_height) {
        u_int line_height;

        if (vt_term_get_vertical_mode(terms[count])) {
          return 0;
        }

        /* XXX */
        line_height = win->height / vt_term_get_rows(terms[count]);

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
    if (count < num_of_terms) {
      vt_term_set_config(terms[count], "use_local_echo", "false");
    }

    return 1;
  }
}

/* --- global functions --- */

int ui_event_source_init(void) { return 1; }

int ui_event_source_final(void) { return 1; }

int ui_event_source_process(void) {
  ALooper *looper;
  int ident;
  int events;
  struct android_poll_source *source;
  vt_term_t **terms;
  u_int num_of_terms;
  u_int count;
  static u_int prev_num_of_terms;
  static int *fds;

  looper = ALooper_forThread();

  if ((num_of_terms = vt_get_all_terms(&terms)) == 0) {
    ui_display_final();
    free(fds);
    fds = NULL;
  } else {
    void *p;

    if (prev_num_of_terms != num_of_terms && (p = realloc(fds, sizeof(int) * num_of_terms))) {
      fds = p;
    }

    for (count = 0; count < num_of_terms; count++) {
      fds[count] = vt_term_get_master_fd(terms[count]);
      ALooper_addFd(looper, fds[count], 1000 + fds[count], ALOOPER_EVENT_INPUT, NULL, NULL);
    }
  }

  prev_num_of_terms = num_of_terms;

  /*
   * Read all pending events.
   * Don't block ALooper_pollAll because commit_text or preedit_text can
   * be changed from main thread.
   */
  ident = ALooper_pollAll(100, /* milisec. -1 blocks forever waiting for events */
                          NULL, &events, (void **)&source);

  pthread_mutex_lock(&mutex);

  if (ident >= 0) {
    if (!ui_display_process_event(source, ident)) {
      ALooper_removeFds(looper, fds, num_of_terms);
      prev_num_of_terms = 0;
      free(fds);
      fds = NULL;

      pthread_mutex_unlock(&mutex);

      return 0;
    }

    for (count = 0; count < num_of_terms; count++) {
      if (vt_term_get_master_fd(terms[count]) + 1000 == ident) {
        ui_window_t *win;

        if (cur_preedit_text && (win = XWINDOW_OF(terms[count])) && win->is_focused) {
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
  } else {
    ui_display_idling(NULL);
  }

  if (num_of_terms > 0) {
    ALooper_removeFds(looper, fds, num_of_terms);
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

int ui_event_source_remove_fd(int fd) { return 0; }

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
