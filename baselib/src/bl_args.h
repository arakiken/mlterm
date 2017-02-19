/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_ARGS_H__
#define __BL_ARGS_H__

#include "bl_mem.h" /* alloca */
#include "bl_str.h" /* bl_count_char_in_str */

#define bl_arg_str_to_array(argc, args)                                      \
  _bl_arg_str_to_array(/*                                                    \
                         * '\t' is not recognized as separator.              \
                         * If you try to recognize '\t', don't forget to add \
                         * checking "-e\t" in                                \
                         * set_config("mlclient") in ui_screen.c.            \
                         */                                                  \
                       alloca(sizeof(char*) * (bl_count_char_in_str(args, ' ') + 2)), argc, args)

int bl_parse_options(char **opt, char **opt_val, int *argc, char ***argv);

char** _bl_arg_str_to_array(char **argv, int *argc, char *args);

#endif
