/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "indian.h"

static char *a2i_binsearch(struct a2i_tabl *a2i_table, char *word, int lindex, int hindex) {
  int mindex, result;

  while (lindex < hindex) {
    mindex = (lindex + hindex) / 2;
    result = strcmp(a2i_table[mindex].ascii, word);

    if (result == 0) return a2i_table[mindex].iscii;
    if (result > 0) hindex = mindex;
    if (result < 0) lindex = mindex + 1;
  }

  return word;
}

/* IIT Kanpur WX-Keyboard */

static int matra(char *mstr) {
  int i;
  char mvowels[11] = {'A', 'i', 'I', 'H', 'u', 'U', 'q', 'e', 'E', 'o', 'O'};
  for (i = 0; i < 11; i++)
    if (mstr[0] == mvowels[i]) return 1;

  return 0;
}

char *iitk2iscii(struct a2i_tabl *a2i_table, char *buf, char *remem, int a2i_sz) {
  char bufferi[1000], buffer1[1000];

  if (buf[0] == 'a') remem[0] = buf[0];
  strcpy(bufferi, buf);
  memset(buffer1, 0, 1000);

  if (remem[0] == 'a' && (matra(bufferi) == 1) && bufferi[0] != 'a') {
    bufferi[1] = bufferi[0];
    bufferi[0] = 'a';
    bufferi[2] = '\0';
    sprintf(buffer1 + strlen(buffer1), "\b%s", a2i_binsearch(a2i_table, bufferi, 0, a2i_sz));
  } else {
    memset(remem, 0, 5);
    strcpy(buffer1, a2i_binsearch(a2i_table, bufferi, 0, a2i_sz));
  }
  memset(buf, 0, 8);
  strncpy(buf, buffer1, strlen(buffer1));
  return buf;
}

/* Inscript Keyboard */

static char *ins_process(struct a2i_tabl *a2i_table, char *str, int a2i_sz) {
  char *buffer1;

  buffer1 = (char *)malloc(1000);

  sprintf(buffer1, "%s", (char *)a2i_binsearch(a2i_table, str, 0, a2i_sz));
  return buffer1;
}

char *ins2iscii(struct a2i_tabl *a2i_table, char *kbuf, int a2i_sz) {
  char *buffer1, *bufferi;

  bufferi = (char *)calloc(1000, sizeof(char));
  strcat(bufferi, kbuf);
  buffer1 = ins_process(a2i_table, bufferi, a2i_sz);
  strncpy(kbuf, buffer1, strlen(buffer1));
  free(bufferi);
  free(buffer1);
  return kbuf;
}
