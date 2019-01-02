/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

extern "C" {
#define USE_LIBSSH2
#define USE_MOSH
#include "../vt_pty_intern.h"

#include <stdio.h>  /* sprintf */
#include <unistd.h> /* close */
#include <string.h> /* strchr/memcpy */
#include <fcntl.h>
#include <stdlib.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_file.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_str.h>
#include <pobl/bl_net.h>
#include <pobl/bl_locale.h>
#include <pobl/bl_unistd.h> /* bl_setenv */

#include <arpa/inet.h> /* inet_ntoa */
#include <pthread.h>

#ifdef __CYGWIN__
#include <sys/wait.h> /* waitpid */
#endif
}

#include <fatal_assert.h>
#include <parser.h>
#include <completeterminal.h>
#include <networktransport.h>
#include <networktransport-impl.h>
#include <terminaloverlay.h>
#include <select.h>
#include <user.h>

#if 0
#define __DEBUG
#endif

#if 0
#define TEST_BY_SSH
#endif

typedef struct vt_pty_mosh {
  vt_pty_t pty;

  Terminal::Complete complete;
  Terminal::Framebuffer framebuffer;
  Network::Transport<Network::UserStream, Terminal::Complete> *network;
  Terminal::Display display;
  Overlay::OverlayManager *overlay;
  bool initialized;
  char *buf;
  size_t buf_len;
  bool ready;

} vt_pty_mosh_t;

/* --- static variables --- */

static vt_pty_mosh_t **ptys;
static u_int num_ptys;
static Network::Transport<Network::UserStream, Terminal::Complete> *dead_network;

static int fds[2];
static bool event_in_pipe = false;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

/* --- static functions --- */


#ifdef __CYGWIN__
static int check_sig_child(pid_t pid) {
  /* SIGCHLD signal isn't delivered on cygwin even if mlconfig exits. */
  int status;

  if (pid > 0 && waitpid(pid, &status, WNOHANG) == pid) {
    bl_trigger_sig_child(pid);

    return 1;
  } else {
    return 0;
  }
}
#endif

static ssize_t lo_read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
#ifdef __CYGWIN__
  if (check_sig_child(pty->config_menu.pid)) {
    /* vt_pty_mosh_set_use_loopback(0) is called from sig_child() in vt_config_menu.c. */
    return 0;
  }
#endif

  return read(pty->master, buf, len);
}

static ssize_t lo_write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
#ifdef __CYGWIN__
  if (check_sig_child(pty->config_menu.pid)) {
    /*
     * vt_pty_mosh_set_use_loopback(0) is called from sig_child()
     * in vt_config_menu.c is called
     */
    return 0;
  }
#endif

  return write(pty->slave, buf, len);
}

static int use_loopback(vt_pty_t *pty) {
  int fds[2];

  if (pty->stored) {
    pty->stored->ref_count++;

    return 1;
  }

  if ((pty->stored = (struct vt_pty::_stored*)malloc(sizeof(*pty->stored))) == NULL) {
    return 0;
  }

  pty->stored->master = pty->master;
  pty->stored->slave = pty->slave;
  pty->stored->read = pty->read;
  pty->stored->write = pty->write;

  if (pipe(fds) == 0) {
    fcntl(fds[0], F_SETFL, O_NONBLOCK | fcntl(pty->master, F_GETFL, 0));
    fcntl(fds[1], F_SETFL, O_NONBLOCK | fcntl(pty->slave, F_GETFL, 0));

    pty->read = lo_read_pty;
    pty->write = lo_write_to_pty;
  } else {
    free(pty->stored);
    pty->stored = NULL;

    return 0;
  }

  pty->master = fds[0];
  pty->slave = fds[1];

  pty->stored->ref_count = 1;

  return 1;
}

static int unuse_loopback(vt_pty_t *pty) {
  if (!pty->stored || --(pty->stored->ref_count) > 0) {
    return 0;
  }

  close(pty->slave);
  close(pty->master);

  pty->master = pty->stored->master;
  pty->slave = pty->stored->slave;
  pty->read = pty->stored->read;
  pty->write = pty->stored->write;

  free(pty->stored);
  pty->stored = NULL;

  return 1;
}

static void give_hint_to_overlay(vt_pty_mosh_t *pty_mosh) {
  pty_mosh->overlay->get_notification_engine().server_heard(
    pty_mosh->network->get_latest_remote_state().timestamp);
  pty_mosh->overlay->get_notification_engine().server_acked(
    pty_mosh->network->get_sent_state_acked_timestamp());
  pty_mosh->overlay->get_prediction_engine().set_local_frame_acked(
    pty_mosh->network->get_sent_state_acked());
  pty_mosh->overlay->get_prediction_engine().set_send_interval(pty_mosh->network->send_interval());
  pty_mosh->overlay->get_prediction_engine().set_local_frame_late_acked(
    pty_mosh->network->get_latest_remote_state().state.get_echo_ack());
}

static void *watch_ptys(void *arg) {
  pthread_detach(pthread_self());

  Select &sel = Select::get_instance();

  while (num_ptys > 0) {
    sel.clear_fds();

    int timeout = 100;
    pthread_mutex_lock(&event_mutex);
    u_int count;
    u_int prev_num_ptys = num_ptys;
    for (count = 0; count < num_ptys; count++) {
      std::vector<int> fd_list(ptys[count]->network->fds());
      for (std::vector<int>::const_iterator it = fd_list.begin();
           it != fd_list.end(); it++) {
        sel.add_fd(*it);
      }

      int wait = ptys[count]->network->wait_time();
      if (timeout > wait) {
        timeout = wait;
      }
    }
    pthread_mutex_unlock(&event_mutex);

    if (sel.select(timeout) < 0) {
      break;
    }

    bool ready = false;

    pthread_mutex_lock(&event_mutex);
    if (prev_num_ptys == num_ptys) {
      for (count = 0; count < num_ptys; count++) {
        std::vector<int> fd_list(ptys[count]->network->fds());
        for (std::vector<int>::const_iterator it = fd_list.begin();
             it != fd_list.end(); it++) {
          if (sel.read(*it)) {
            ptys[count]->network->recv();
            give_hint_to_overlay(ptys[count]);
            ptys[count]->ready = ready = true;

            break;
          }
        }
        ptys[count]->network->tick();
      }
    } else if (dead_network) {
      delete dead_network;
      dead_network = NULL;
    }
    pthread_mutex_unlock(&event_mutex);

    if (ready && !event_in_pipe) {
      event_in_pipe = true;
      write(fds[1], "G", 1);
      fsync(fds[1]);
    }
  }

  close(fds[0]);
  close(fds[1]);

  return NULL;
}

static int final(vt_pty_t *pty) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  if (pty_mosh->buf_len > 0) {
    free(pty_mosh->buf);
  }

  pthread_mutex_lock(&event_mutex);

  u_int count;
  for (count = 0; count < num_ptys; count++) {
    if (ptys[count] == pty_mosh) {
      ptys[count] = ptys[--num_ptys];
    }
  }

  if (dead_network) {
    delete dead_network; /* XXX */
  }
  /*
   * If delete pty_mosh->network here, watch_ptys() might fall into infinite loop.
   * (in sel.select()?)
   */
  dead_network = pty_mosh->network;

  delete pty_mosh->overlay;

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  pthread_mutex_lock(&event_mutex);

  pty_mosh->network->get_current_state().push_back(Parser::Resize(cols, rows));
  pty_mosh->network->tick();

  pty_mosh->overlay->get_prediction_engine().reset();

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  pthread_mutex_lock(&event_mutex);

  pty_mosh->overlay->get_prediction_engine().set_local_frame_sent(
    pty_mosh->network->get_sent_state_last());

  for (size_t count = 0; count < len; count++) {
    pty_mosh->overlay->get_prediction_engine().new_user_byte(buf[count], pty_mosh->framebuffer);
    pty_mosh->network->get_current_state().push_back(Parser::UserByte(buf[count]));
  }
  pty_mosh->network->tick();

  pthread_mutex_unlock(&event_mutex);

  return len;
}

static ssize_t read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  if (event_in_pipe) {
    char dummy[16];
    if (read(fds[0], dummy, sizeof(dummy)) > 0) {
      event_in_pipe = false;
    }
  }

  if (!pty_mosh->network->has_remote_addr()) {
    return 0;
  } else if (((int64_t)pty_mosh->network->get_remote_state_num()) < 0) {
    bl_trigger_sig_child(pty->child_pid);

    return -1;
  }

#if 0
  bl_debug_printf("REMOTE %d %d %d %d %d\n",
                  pty_mosh->network->has_remote_addr(),
                  pty_mosh->network->shutdown_in_progress(),
                  pty_mosh->network->get_remote_state_num(),
                  pty_mosh->network->get_latest_remote_state().timestamp,
                  pty_mosh->network->get_sent_state_acked_timestamp());
#endif

  size_t prev_len;
  if ((prev_len = min(pty_mosh->buf_len, len)) > 0) {
    memcpy(buf, pty_mosh->buf, prev_len);
    buf += prev_len;
    len -= prev_len;

    if ((pty_mosh->buf_len -= prev_len) == 0) {
      free(pty_mosh->buf);
    } else {
      memmove(pty_mosh->buf, pty_mosh->buf + prev_len, pty_mosh->buf_len);

      return prev_len;
    }
  }

  if (!pty_mosh->ready) {
    return prev_len;
  }
  pty_mosh->ready = false;

  pthread_mutex_lock(&event_mutex);
  Terminal::Framebuffer framebuffer = pty_mosh->network->get_latest_remote_state().state.get_fb();
  pty_mosh->overlay->apply(framebuffer);
  pthread_mutex_unlock(&event_mutex);

  if (!pty_mosh->initialized) {
    /* Put terminal in application-cursor-key mode */
    std::string init_str = pty_mosh->display.open();

    if (init_str.length() > len) {
      memcpy(buf, init_str.c_str(), len);
      if ((pty_mosh->buf = (char*)malloc(init_str.length() - len))) {
        pty_mosh->buf_len = init_str.length() - len;
        memcpy(pty_mosh->buf, init_str.c_str() + len, pty_mosh->buf_len);
      }

      return prev_len + len;
    } else {
      memcpy(buf, init_str.c_str(), init_str.length());
      prev_len += init_str.length();
      buf += init_str.length();
      len -= init_str.length();
    }
  }

  std::string update = pty_mosh->display.new_frame(pty_mosh->initialized, pty_mosh->framebuffer,
                                                   framebuffer);
  pty_mosh->framebuffer = framebuffer;
  pty_mosh->initialized = true;

  memcpy(buf, update.c_str(), min(update.length(), len));
  if (update.length() > len) {
    if ((pty_mosh->buf = (char*)malloc(update.length() - len))) {
      pty_mosh->buf_len = update.length() - len;
      memcpy(pty_mosh->buf, update.c_str() + len, pty_mosh->buf_len);
    }
  } else {
    len = update.length();
  }

  return len + prev_len;
}

/* --- global functions --- */

vt_pty_t *vt_pty_mosh_new(const char *cmd_path, /* If NULL, child prcess is not exec'ed. */
                          char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                          char **env,           /* can be NULL */
                          const char *uri, const char *pass, const char *pubkey, /* can be NULL */
                          const char *privkey,                                   /* can be NULL */
                          u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  char *host;
  char *ip;

  if (!bl_parse_uri(NULL, NULL, &host, NULL, NULL, NULL, bl_str_alloca_dup(uri))) {
    return NULL;
  }

  struct addrinfo hints;
  struct addrinfo *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, NULL, &hints, &res) != 0) {
    return NULL;
  }

  struct in_addr addr;
  addr.s_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
  ip = bl_str_alloca_dup(inet_ntoa(addr));
  freeaddrinfo(res);

  if (!ip) {
    return NULL;
  }

  if (strcmp(host, ip) != 0) {
    char *new_uri;
    if ((new_uri = (char*)alloca(strlen(uri) - strlen(host) + strlen(ip) + 1))) {
      strcpy(new_uri, uri);
      uri = bl_str_replace(new_uri, host, ip);
    }
  }

#ifdef TEST_BY_SSH
  int fds[2];
  if (pipe(fds) == -1) {
    return NULL;
  }

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    bl_file_set_cloexec(fds[1]);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    char *argv[] = { "ssh", "-n", "-tt", "-Snone",
#if 0
                     /* This proxy command outputs "MOSH IP ..." */
                     "-oProxyCommand=/usr/bin/mosh --family=prefer-inet --fake-proxy -- %h %p",
#else
                     "-oProxyCommand=nc %h %p",
#endif
                     ip, "--", "mosh-server", "new", "-c", "256", "-s",
                     "-l", "LANG=en_US.UTF-8", "--", "/bin/bash", NULL};
    execv("/usr/bin/ssh", argv);
  } else if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return NULL;
  }

  close(fds[1]);
#else
  char *base_argv[] = { "mosh-server", "new", "-c", "256", "-s", "-l", "LANG=en_US.UTF-8", NULL };

  char *locale;
  /* The default locale is C.UTF-8 on cygwin. */
  if ((locale = bl_get_locale()) && strncmp(locale, "C.", 2) != 0) {
    char *p;
    if ((p = (char*)alloca(5 + strlen(locale) + 1))) {
      sprintf(p, "LANG=%s", locale);
      base_argv[6] = p;
    }
  }

  char *mosh_server = getenv("MOSH_SERVER");
  if (mosh_server) {
    base_argv[0] = mosh_server;
  }

  u_int argv_count = 0;
  if (cmd_argv) {
    while (cmd_argv[argv_count++]);
  }

  u_int env_count = 0;
  if (env) {
    while (env[env_count++]);
  }

  char **argv;
  if (argv_count + env_count > 0 &&
      (argv = (char**)alloca(sizeof(base_argv) + sizeof(char*) * (argv_count + env_count * 2 + 8)))) {
    memcpy(argv, base_argv, sizeof(base_argv));
    char **argv_p = argv + sizeof(base_argv) / sizeof(base_argv[0]) - 1;

    for (env_count = 0; env[env_count]; env_count++) {
      /* "COLORFGBG=default;default" breaks following environmental variables. */
      if (!strchr(env[env_count], ';')) {
        *(argv_p++) = "-l";
        *(argv_p++) = env[env_count];
      }
    }

    if (cmd_argv) {
      *(argv_p++) = "--";
      memcpy(argv_p, cmd_argv, (argv_count + 1) * sizeof(char*));
    } else {
      *argv_p = NULL;
    }
  } else {
    argv = base_argv;
  }

#ifdef __DEBUG
  int count;
  bl_debug_printf("Arguments:\n");
  for (count = 0; argv[count]; count++) {
    bl_debug_printf("=> %s\n", argv[count]);
  }
#endif

  vt_pty_t *ssh = vt_pty_ssh_new(argv[0], argv, env, uri, pass,
                                 pubkey, privkey, cols, rows, width_pix, height_pix);
  if (!ssh) {
    return NULL;
  }
#endif

  char *key = NULL;
  char *port = NULL;
  char buf[1024];
  size_t filled = 0;
  ssize_t len;
#ifdef TEST_BY_SSH
  /* blocking */
  while ((len = read(fds[0], buf + filled, sizeof(buf) - 1 - filled)) > 0)
#else
  /* non blocking */
  while ((len = (*ssh->read)(ssh, (u_char*)buf + filled, sizeof(buf) - 1 - filled)) >= 0)
#endif
  {
    buf[filled += len] = '\0';

    char *line = buf;
    char *p;
    while ((p = strchr(line, '\r')) || (p = strchr(line, '\n'))) {
      *p = '\0';

      if (strncmp(line, "MOSH CONNECT ", 13) == 0) {
        line += 13;
        port = line;
        while ('0' <= *line && *line <= '9') { line++; }
        *line = '\0';
        port = bl_str_alloca_dup(port);
        key = bl_str_alloca_dup(line + 1);

#ifdef __DEBUG
        bl_debug_printf("MOSH_KEY=%s PORT=%s\n", key, port);
#endif
      } else if (strncmp(line, "MOSH IP ", 8) == 0) {
        ip = bl_str_alloca_dup(line + 8);

#ifdef __DEBUG
        bl_debug_printf("IP=%s\n", ip);
#endif
      /*
       * "The locale requested by LANG=... isn't available here."
       * "Running `locale-gen ...' may be necessary."
       */
      } else if (strstr(line, "available here") ||
                 strstr(line, "Running")) {
        bl_msg_printf("mosh-server: %s\n", line);
      }
#ifdef __DEBUG
      else {
        bl_debug_printf("%s\n", line);
      }
#endif

      line = p + (*(p + 1) == '\n' ? 2 : 1);
    }

    if (line != buf) {
      filled = strlen(line);
      memmove(buf, line, filled);
    }
  }

#ifdef TEST_BY_SSH
  close(fds[0]);
#else
  (*ssh->final)(ssh);
  free(ssh);
#endif

  vt_pty_mosh_t *pty;
  if (key && (pty = (vt_pty_mosh_t*)calloc(1, sizeof(vt_pty_mosh_t)))) {
    Network::UserStream blank;
    Terminal::Complete complete(cols, rows);

#ifdef __DEBUG
    bl_debug_printf("URI %s IP %s PORT %s KEY %s\n", uri, ip, port, key);
#endif

    pthread_mutex_lock(&event_mutex);

    pty->network =
      new Network::Transport<Network::UserStream, Terminal::Complete>(blank, complete,
                                                                      key, ip,
                                                                      port ? port : "60001");
    if (pty->network) {
      pty->network->set_send_delay(1); /* minimal delay on outgoing keystrokes */
      pty->network->get_current_state().push_back(Parser::Resize(cols, rows));
      pty->network->set_verbose(0); /* 1 causes abort in init_diff() in transportsender-impl.h */

      pthread_mutex_unlock(&event_mutex);

      Select::set_verbose(0);

      /* see Display::Display() in terminaldisplayinit.cc */
      char *term = getenv("TERM");
      if (term == NULL ||
          (strncmp(term, "xterm", 5) && strncmp(term, "rxvt", 4) &&
           strncmp(term, "kterm", 5) && strncmp(term, "Eterm", 5) &&
           strncmp(term, "screen", 6))) {
        bl_setenv("TERM", "xterm", 1);
      }

      pty->display = Terminal::Display(true);
      pty->initialized = false;
      pty->framebuffer = Terminal::Framebuffer(cols, rows);
      /* OverlayManager doesn't support operator= */
      pty->overlay = new Overlay::OverlayManager();

      char *predict_mode;
      while ((predict_mode = getenv("MOSH_PREDICTION_DISPLAY"))) {
        Overlay::PredictionEngine::DisplayPreference pref;

        if (strcmp(predict_mode, "always") == 0) {
          pref = Overlay::PredictionEngine::Always;
        } else if (strcmp(predict_mode, "never") == 0) {
          pref = Overlay::PredictionEngine::Never;
        } else if (strcmp(predict_mode, "adaptive") == 0) {
          pref = Overlay::PredictionEngine::Adaptive;
        } else if (strcmp(predict_mode, "experimental") == 0) {
          pref = Overlay::PredictionEngine::Experimental;
        } else {
          break;
        }
        pty->overlay->get_prediction_engine().set_display_preference(pref);
        break;
      }

      pty->pty.final = final;
      pty->pty.set_winsize = set_winsize;
      pty->pty.write = write_to_pty;
      pty->pty.read = read_pty;

      pty->pty.child_pid = 0;
      while (*key) {
        pty->pty.child_pid += *(key++);
      }

      u_int count = 0;
      while (count < num_ptys) {
        if (ptys[count]->pty.child_pid == pty->pty.child_pid) {
          pty->pty.child_pid++;
          count = 0;
        } else {
          count++;
        }
      }

      void *p;
      if ((p = realloc(ptys, sizeof(*ptys) * (num_ptys + 1)))) {
        ptys = (vt_pty_mosh_t**)p;
        ptys[num_ptys++] = pty;
        if (num_ptys == 1) {
          pipe(fds);

          pthread_t thrd;
          pthread_create(&thrd, NULL, watch_ptys, NULL);
        }
      }

      pty->pty.master = fds[0];
      pty->pty.slave = -2; /* -1: SSH */

      return &pty->pty;
    } else {
      free(pty);
    }
 
    pthread_mutex_unlock(&event_mutex);
  }

  return NULL;
}

int vt_pty_mosh_set_use_loopback(vt_pty_t *pty, int use) {
  if (use) {
    use_loopback(pty);
    return 1;
  } else {
    return unuse_loopback(pty);
  }
}
