/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#ifndef  __ML_PTY_INTERN_H__
#define  __ML_PTY_INTERN_H__

#include  "ml_pty.h"


typedef struct  ml_pty
{
	int  master ;		/* master pty fd */
	int  slave ;		/* slave pty fd */
	pid_t  child_pid ;

	/* Used in ml_write_to_pty */
	u_char *  buf ;
	size_t  left ;
	size_t  size ;

	int (*delete)( ml_pty_ptr_t) ;
	int (*set_winsize)( ml_pty_ptr_t , u_int , u_int) ;
	ssize_t (*write)( ml_pty_ptr_t , u_char * , size_t) ;
	ssize_t (*read)( ml_pty_ptr_t , u_char * , size_t) ;

	ml_pty_event_listener_t *  pty_listener ;

} ml_pty_t ;


ml_pty_t *  ml_pty_unix_new( char *  cmd_path ,	char **  cmd_argv , char **  env , char *  host ,
	u_int  cols , u_int  rows) ;

ml_pty_t *  ml_pty_ssh_new( char *  cmd_path ,	char **  cmd_argv , char **  env , char *  host ,
	char *  pass , 	char *  pubkey , char *  privkey , u_int  cols , u_int  rows) ;

ml_pty_t *  ml_pty_pipe_new( char *  cmd_path ,	char **  cmd_argv , char **  env , char *  host ,
	char *  pass , u_int  cols , u_int  rows) ;


#endif
