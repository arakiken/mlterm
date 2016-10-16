/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/* Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined. */
#ifdef USE_LIBSSH2

#include <stdio.h>
#include <pobl/bl_str.h>
#include <pobl/bl_mem.h>

#include "../ui_connect_dialog.h"
#include "../ui_screen.h"

/* --- static variables --- */

static int end_input;
static char *password;
static size_t password_len;

/* --- static functions --- */

static void key_pressed(ui_window_t *win, XKeyEvent *event) {
  if (event->ksym == XK_Return) {
    if (!password) {
      password = strdup("");
    }

    end_input = 1;
  } else {
    if (event->ksym == XK_BackSpace) {
      if (password_len > 0) {
        password[--password_len] = '\0';
      }
    } else if (0x20 <= event->ksym && event->ksym <= 0x7e) {
      void *p;

      if ((p = realloc(password, password_len + 2))) {
        password = p;
        password[password_len++] = event->ksym;
        password[password_len] = '\0';
      }
    }
  }
}

/* --- global functions --- */

int ui_connect_dialog(char **uri,      /* Should be free'ed by those who call this. */
                      char **pass,     /* Same as uri. If pass is not input, "" is set. */
                      char **exec_cmd, /* Same as uri. If exec_cmd is not input, NULL is set. */
                      int *x11_fwd,    /* in/out */
                      char *display_name, Window parent_window, char **sv_list,
                      char *def_server /* (<user>@)(<proto>:)<server address>(:<encoding>). */
                      ) {
  ui_screen_t *screen;
  char prompt[] = "Password: ";
  void (*orig_key_pressed)();

  if (!(*uri = strdup(def_server))) {
    return 0;
  }

  screen = (ui_screen_t *)parent_window;

  ui_window_clear_all(&screen->window);

  ui_window_draw_image_string(&screen->window, ui_get_usascii_font(screen->font_man),
                              ui_get_xcolor(screen->color_man, VT_FG_COLOR),
                              ui_get_xcolor(screen->color_man, VT_BG_COLOR), 0,
                              ui_line_ascent(screen), prompt, sizeof(prompt) - 1);

  orig_key_pressed = screen->window.key_pressed;
  screen->window.key_pressed = key_pressed;

  do {
    ui_display_receive_next_event(NULL);
  } while (!end_input);

  end_input = 0;
  screen->window.key_pressed = orig_key_pressed;

  if (!password) {
    free(*uri);

    return 0;
  }

  *pass = password;
  password = NULL;
  password_len = 0;

  *exec_cmd = NULL;

#if 0
  ui_window_update_all(&screen->window);
#endif

  return 1;
}

#endif
