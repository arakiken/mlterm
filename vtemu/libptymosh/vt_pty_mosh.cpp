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
#include <pobl/bl_str.h>
#include <pobl/bl_net.h>
#include <pobl/bl_locale.h>

#ifndef USE_WIN32API
#define _XOPEN_SOURCE
#include <wchar.h> /* wcwidth */
#endif
}

#include <fatal_assert.h>
#include <parser.h>
#include <completeterminal.h>
#include <networktransport.h>
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

} vt_pty_mosh_t;

/* --- static variables --- */

static vt_pty_mosh_t **ptys;
static u_int num_ptys;
static Network::Transport<Network::UserStream, Terminal::Complete> *dead_network;

#ifdef USE_WIN32API
static int full_width_prop = -1;

static void (*trigger_pty_read)(void);
#else
static int event_fds[2];
#endif
static int event_in_pipe;
static pthread_cond_t event_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

/* --- static functions --- */

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

/*
 * Return value
 * -1: utf8_ch is invalid.
 * -2: utf8_ch is insufficient.
 */
size_t convert_utf8_to_ucs(u_int32_t *ucs_ch, u_char *utf8_ch, size_t len) {
  if ((utf8_ch[0] & 0xc0) == 0x80) {
    return (size_t) -1;
  } else if ((utf8_ch[0] & 0x80) == 0) {
    *ucs_ch = utf8_ch[0];

    return 1;
  } else if ((utf8_ch[0] & 0xe0) == 0xc0) {
    if (len < 2) {
      return (size_t) -2;
    }

    if (utf8_ch[1] < 0x80) {
      return (size_t) -1;
    }

    *ucs_ch = (((utf8_ch[0] & 0x1f) << 6) & 0xffffffc0) | (utf8_ch[1] & 0x3f);

    if (*ucs_ch < 0x80) {
      return (size_t) -1;
    }

    return 2;
  } else if ((utf8_ch[0] & 0xf0) == 0xe0) {
    if (len < 3) {
      return (size_t) -2;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80) {
      return (size_t) -1;
    }

    *ucs_ch = (((utf8_ch[0] & 0x0f) << 12) & 0xffff000) |
              (((utf8_ch[1] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[2] & 0x3f);

    if (*ucs_ch < 0x800) {
      return (size_t) -1;
    }

    return 3;
  } else if ((utf8_ch[0] & 0xf8) == 0xf0) {
    if (len < 4) {
      return (size_t) -2;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80) {
      return (size_t) -1;
    }

    *ucs_ch = (((utf8_ch[0] & 0x07) << 18) & 0xfffc0000) |
              (((utf8_ch[1] & 0x3f) << 12) & 0xffff000) |
              (((utf8_ch[2] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[3] & 0x3f);

    if (*ucs_ch < 0x10000) {
      return (size_t) -1;
    }

    return 4;
  } else if ((utf8_ch[0] & 0xfc) == 0xf8) {
    if (len < 5) {
      return (size_t) -2;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80 || utf8_ch[4] < 0x80) {
      return (size_t) -1;
    }

    *ucs_ch = (((utf8_ch[0] & 0x03) << 24) & 0xff000000) |
              (((utf8_ch[1] & 0x3f) << 18) & 0xfffc0000) |
              (((utf8_ch[2] & 0x3f) << 12) & 0xffff000) |
              (((utf8_ch[3] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[4] & 0x3f);

    if (*ucs_ch < 0x200000) {
      return (size_t) -1;
    }

    return 5;
  } else if ((utf8_ch[0] & 0xfe) == 0xfc) {
    if (len < 6) {
      return (size_t) -2;
    }

    if (utf8_ch[1] < 0x80 || utf8_ch[2] < 0x80 || utf8_ch[3] < 0x80 || utf8_ch[4] < 0x80 ||
        utf8_ch[5] < 0x80) {
      return (size_t) -1;
    }

    *ucs_ch =
        (((utf8_ch[0] & 0x01 << 30) & 0xc0000000)) | (((utf8_ch[1] & 0x3f) << 24) & 0xff000000) |
        (((utf8_ch[2] & 0x3f) << 18) & 0xfffc0000) | (((utf8_ch[3] & 0x3f) << 12) & 0xffff000) |
        (((utf8_ch[4] & 0x3f) << 6) & 0xffffffc0) | (utf8_ch[4] & 0x3f);

    if (*ucs_ch < 0x4000000) {
      return (size_t) -1;
    }

    return 6;
  } else {
    return (size_t) -1;
  }
}

/*
 * Return value
 * -1: ucs_ch is invalid.
 * -2: utf8_ch is insufficient.
 */
size_t convert_ucs_to_utf8(u_char *utf8, size_t len, u_int32_t ucs_ch) {
  if (/* 0x00 <= ucs_ch && */ ucs_ch <= 0x7f) {
    utf8[0] = ucs_ch;

    return 1;
  } else if (ucs_ch <= 0x07ff) {
    if (len < 2) {
      return (size_t) -2;
    }

    utf8[0] = ((ucs_ch >> 6) & 0xff) | 0xc0;
    utf8[1] = (ucs_ch & 0x3f) | 0x80;

    return 2;
  } else if (ucs_ch <= 0xffff) {
    if (len < 3) {
      return (size_t) -2;
    }

    utf8[0] = ((ucs_ch >> 12) & 0x0f) | 0xe0;
    utf8[1] = ((ucs_ch >> 6) & 0x3f) | 0x80;
    utf8[2] = (ucs_ch & 0x3f) | 0x80;

    return 3;
  } else if (ucs_ch <= 0x1fffff) {
    if (len < 4) {
      return (size_t) -2;
    }

    utf8[0] = ((ucs_ch >> 18) & 0x07) | 0xf0;
    utf8[1] = ((ucs_ch >> 12) & 0x3f) | 0x80;
    utf8[2] = ((ucs_ch >> 6) & 0x3f) | 0x80;
    utf8[3] = (ucs_ch & 0x3f) | 0x80;

    return 4;
  } else if (ucs_ch <= 0x03ffffff) {
    if (len < 5) {
      return (size_t) -2;
    }

    utf8[0] = ((ucs_ch >> 24) & 0x03) | 0xf8;
    utf8[1] = ((ucs_ch >> 18) & 0x3f) | 0x80;
    utf8[2] = ((ucs_ch >> 12) & 0x3f) | 0x80;
    utf8[4] = ((ucs_ch >> 6) & 0x3f) | 0x80;
    utf8[5] = (ucs_ch & 0x3f) | 0x80;

    return 5;
  } else if (ucs_ch <= 0x7fffffff) {
    if (len < 6) {
      return (size_t) -2;
    }

    utf8[0] = ((ucs_ch >> 30) & 0x01) | 0xfc;
    utf8[1] = ((ucs_ch >> 24) & 0x3f) | 0x80;
    utf8[2] = ((ucs_ch >> 18) & 0x3f) | 0x80;
    utf8[3] = ((ucs_ch >> 12) & 0x3f) | 0x80;
    utf8[4] = ((ucs_ch >> 6) & 0x3f) | 0x80;
    utf8[5] = (ucs_ch & 0x3f) | 0x80;

    return 6;
  } else {
    return (size_t) -1;
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
static int _socketpair(int af, int type, int proto, SOCKET sock[2]) {
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
  if (_socketpair(AF_INET, SOCK_STREAM, 0, (SOCKET*)fds) == 0) {
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
  if (!pty->stored || --(pty->stored->ref_count) > 0) {
    return 0;
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
    pthread_mutex_unlock(&event_mutex);

    struct timeval tv;
    tv.tv_usec = timeout * 1000;
    tv.tv_sec = 0;
    int sel_result = select(maxfd + 1, &fds, NULL, NULL, &tv);
    if (sel_result < 0) {
      break;
    }
    freeze_timestamp();

    bool ready = false;

    pthread_mutex_lock(&event_mutex);
    if (prev_num_ptys == num_ptys) {
      for (count = 0; count < num_ptys; count++) {
        if (sel_result > 0) {
          std::vector<int> fd_list(ptys[count]->network->fds());
          for (std::vector<int>::const_iterator it = fd_list.begin();
               it != fd_list.end(); it++) {
            if (FD_ISSET(*it, &fds)) {
              ptys[count]->network->recv();
              give_hint_to_overlay(ptys[count]);
              ptys[count]->ready = ready = true;

              break;
            }
          }
        }
        ptys[count]->network->tick();
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

  pthread_mutex_lock(&event_mutex);
  Terminal::Framebuffer framebuffer = pty_mosh->network->get_latest_remote_state().state.get_fb();
  pty_mosh->overlay->apply(framebuffer);
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
      }
#if 1
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

#ifndef USE_WIN32API
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
