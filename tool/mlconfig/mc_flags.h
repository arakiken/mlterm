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

GtkWidget * mc_flag_config_widget_new(int id);
void mc_update_flag_mode(int id);

#endif
