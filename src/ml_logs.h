/*
 *	$Id$
 */

#ifndef  __ML_LOGS_H__
#define  __ML_LOGS_H__


#include  <kiklib/kik_cycle_index.h>

#include  "ml_char.h"
#include  "ml_image.h"


typedef struct  ml_logs
{
	ml_image_line_t *  lines ;
	kik_cycle_index_t *  index ;
	u_int  num_of_rows ;

} ml_logs_t ;


int  ml_log_init( ml_logs_t *  logs , u_int  num_of_rows) ;

int  ml_log_final( ml_logs_t *  logs) ;

int  ml_change_log_size( ml_logs_t *  logs , u_int  num_of_rows) ;

int  ml_log_add( ml_logs_t *  logs , ml_image_line_t *  line) ;

ml_image_line_t *  ml_log_get( ml_logs_t *  logs , int  at) ;

u_int  ml_get_num_of_logged_lines( ml_logs_t *  logs) ;

u_int  ml_get_log_size( ml_logs_t *  logs) ;

int  ml_log_reverse_color( ml_logs_t *  logs , int  char_index , int  row) ;

int  ml_log_restore_color( ml_logs_t *  logs , int  char_index , int  row) ;


#endif
