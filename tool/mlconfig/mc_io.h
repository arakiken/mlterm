/*
 *	$Id$
 */

#ifndef  __MC_IO_H__
#define  __MC_IO_H__


typedef enum {
	mc_io_set      = 5379,
	mc_io_get      = 5381,
	mc_io_save     = 5382,
	mc_io_set_save = 5383
} mc_io_t;


int  mc_set_str_value( char *  key , char *  value) ;

int  mc_set_flag_value( char *  key , int  flag_val) ;

int  mc_flush(mc_io_t  io) ;

char *  mc_get_str_value( char *  key) ;

int  mc_get_flag_value( char *  key) ;

char *  mc_get_bgtype(void) ;


#endif
