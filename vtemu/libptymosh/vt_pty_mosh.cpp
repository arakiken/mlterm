/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

extern "C" {
#undef USE_LIBSSH2
#include "../vt_pty_intern.h"

#include <stdio.h>  /* sprintf */
#include <unistd.h> /* close */
#include <string.h> /* strchr/memcpy */
#include <stdlib.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_file.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_str.h>
#include <pobl/bl_net.h>
#include <pobl/bl_locale.h>

#include <arpa/inet.h> /* inet_ntoa */
#include <pthread.h>
}

#include <fatal_assert.h>
#include <parser.h>
#include <completeterminal.h>
#include <networktransport.h>
#include <networktransport-impl.h>
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
  bool initialized;
  char *buf;
  size_t buf_len;
  bool ready;

} vt_pty_mosh_t;

/* --- static variables --- */

static vt_pty_mosh_t **ptys;
static u_int num_ptys;

static int fds[2];
static bool event_in_pipe = false;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

/* --- static functions --- */

static void *watch_ptys(void *arg) {
  pthread_detach(pthread_self());

  Select &sel = Select::get_instance();

  while (num_ptys > 0) {
    sel.clear_fds();

    int timeout = 100;
    pthread_mutex_lock(&event_mutex);
    u_int count;
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
    for (count = 0; count < num_ptys; count++) {
      std::vector<int> fd_list(ptys[count]->network->fds());
      for (std::vector<int>::const_iterator it = fd_list.begin();
           it != fd_list.end(); it++) {
        if (sel.read(*it)) {
          ptys[count]->network->recv();
          ptys[count]->ready = ready = true;

          break;
        }
      }
      ptys[count]->network->tick();
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

  delete pty_mosh->network;

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  pthread_mutex_lock(&event_mutex);

  ((vt_pty_mosh_t*)pty)->network->get_current_state().push_back(Parser::Resize(cols, rows));
  ((vt_pty_mosh_t*)pty)->network->tick();

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  pthread_mutex_lock(&event_mutex);

  for (size_t count = 0; count < len; count++) {
    ((vt_pty_mosh_t*)pty)->network->get_current_state().push_back(Parser::UserByte(buf[count]));
  }
  ((vt_pty_mosh_t*)pty)->network->tick();

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
  Terminal::Framebuffer framebuffer(pty_mosh->network->get_latest_remote_state().state.get_fb());
  pthread_mutex_unlock(&event_mutex);

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

      pty->display = Terminal::Display(true);
      pty->initialized = false;
      pty->framebuffer = Terminal::Framebuffer(cols, rows);

      pty->pty.final = final;
      pty->pty.set_winsize = set_winsize;
      pty->pty.write = write_to_pty;
      pty->pty.read = read_pty;

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
      pty->pty.slave = -1;

      pty->pty.child_pid = 0;
      while (*key) {
        pty->pty.child_pid = *(key++);
      }

      return &pty->pty;
    } else {
      free(pty);
    }
 
    pthread_mutex_unlock(&event_mutex);
  }

  return NULL;
}
