/*
 *	$Id$
 */

#ifndef  __KIK_CONF_IO_H__
#define  __KIK_CONF_IO_H__


#include  "kik_file.h"


typedef struct kik_conf_write
{
	FILE *  to ;
	char **  lines ;
	u_int  scale ;
	u_int  num ;

} kik_conf_write_t ;


int  kik_set_sys_conf_dir( char *  dir) ;

char *  kik_get_sys_rc_path( char *  rcfile) ;

char *  kik_get_user_rc_path( char *  rcfile) ;

kik_conf_write_t *  kik_conf_write_open( char *  name) ;

int  kik_conf_io_write( kik_conf_write_t *  conf , char *  key , char *  val) ;

int  kik_conf_write_close( kik_conf_write_t *  conf) ;

int  kik_conf_io_read( kik_file_t *  from , char **  key , char **  val) ;


#endif
