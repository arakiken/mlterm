/*
 *	$Id$
 */

#ifndef  __MC_FLAGS_H__
#define  __MC_FLAGS_H__


#include  <gtk/gtk.h>

#define MC_FLAG_MODES 8

#define MC_FLAG_XFT 0
#define MC_FLAG_AA 1
#define MC_FLAG_VCOL 2
#define MC_FLAG_COMB 3
#define MC_FLAG_DYNCOMB 4
#define MC_FLAG_RECVUCS 5
#define MC_FLAG_MCOL 6
#define MC_FLAG_BIDI 7

GtkWidget * mc_flag_config_widget_new(int id);
void mc_update_flag_mode(int id);

#endif
