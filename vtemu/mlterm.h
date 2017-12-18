/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_term_manager.h"

vt_term_t *mlterm_open(char *host, char *pass, int cols, int rows, u_int log_size,
                       char *encoding_str, char **argv,
                       vt_xterm_event_listener_t *xterm_listener,
                       vt_config_event_listener_t *config_listener,
                       vt_screen_event_listener_t *screen_listener,
                       vt_pty_event_listener_t *pty_listener, int open_pty);

void mlterm_final(void);
