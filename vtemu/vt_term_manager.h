/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_TERM_MANAGER_H__
#define __VT_TERM_MANAGER_H__

#include "vt_term.h"

int vt_term_manager_init(u_int multiple);

int vt_term_manager_final(void);

int vt_set_auto_restart_cmd(char *cmd);

vt_term_t *vt_create_term(const char *term_type, u_int cols, u_int rows, u_int tab_size,
                          u_int log_size, vt_char_encoding_t encoding, int is_auto_encoding,
                          int use_auto_detect, int logging_vt_seq, vt_unicode_policy_t policy,
                          int col_size_a, int use_char_combining, int use_multi_col_char,
                          int use_ctl, vt_bidi_mode_t bidi_mode, const char *bidi_separators,
                          int use_dynamic_comb, vt_bs_mode_t bs_mode,
                          vt_vertical_mode_t vertical_mode, int use_local_echo,
                          const char *win_name, const char *icon_name,
                          vt_alt_color_mode_t alt_color_mode, int use_ot_layout);

int vt_destroy_term(vt_term_t *term);

vt_term_t *vt_get_term(const char *dev);

vt_term_t *vt_get_detached_term(const char *dev);

vt_term_t *vt_next_term(vt_term_t *term);

vt_term_t *vt_prev_term(vt_term_t *term);

u_int vt_get_all_terms(vt_term_t ***terms);

int vt_close_dead_terms(void);

char *vt_get_pty_list(void);

void vt_term_manager_enable_zombie_pty(void);

#endif
