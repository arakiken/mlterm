/*
 *	$Id$
 */

#ifndef  __MC_FLAGS_H__
#define  __MC_FLAGS_H__


#include  <gtk/gtk.h>

#define MC_FLAG_MODES 7

#define MC_FLAG_AA 0
#define MC_FLAG_VCOL 1
#define MC_FLAG_COMB 2
#define MC_FLAG_DYNCOMB 3
#define MC_FLAG_RECVUCS 4
#define MC_FLAG_MCOL 5
#define MC_FLAG_BIDI 6

#define MC_FLAG_CONFIG0 "use_anti_alias"
#define MC_FLAG_CONFIG1 "use_variable_column_width"
#define MC_FLAG_CONFIG2 "use_combining"
#define MC_FLAG_CONFIG3 "use_dynamic_comb"
#define MC_FLAG_CONFIG4 "receive_string_via_ucs"
#define MC_FLAG_CONFIG5 "use_multi_column_char"
#define MC_FLAG_CONFIG6 "use_bidi"

#define MC_FLAG_LABEL0 "Anti Alias"
#define MC_FLAG_LABEL1 "Variable column width"
#define MC_FLAG_LABEL2 "Combining"
#define MC_FLAG_LABEL3 "Combining = 1 (or 0) logical column(s)"
#define MC_FLAG_LABEL4 "Process received strings via Unicode"
#define MC_FLAG_LABEL5 "Fullwidth = 2 (or 1) logical column(s)"
#define MC_FLAG_LABEL6 "Bidi (UTF8 only)"

GtkWidget * mc_flag_config_widget_new(int id);
void mc_update_flag_mode(int id);

#endif
