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

#include <pthread.h>

#ifdef USE_WIN32API
#include <winsock2.h> /* inet_ntoa (winsock2.h should be included before windows.h) */
#include <mef/ef_ucs_property.h>
#else
#include <sys/select.h>
#include <arpa/inet.h> /* inet_ntoa */
#endif

#ifdef __CYGWIN__
#include <sys/wait.h> /* waitpid */
#endif

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* malloc */
#include <pobl/bl_path.h>   /* bl_basename */
#include <pobl/bl_file.h>
#include <pobl/bl_sig_child.h>
#include <pobl/bl_str.h> /* bl_str_replace */
#include <pobl/bl_net.h>
#include <pobl/bl_locale.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */

#ifndef USE_WIN32API
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <wchar.h> /* wcwidth */
#endif
}

#include <fatal_assert.h>
#include <parser.h>
#include <completeterminal.h>
#include <networktransport.h>
#ifdef MOSH_SIXEL
#define USE_TCP_RECV_FROM_SERVER
#endif
#include <networktransport-impl.h>
#include <terminaloverlay.h>
#include <user.h>
#include <timestamp.h>

#if 0
#define __DEBUG
#endif

#if 0
#define TEST_BY_SSH
#endif

/*
 * Following 3 steps allow you to use original terminaldisplayinit.cc.
 * 1) Remove terminaldisplayinit.cc here.
 * 2) #if 0 => #if 1 (define USE_ORIG_TERMINALDISPLAYINIT
 * 3) Add -lncursesw to Makefile.in
 */
#if 0
#define USE_ORIG_TERMINALDISPLAYINIT
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

  bool quit_sequence_started;
  bool lf_entered;
  /* bool escape_requires_lf; */

} vt_pty_mosh_t;

/* --- static variables --- */

static vt_pty_mosh_t **ptys;
static u_int num_ptys;
static Network::Transport<Network::UserStream, Terminal::Complete> *dead_network;

#ifdef MOSH_SIXEL
/* for establish_tcp_connection */
static Network::Transport<Network::UserStream, Terminal::Complete> *cur_network;
#endif

#ifdef USE_WIN32API
static int full_width_prop = -1;

static void (*trigger_pty_read)(void);
#else
static int event_fds[2];
#endif
static int event_in_pipe;
static pthread_cond_t event_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

static int escape_key = 0x1e;
static int escape_pass_key = '^';
static int escape_pass_key2 = '^';
static std::wstring escape_key_help =
#ifdef MOSH_SIXEL
  L"Commands: \".\" quits, \",\" resets, \"^\" gives literal Ctrl-^";
#else
  L"Commands: \".\" quits, \"^\" gives literal Ctrl-^";
#endif
static bool escape_requires_lf = false;

/* --- static functions --- */

#ifdef MOSH_SIXEL
void establish_tcp_connection(int port) {
  if (port <= 0 || cur_network->tcp_sock >= 0) {
    return;
  }

  cur_network->tcp_sock = tcp_connect(cur_network->get_remote_addr().sin.sin_addr.s_addr, port);
  if (cur_network->tcp_sock < 0) {
    bl_error_printf("Failed to connect to mosh server via tcp to send/recv zmodem packets.\n");
  }
}
#endif

#ifdef USE_WIN32API

int wcwidth(wchar_t ch) {
  ef_property_t prop = ef_get_ucs_property(ch);

  if (full_width_prop == -1) {
    char *env = getenv("MOSH_AWIDTH");

    if (env && *env == '2') {
      full_width_prop = EF_FULLWIDTH | EF_AWIDTH;
    } else {
      full_width_prop = EF_FULLWIDTH;
    }
  }

  if (prop & full_width_prop) {
    return 2;
  } else if (prop & EF_COMBINING) {
    return 0;
  } else {
    return 1;
  }
}

/* Same as vt_pty_ssh.cpp */
static ssize_t lo_recv_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  return recv(pty->master, (char*)buf, len, 0);
}

/* Same as vt_pty_ssh.cpp */
static ssize_t lo_send_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  return send(pty->slave, (char*)buf, len, 0);
}

/* Same as vt_pty_ssh.cpp */
static int _socketpair(int af, int type, int proto, int sock[2]) {
  SOCKET listen_sock;
  SOCKADDR_IN addr;
  int addr_len;

  if ((listen_sock = WSASocket(af, type, proto, NULL, 0, 0)) == INVALID_SOCKET) {
    return -1;
  }

  addr_len = sizeof(addr);

  memset((void *)&addr, 0, sizeof(addr));
  addr.sin_family = af;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(listen_sock, (SOCKADDR *)&addr, addr_len) != 0) {
    goto error1;
  }

  if (getsockname(listen_sock, (SOCKADDR *)&addr, &addr_len) != 0) {
    goto error1;
  }

  if (listen(listen_sock, 1) != 0) {
    goto error1;
  }

  /* select() and receive() can call simultaneously on java. */
  if ((sock[0] = WSASocket(af, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
    goto error1;
  }

  if (connect(sock[0], (SOCKADDR *)&addr, addr_len) != 0) {
    goto error2;
  }

  if ((sock[1] = accept(listen_sock, 0, 0)) == INVALID_SOCKET) {
    goto error2;
  }

  closesocket(listen_sock);

  return 0;

error2:
  closesocket(sock[0]);

error1:
  closesocket(listen_sock);

  return -1;
}
#endif /* USE_WIN32API */

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

#ifdef USE_WIN32API
  if (_socketpair(AF_INET, SOCK_STREAM, 0, fds) == 0) {
    u_long val;

    val = 1;
    ioctlsocket(fds[0], FIONBIO, &val);
    val = 1;
    ioctlsocket(fds[1], FIONBIO, &val);

    pty->read = lo_recv_pty;
    pty->write = lo_send_to_pty;
  } else if (_pipe(fds, 256, O_BINARY) == 0) {
    pty->read = lo_read_pty;
    pty->write = lo_write_to_pty;
  }
#else
  if (pipe(fds) == 0) {
    fcntl(fds[0], F_SETFL, O_NONBLOCK | fcntl(pty->master, F_GETFL, 0));
    fcntl(fds[1], F_SETFL, O_NONBLOCK | fcntl(pty->slave, F_GETFL, 0));

    pty->read = lo_read_pty;
    pty->write = lo_write_to_pty;
  }
#endif
  else {
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
  char buf[128];
  ssize_t len;

  if (!pty->stored || --(pty->stored->ref_count) > 0) {
    return 0;
  }

  while ((len = (*pty->read)(pty, (u_char*)buf, sizeof(buf))) > 0) {
    char *p;

    if (!(p = (char*)realloc(((vt_pty_mosh_t *)pty)->buf, ((vt_pty_mosh_t *)pty)->buf_len + len))) {
      break;
    }

    memcpy(p + ((vt_pty_mosh_t *)pty)->buf_len, buf, len);
    ((vt_pty_mosh_t *)pty)->buf = p;
    ((vt_pty_mosh_t *)pty)->buf_len += len;
  }

#ifdef USE_WIN32API
  if (pty->read == lo_recv_pty) {
    closesocket(pty->slave);
    closesocket(pty->master);
  } else
#endif
  {
    close(pty->slave);
    close(pty->master);
  }

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

#ifdef MOSH_SIXEL
static int zmodem_on_tcp = -2;
#endif

static void *watch_ptys(void *arg) {
  pthread_detach(pthread_self());

  int total_timeout = 0;

  while (num_ptys > 0) {
    int timeout = 100;
    int maxfd = 0;
    fd_set fds;

    FD_ZERO(&fds);

    pthread_mutex_lock(&event_mutex);
    u_int count;
    u_int prev_num_ptys = num_ptys;
    for (count = 0; count < num_ptys; count++) {
#ifdef MOSH_SIXEL
      if (zmodem_on_tcp != ptys[count]->network->tcp_sock)
#endif
      {
        std::vector<int> fd_list(ptys[count]->network->fds());
        for (std::vector<int>::const_iterator it = fd_list.begin();
             it != fd_list.end(); it++) {
          FD_SET(*it, &fds);
          if (*it > maxfd) {
            maxfd = *it;
          }
        }

        int wait = ptys[count]->network->wait_time();
        if (timeout > wait) {
          timeout = wait;
        }
      }

#ifdef MOSH_SIXEL
      int fd = ptys[count]->network->tcp_sock;
      if (fd >= 0 &&
          (zmodem_on_tcp == -2 || zmodem_on_tcp == fd)) {
        FD_SET(fd, &fds);
        if (fd > maxfd) {
          maxfd = fd;
        }
      }
#endif
    }
    pthread_mutex_unlock(&event_mutex);

    struct timeval tv;
    tv.tv_usec = timeout * 1000;
    tv.tv_sec = 0;
    int sel_result = select(maxfd + 1, &fds, NULL, NULL, &tv);
#if 0
    if (sel_result < 0 && errno != EINTR) {
      break;
    }
#else
    /* Loop continues until num_ptys becomes 0 even if select() returns -1. */
#endif
    freeze_timestamp();

    bool ready = false;

    pthread_mutex_lock(&event_mutex);
    if (prev_num_ptys == num_ptys) {
      for (count = 0; count < num_ptys; count++) {
        if (sel_result > 0) {
          std::vector<int> fd_list(ptys[count]->network->fds());

#ifdef MOSH_SIXEL
          int fd = ptys[count]->network->tcp_sock;

          if (fd >= 0 &&
              (zmodem_on_tcp == -2 || zmodem_on_tcp == fd)) {
            if (FD_ISSET(fd, &fds)) {
              cur_ps = &ptys[count]->network->ps; /* for tcp_recv_from_server() */
              bool close_socket = !tcp_recv_from_server(fd);

              if (zmodem_processing(&ptys[count]->network->ps)) {
                zmodem_on_tcp = fd;
              } else if (zmodem_on_tcp >= 0) {
                zmodem_on_tcp = -2;
                close_socket = true;
              }

              if (close_socket) {
                closesocket(fd);
                ptys[count]->network->tcp_sock = -1;
              }

              ptys[count]->ready = ready = true;

              goto skip_udp;
            } else if (zmodem_on_tcp == fd) {
              if (!zmodem_processing(&ptys[count]->network->ps)) {
                zmodem_on_tcp = -2;
                closesocket(fd);
                ptys[count]->network->tcp_sock = -1;
              }

              goto skip_udp;
            }
          }
#endif

          for (std::vector<int>::const_iterator it = fd_list.begin();
               it != fd_list.end(); it++) {
            if (FD_ISSET(*it, &fds)) {
#ifdef MOSH_SIXEL
              /* for establish_tcp_connection() which can be called by network->recv() */
              cur_network = ptys[count]->network;
              /* for network->recv() */
              cur_ps = &ptys[count]->network->ps;
#endif
              ptys[count]->network->recv();
              give_hint_to_overlay(ptys[count]);
              ptys[count]->ready = ready = true;

              break;
            }
          }

#ifdef MOSH_SIXEL
        skip_udp:
          ;
#endif
        }
#ifdef MOSH_SIXEL
        pass_seq_change_buf(&ptys[count]->network->ps, 1, false);
#endif
        ptys[count]->network->tick();
#ifdef MOSH_SIXEL
        pass_seq_change_buf(&ptys[count]->network->ps, 0, false);
#endif
      }
    } else if (dead_network) {
      delete dead_network;
      dead_network = NULL;
    }
    pthread_mutex_unlock(&event_mutex);

    if (!ready) {
      total_timeout += timeout;

      if (total_timeout >= 500) {
        total_timeout = 0;

        ready = true;
        for (count = 0; count < num_ptys; count++) {
          ptys[count]->ready = true;
        }
      }
    } else {
      total_timeout = 0;
    }

    if (ready) {
      pthread_mutex_lock(&event_mutex);
      while (event_in_pipe) {
        pthread_cond_wait(&event_cond, &event_mutex);
      }
      event_in_pipe = true;
      pthread_mutex_unlock(&event_mutex);

#ifdef USE_WIN32API
      (*trigger_pty_read)();
#else
      write(event_fds[1], "G", 1);
      fsync(event_fds[1]);
#endif
    }
  }

  if (dead_network) {
    delete dead_network;
    dead_network = NULL;
  }

#ifndef USE_WIN32API
  close(event_fds[0]);
  close(event_fds[1]);
#endif

  return NULL;
}

static int final(vt_pty_t *pty) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  unuse_loopback(pty);

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
   * (in select()?)
   */
  dead_network = pty_mosh->network;
#ifdef MOSH_SIXEL
  if (dead_network->tcp_sock >= 0) {
    closesocket(dead_network->tcp_sock);
    dead_network->tcp_sock = -1; /* for dead_network->tick() not to send via tcp_sock */
  }
#endif
  dead_network->start_shutdown();
  dead_network->tick();

  delete pty_mosh->overlay;

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static int set_winsize(vt_pty_t *pty, u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  pthread_mutex_lock(&event_mutex);

  pty_mosh->network->get_current_state().push_back(Parser::Resize(cols, rows));

#ifdef MOSH_SIXEL
  pass_seq_change_buf(&pty_mosh->network->ps, 1, false);
#endif
  pty_mosh->network->tick();
#ifdef MOSH_SIXEL
  pass_seq_change_buf(&pty_mosh->network->ps, 0, false);
#endif

  pty_mosh->overlay->get_prediction_engine().reset();

  pthread_mutex_unlock(&event_mutex);

  return 1;
}

static ssize_t write_to_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  pthread_mutex_lock(&event_mutex);

#ifdef MOSH_SIXEL
  bool zmodem_cancel = (strcmp((char*)buf, "**\x18\x18\x18\x18\x18\x18\x18\x18"
                                           "\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08") == 0);

  if (pty_mosh->network->tcp_sock >= 0 &&
      tcp_send(pty_mosh->network->tcp_sock, (char*)buf, len)) {
    if (zmodem_cancel) {
      pass_seq_full_reset(&pty_mosh->network->ps);

      if (zmodem_on_tcp == pty_mosh->network->tcp_sock) {
        zmodem_on_tcp = -2;
      }
      closesocket(pty_mosh->network->tcp_sock);
      pty_mosh->network->tcp_sock = -1;
    }

    pass_seq_change_buf(&pty_mosh->network->ps, 1, false);
    pty_mosh->network->tick();
    pass_seq_change_buf(&pty_mosh->network->ps, 0, false);

    goto end;
  } else {
    if (zmodem_cancel) {
      pass_seq_full_reset(&pty_mosh->network->ps);
    }
  }

  pass_seq_change_buf(&pty_mosh->network->ps, 1, false);
#endif

  pty_mosh->overlay->get_prediction_engine().set_local_frame_sent(
    pty_mosh->network->get_sent_state_last());

#ifdef MOSH_SIXEL
  /* for overlays.get_prediction_engine().new_user_byte() */
  cur_ps = &pty_mosh->network->ps;
#endif

  for (size_t count = 0; count < len; count++) {
    pty_mosh->overlay->get_prediction_engine().new_user_byte(buf[count], pty_mosh->framebuffer);

#ifdef MOSH_SIXEL
    if (pass_seq_has_zmodem(&pty_mosh->network->ps) || zmodem_processing(&pty_mosh->network->ps)) {
      goto skip;
    }
#endif

    if (pty_mosh->quit_sequence_started) {
      if (buf[count] == '.') {
        /* Quit sequence is Ctrl-^ . */
        if (pty_mosh->network->has_remote_addr()) {
          pty_mosh->overlay->get_notification_engine().set_notification_string(
            std::wstring( L"Exiting on user request..." ), true);
          bl_trigger_sig_child(pty->child_pid);
        }

        pthread_mutex_unlock(&event_mutex);

        return -1;
      }
#ifdef MOSH_SIXEL
      else if (buf[count] == ',') {
        /* Reset */
        pass_seq_full_reset(&pty_mosh->network->ps);
      }
#endif
      else if (buf[count] == 0x1a) {
        /* Suspend sequence is escape_key Ctrl-Z */
      } else if ((buf[count] == escape_pass_key) || (buf[count] == escape_pass_key2)) {
        /* Emulation sequence to type escape_key is escape_key +
           escape_pass_key (that is escape key without Ctrl) */
        pty_mosh->network->get_current_state().push_back(Parser::UserByte(escape_key));
      } else {
        /* Escape key followed by anything other than . and ^ gets sent literally */
        pty_mosh->network->get_current_state().push_back(Parser::UserByte(escape_key));
        pty_mosh->network->get_current_state().push_back(Parser::UserByte(buf[count]));
      }

      pty_mosh->quit_sequence_started = false;

      if (pty_mosh->overlay->get_notification_engine().get_notification_string() ==
          escape_key_help) {
        pty_mosh->overlay->get_notification_engine().set_notification_string(L"");
      }

      continue;
    }

    pty_mosh->quit_sequence_started = (escape_key > 0) && (buf[count] == escape_key) &&
                                      (pty_mosh->lf_entered || !escape_requires_lf) ;

    if (pty_mosh->quit_sequence_started) {
      pty_mosh->lf_entered = false;
      pty_mosh->overlay->get_notification_engine().set_notification_string(escape_key_help,
                                                                           true, false);

      continue;
    }

    /* LineFeed, Ctrl-J, '\n' or CarriageReturn, Ctrl-M, '\r' */
    pty_mosh->lf_entered = (buf[count] == 0x0A || buf[count] == 0x0D);

    /* XXX */
#if 0
    if (buf[count] == 0x0C) { /* Ctrl-L */
      /* Repaint is requested */
    }
#endif

  skip:
    pty_mosh->network->get_current_state().push_back(Parser::UserByte(buf[count]));
  }

#ifdef MOSH_SIXEL
  pass_seq_change_buf(&pty_mosh->network->ps, 0, true /* clear pass seq generated by overlay */);
  pass_seq_change_buf(&pty_mosh->network->ps, 1, false);
#endif
  pty_mosh->network->tick(); /* send UserByte(buf) above to the server */
#ifdef MOSH_SIXEL
  pass_seq_change_buf(&pty_mosh->network->ps, 0, false);

end:
#endif

  pthread_mutex_unlock(&event_mutex);

  return len;
}

static ssize_t read_pty(vt_pty_t *pty, u_char *buf, size_t len) {
  vt_pty_mosh_t *pty_mosh = (vt_pty_mosh_t*)pty;

  if (event_in_pipe) {
#ifndef USE_WIN32API
    char dummy[16];

    if (read(event_fds[0], dummy, sizeof(dummy)) > 0)
#endif
    {
      pthread_mutex_lock(&event_mutex);
      pthread_cond_signal(&event_cond);
      event_in_pipe = false;
      pthread_mutex_unlock(&event_mutex);
    }
  }

  if (!pty_mosh->network->has_remote_addr()) {
    return 0;
  } else if (((int64_t)pty_mosh->network->get_remote_state_num()) < 0) {
    bl_trigger_sig_child(pty->child_pid);

    return 0;
  }

#if 0
  bl_debug_printf("REMOTE %d %d %d %d %d\n",
                  pty_mosh->network->has_remote_addr(),
                  pty_mosh->network->shutdown_in_progress(),
                  pty_mosh->network->get_remote_state_num(),
                  pty_mosh->network->get_latest_remote_state().timestamp,
                  pty_mosh->network->get_sent_state_acked_timestamp());
#endif

  size_t prev_len = std::min(pty_mosh->buf_len, len);

  if (pty_mosh->buf_len > 0) {
    /*
     * Even if pty_mosh->buf_len > 0, prev_len may be 0 because len may be 0.
     * In this case, it is necessary to enter this block not to change
     * pty_mosh->buf_len.
     */
    memcpy(buf, pty_mosh->buf, prev_len);
    buf += prev_len;
    len -= prev_len;

    if ((pty_mosh->buf_len -= prev_len) == 0) {
      free(pty_mosh->buf);
      pty_mosh->buf = NULL;
    } else {
      memmove(pty_mosh->buf, pty_mosh->buf + prev_len, pty_mosh->buf_len);

      return prev_len;
    }
  }

  if (!pty_mosh->ready) {
    return prev_len;
  }
  pty_mosh->ready = false;

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

#ifdef MOSH_SIXEL /* defined in parser.h */
  pthread_mutex_lock(&event_mutex);

  size_t seq_len;
  char *seq = pass_seq_get(&pty_mosh->network->ps, &seq_len);

  if (seq) {
    /* XXX in case len < 8 */
    if (len >= 8) {
      size_t prepend;

      if (!pass_seq_has_zmodem(&pty_mosh->network->ps) && memcmp(seq, "\x1bP", 2) == 0) {
        memcpy(buf, "\x1b[?8800h", 8); /* for DRCS-Sixel */
        len -= 8;
        memcpy(buf + 8, seq, std::min(seq_len, len));
        prepend = 8;
      } else {
        memcpy(buf, seq, std::min(seq_len, len));
        prepend = 0;
      }

      if (seq_len > len) {
        if ((pty_mosh->buf = (char*)malloc(seq_len - len))) {
          pty_mosh->buf_len = seq_len - len;
          memcpy(pty_mosh->buf, seq + len, pty_mosh->buf_len);
        }
      } else {
        len = seq_len;
      }

      pass_seq_reset(&pty_mosh->network->ps);

      pthread_mutex_unlock(&event_mutex);

      return len + prepend + prev_len;
    }
  } else if (zmodem_processing(&pty_mosh->network->ps)) {
    pthread_mutex_unlock(&event_mutex);

    return prev_len;
  }

  pthread_mutex_unlock(&event_mutex);
#endif

  pthread_mutex_lock(&event_mutex);
  Terminal::Framebuffer framebuffer = pty_mosh->network->get_latest_remote_state().state.get_fb();
  pty_mosh->overlay->apply(framebuffer);
  pthread_mutex_unlock(&event_mutex);

  std::string update = pty_mosh->display.new_frame(pty_mosh->initialized, pty_mosh->framebuffer,
                                                   framebuffer);
  pty_mosh->framebuffer = framebuffer;
  pty_mosh->initialized = true;

  memcpy(buf, update.c_str(), std::min(update.length(), len));
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

#ifdef USE_WIN32API
void vt_pty_mosh_set_pty_read_trigger(void (*func)(void)) {
  trigger_pty_read = func;
}
#endif

vt_pty_t *vt_pty_mosh_new(const char *cmd_path, /* If NULL, child prcess is not exec'ed. */
                          char **cmd_argv,      /* can be NULL(only if cmd_path is NULL) */
                          char **env,           /* can be NULL */
                          const char *uri, const char *pass, const char *pubkey, /* can be NULL */
                          const char *privkey,                                   /* can be NULL */
                          u_int cols, u_int rows, u_int width_pix, u_int height_pix) {
  char *uri_dup;
  char *host;
  char *ip;

  if (!(uri_dup = (char*)alloca(strlen(uri) + 1)) ||
      !bl_parse_uri(NULL, NULL, &host, NULL, NULL, NULL, strcpy(uri_dup, uri))) {
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
  freeaddrinfo(res);

  ip = inet_ntoa(addr);
  char ip_tmp[strlen(ip) + 1];
  ip = strcpy(ip_tmp, ip);

  if (strcmp(host, ip) != 0) {
    char *new_uri;

    if ((new_uri = (char*)alloca(strlen(uri) - strlen(host) + strlen(ip) + 1))) {
      const char *p = strstr(uri, host);
      memcpy(new_uri, uri, p - uri);
      strcpy(new_uri + (p - uri), ip);
      uri = strcat(new_uri, uri + (p - uri + strlen(host)));
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
  if ((locale = bl_get_locale())) {
#ifdef USE_WIN32API
    if (strstr(locale, "Japanese")) {
      locale = "ja_JP.UTF-8";
    } else if (strstr(locale, "Taiwan")) {
      locale = "zh_TW.UTF-8";
    } else if (strstr(locale, "Hong Kong")) {
      locale = "zh_HK.UTF-8";
    } else if (strstr(locale, "China")) {
      locale = "zh_CN.UTF-8";
    } else if (strstr(locale, "Korean")) {
      locale = "ko_KR.UTF-8";
    } else {
      locale = "en_US.UTF-8";
      goto end_locale;
    }

  end_locale:
#endif

    /* The default locale is C.UTF-8 on cygwin. */
    if (strncmp(locale, "C.", 2) != 0) {
      char *p;
      if ((p = (char*)alloca(5 + strlen(locale) + 1))) {
        sprintf(p, "LANG=%s", locale);
        base_argv[6] = p;
      }
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

#ifdef MOSH_SIXEL
  if (getenv("MOSH_NO_TCP")) {
    argv_count++;
  }
#endif

  if (getenv("MOSH_UDP_PORT")) {
    argv_count += 2;
  }

  u_int env_count = 0;
  if (env) {
    while (env[env_count++]);
  }

  char **argv;
  if (argv_count + env_count > 0 &&
      (argv = (char**)alloca(sizeof(base_argv) + sizeof(char*) * (argv_count + env_count * 2)))) {
    memcpy(argv, base_argv, sizeof(base_argv));
    char **argv_p = argv + BL_ARRAY_SIZE(base_argv) - 1;

#ifdef MOSH_SIXEL
    if (getenv("MOSH_NO_TCP")) {
      *(argv_p++) = "-t";
    }
#endif

    /* port:[port2] (Default 60000-6100) (See -p option of mosh-server) */
    char *udp_port = getenv("MOSH_UDP_PORT");
    if (udp_port) {
      *(argv_p++) = "-p";
      *(argv_p++) = udp_port;
    }

    for (env_count = 0; env[env_count]; env_count++) {
      /* "COLORFGBG=default;default" breaks following environmental variables. */
      if (!strchr(env[env_count], ';')) {
#ifndef USE_ORIG_TERMINALDISPLAYINIT
        if (strncmp(env[env_count], "TERM=", 5) == 0 &&
            (strcmp(env[env_count] + 5, "mlterm") == 0 ||
             strcmp(env[env_count] + 5, "kterm") == 0)) {
          bl_msg_printf("mlterm's mosh doesn't support %s which disables bce.\n", env[env_count]);
        }
#endif

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
        char *port_tmp = (line += 13);
        while ('0' <= *line && *line <= '9') { line++; }
        *(line++) = '\0';

        if ((port = (char*)alloca(strlen(port_tmp) + 1))) {
          strcpy(port, port_tmp);
        }
        if ((key = (char*)alloca(strlen(line) + 1))) {
          strcpy(key, line);
        }

#ifdef __DEBUG
        bl_debug_printf("MOSH_KEY=%s PORT=%s\n", key, port);
#endif
      } else if (strncmp(line, "MOSH IP ", 8) == 0) {
        if ((ip = (char*)alloca(strlen(line + 8) + 1))) {
          strcpy(ip, line + 8);
        }

#ifdef __DEBUG
        bl_debug_printf("IP=%s\n", ip);
#endif
      }
#if 1
      /* See src/frontend/mosh-server.cc at https://github.com/arakiken/mosh/tree/sixel-winsock */
      else if (strncmp(line, "MOSH AWIDTH ", 12) == 0) {
#ifdef USE_WIN32API
        /*
         * SUSv2 and glibc (>=2.1.3) defines the tyep of the argument of putenv()
         * as 'char*' (not 'const char*').
         */
        putenv(line[12] == '2' ? (char*)"MOSH_AWIDTH=2" : (char*)"MOSH_AWIDTH=1");
#else
        int serv_width = (line[12] == '2') ? 2 : 1;
        int cli_width = wcwidth(0x25a0); /* See vt_parser_set_pty() in vt_parser.c */

        if (serv_width != cli_width) {
          bl_msg_printf("The number of columns of awidth chars doesn't match.\n"
                        " mosh-server: %d, mosh-client: %d\n", serv_width, cli_width);
        }
#endif
      }
#endif
      /*
       * "The locale requested by LANG=... isn't available here."
       * "Running `locale-gen ...' may be necessary."
       */
      else if (strstr(line, "available here") ||
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
  /* See vt_pty_destroy() in vt_pty.c */
  free(ssh->cmd_line);
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

#ifdef USE_ORIG_TERMINALDISPLAYINIT
      /* see Display::Display() in mosh-x.x.x/src/terminal/terminaldisplayinit.cc */
      char *term = getenv("TERM");
      if (term == NULL ||
          (strncmp(term, "xterm", 5) && strncmp(term, "rxvt", 4) &&
           strncmp(term, "kterm", 5) && strncmp(term, "Eterm", 5) &&
           strncmp(term, "screen", 6))) {
        putenv((char*)"TERM=xterm");
      }
#endif

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

#if 0
      if (!getenv("MOSH_TITLE_NOPREFIX")) {
        pty->overlay->set_title_prefix(wstring(L"[mosh] "));
      }
#endif

      pty->pty.mode = PTY_MOSH;

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
#ifndef USE_WIN32API
          pipe(event_fds);
#endif

          pthread_t thrd;
          pthread_create(&thrd, NULL, watch_ptys, NULL);
        }
      }

#ifdef USE_WIN32API
      pty->pty.master = 10000; /* dummy */
#else
      pty->pty.master = event_fds[0];
#endif
      pty->pty.slave = -1;

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
