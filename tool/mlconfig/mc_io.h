/*
 *	$Id$
 */

#ifndef  __MC_IO_H__
#define  __MC_IO_H__


typedef enum {
	mc_io_set      = 5379,
	mc_io_get      = 5381,
	mc_io_save     = 5382,
	mc_io_set_save = 5383,
	mc_io_set_font = 5384,
	mc_io_get_font = 5386,
	mc_io_set_save_font = 5387,
	mc_io_set_color = 5388,
	mc_io_get_color = 5390,
	mc_io_set_save_color = 5391,
} mc_io_t;


int  mc_set_str_value( char *  key , char *  value) ;

int  mc_set_flag_value( char *  key , int  flag_val) ;

int  mc_flush(mc_io_t  io) ;

char *  mc_get_str_value( char *  key) ;

int  mc_get_flag_value( char *  key) ;

int  mc_set_font_name( mc_io_t  io , char *  file , char *  font_size , char *  cs ,
	char *  font_name) ;

char *  mc_get_font_name( char *  file , char *  font_size , char *  cs) ;


#endif
