/*
 *	update: <2001/11/15(03:40:20)>
 *	$Id$
 */

#ifndef  __KIK_CONF_IO_H__
#define  __KIK_CONF_IO_H__


#include  "kik_file.h"


int  kik_set_sys_conf_dir( char *  dir) ;

char *  kik_get_sys_rc_path( char *  rcfile) ;

char *  kik_get_user_rc_path( char *  rcfile) ;

int  kik_conf_io_write( FILE *  to , char *  key , char *  val) ;

int  kik_conf_io_read( kik_file_t *  from , char **  key , char **  val) ;


#endif
