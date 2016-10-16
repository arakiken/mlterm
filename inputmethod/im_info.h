/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __IM_INFO_H__
#define __IM_INFO_H__

typedef struct im_info {
  char* id;
  char* name;

  u_int num_of_args;

  char** args;
  char** readable_args;

} im_info_t;

#define IM_INFO_DELETE(info)                      \
  do {                                            \
    int i;                                        \
    if ((info)) {                                 \
      if ((info)->id) {                           \
        free((info)->id);                         \
      }                                           \
      if ((info)->name) {                         \
        free((info)->name);                       \
      }                                           \
      for (i = 0; i < (info)->num_of_args; i++) { \
        if ((info)->args[i]) {                    \
          free((info)->args[i]);                  \
        }                                         \
        if ((info)->readable_args[i]) {           \
          free((info)->readable_args[i]);         \
        }                                         \
      }                                           \
      if ((info)->args) {                         \
        free((info)->args);                       \
      }                                           \
      if ((info)->readable_args) {                \
        free((info)->readable_args);              \
      }                                           \
      free((info));                               \
    }                                             \
  } while (0)

#endif
