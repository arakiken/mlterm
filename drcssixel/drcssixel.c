/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> /* STDOUT_FILENO */
#include <sys/ioctl.h> /* TIOCGWINSZ */
#include <sys/stat.h>

static int col_width;
static int line_height;

static int get_cell_size(void) {
#ifdef TIOCGWINSZ
  struct winsize ws;

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 &&
      ws.ws_col > 0 && ws.ws_xpixel > ws.ws_col &&
      ws.ws_row > 0 && ws.ws_ypixel > ws.ws_row) {
    col_width = ws.ws_xpixel / ws.ws_col;
    line_height = ws.ws_ypixel / ws.ws_row;
  } else
#endif
  {
    col_width = 8;
    line_height = 16;
  }

  /* Pcmw >= 5 in DECDLD */
  if (col_width < 5 || 99 < col_width || 99 < line_height) {
    return 0;
  } else {
    return 1;
  }
}

static void write_to_stdout(const char *buf, size_t len) {
  ssize_t written;

  while (1) {
    if ((written = write(STDOUT_FILENO, buf, len)) < 0) {
      if (errno == EAGAIN) {
        usleep(100); /* 0.1 msec */
      } else {
        return;
      }
    } else if (written == len) {
      return;
    } else {
      buf += written;
      len -= written;
    }
  }
}

static inline void switch_94_96_cs(char *charset, int *is_96cs) {
  if (*is_96cs == 0) {
    *is_96cs = 1;
  } else {
    *is_96cs = 0;
  }
  *charset = '0';
}

/*
 * Parse DRCS-Sixel and return unicode characters (UTF-32) which represent image
 * pieces of sixel graphics.
 * DCS Pfn; Pcn; Pe; Pcmw; Pss; 3; Pcmh; Pcss { SP Dscs <SIXEL> ST
 */
unsigned int* drcs_sixel_from_data(const char *sixel, /* DCS P1;P2;P3;q...ST */ size_t sixel_len,
                                   char *charset /* Dscs (in/out) */,
                                   int *is_96cs /* Pcss (in/out) */,
                                   int *num_cols /* out */, int *num_rows /* out */) {
  int width;
  int height;
  const char *sixel_p = sixel;
  int x;
  int y;
  unsigned int *buf;
  /*
   * \x1b[?8800h\x1bP1;0;0;%d;1;3;%d;%d{ %c
   *                       2      2  1   1
   */
  char seq[24 + 2 + 2 + 1 + 1 + 1];

  if (sixel_p[0] == '\x1b' && sixel_p[1] == 'P') {
    sixel_p += 2;
  }

  while ('0' <= *sixel_p && *sixel_p <= ';') { sixel_p++; }

  if (*sixel_p != 'q') {
    return NULL;
  }
  sixel_p ++;

  if (sscanf(sixel_p, "\"%d;%d;%d;%d", &x, &y, &width, &height) != 4 ||
      width == 0 || height == 0) {
    return NULL;
  }

  if (sixel + sixel_len <= sixel_p) {
    return NULL;
  }
  sixel_len -= (sixel_p - sixel);

  if (col_width == 0 && !get_cell_size()) {
    return NULL;
  }

  *num_cols = (width + col_width - 1) / col_width;
  *num_rows = (height + line_height - 1) / line_height;

#if 0
  /*
   * XXX
   * The way of drcs_charset increment from 0x7e character set may be different
   * between terminal emulators.
   */
  if (*charset > '0' &&
      ((*num_cols) * (*num_rows) + 0x5f) / 0x60 > 0x7e - *charset + 1) {
    switch_94_96_cs(charset, is_96cs);
  }
#endif

  sprintf(seq, "\x1b[?8800h\x1bP1;0;0;%d;1;3;%d;%d{ %c",
          col_width, line_height, *is_96cs, *charset);
  write_to_stdout(seq, strlen(seq));
  write_to_stdout(sixel_p, sixel_len);
  if (strncmp(sixel_p + sixel_len - 2, "\x1b\\", 2) != 0) {
    write_to_stdout("\x1b\\", 2);
  }

  if ((buf = malloc(sizeof(*buf) * (*num_cols) * (*num_rows)))) {
    unsigned int *buf_p = buf;
    int col;
    int row;
    unsigned int code = 0x100020 + (*is_96cs ? 0x80 : 0) + *charset * 0x100;

    for(row = 0; row < *num_rows; row++) {
      for(col = 0; col < *num_cols; col++) {
        *(buf_p++) = code++;
        if ((code & 0x7f) == 0x0) {
          if (*charset == 0x7e) {
            switch_94_96_cs(charset, is_96cs);
          } else {
            (*charset)++;
          }

          code = 0x100020 + (*is_96cs ? 0x80 : 0) + *charset * 0x100;
        }
      }
    }

    if (*charset == 0x7e) {
      switch_94_96_cs(charset, is_96cs);
    } else {
      (*charset)++;
    }

    return buf;
  }

  return NULL;
}

unsigned int* drcs_sixel_from_file(const char *file, char *charset /* Dscs (in/out) */,
                                   int *is_96cs /* Pcss (in/out) */,
                                   int *num_cols /* out */ , int *num_rows /* out */) {
  FILE *fp;

  if ((fp = fopen(file, "r"))) {
    struct stat st;
    unsigned int *buf;
    char *sixel;

    fstat(fileno(fp), &st);
    if ((sixel = malloc(st.st_size + 1))) {
      fread(sixel, 1, st.st_size, fp);
      sixel[st.st_size] = '\0';
      buf = drcs_sixel_from_data(sixel, st.st_size, charset, is_96cs, num_cols, num_rows);
      free(sixel);
    } else {
      buf = NULL;
    }

    fclose(fp);

    return buf;
  }

  return NULL;
}
