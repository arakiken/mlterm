/*
 *	$Id$
 */

#ifndef  __ML_PTY_H__
#define  __ML_PTY_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char/uid_t/gid_t */

#ifdef  USE_UTMP
#include  <kiklib/kik_utmp.h>
#endif


typedef struct  ml_pty
{
	int  master ;		/* master pty fd */
	int  slave ;
	char *  slave_name ;
	pid_t  child_pid ;

#ifdef  USE_UTMP
	kik_utmp_t  utmp ;
#endif

	/* model to be written */
	u_char *  buf ;
	size_t  left ;

} ml_pty_t ;


ml_pty_t *  ml_pty_new( char *  cmd_path , char **  cmd_argv , char **  env , char *  host ,
	u_int  cols , u_int  rows) ;

int  ml_pty_delete( ml_pty_t *  pty) ;

int  ml_set_pty_winsize( ml_pty_t *  pty , u_int  cols , u_int  rows) ;

size_t  ml_write_to_pty( ml_pty_t *  pty , u_char *  buf , size_t  len) ;

int  ml_flush_pty( ml_pty_t *  pty) ;

size_t  ml_read_pty( ml_pty_t *  pty , u_char *  bytes , size_t  left) ;


#endif
