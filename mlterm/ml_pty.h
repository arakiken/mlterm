/*
 *	$Id$
 */

#ifndef  __ML_PTY_H__
#define  __ML_PTY_H__


#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#include  <kiklib/kik_types.h>	/* u_int/u_char */

#ifdef  USE_LIBSSH2

#include  "ml_char_encoding.h"


/*
 * defined(__CYGWIN__) is not to link libpthread to mlterm for now.
 * OPEN_PTY_SYNC is defined in java/Makefile.in
 */
#if  (defined(USE_WIN32API) && ! defined(OPEN_PTY_SYNC)) || \
     (defined(HAVE_PTHREAD) && defined(__CYGWIN__))
#define  OPEN_PTY_ASYNC
#endif

#endif	/* USE_LIBSSH2 */


typedef struct  ml_pty_event_listener
{
	void *  self ;

	/* Called when ml_pty_delete. */
	void  (*closed)( void *) ;

} ml_pty_event_listener_t ;

typedef struct  ml_pty *  ml_pty_ptr_t ;


ml_pty_ptr_t  ml_pty_new( const char *  cmd_path , char **  cmd_argv , char **  env ,
	const char *  host , const char *  work_dir , const char *  pass ,
	const char *  pubkey , const char *  privkey , u_int  cols , u_int  rows ,
	u_int  width_pix , u_int  height_pix) ;

ml_pty_ptr_t  ml_pty_new_with( int  master , int  slave , pid_t  child_pid ,
	u_int  cols , u_int  rows , u_int  width_pix , u_int  height_pix) ;

int  ml_pty_delete( ml_pty_ptr_t  pty) ;

int  ml_pty_set_listener( ml_pty_ptr_t  pty,  ml_pty_event_listener_t *  pty_listener) ;

int  ml_set_pty_winsize( ml_pty_ptr_t  pty , u_int  cols , u_int  rows ,
	u_int  width_pix , u_int  height_pix) ;

size_t  ml_write_to_pty( ml_pty_ptr_t  pty , u_char *  buf , size_t  len) ;

size_t  ml_read_pty( ml_pty_ptr_t  pty , u_char *  buf , size_t  left) ;

void  ml_response_config( ml_pty_ptr_t  pty , char *  key , char *  value , int  to_menu) ;

pid_t  ml_pty_get_pid( ml_pty_ptr_t  pty) ;

int  ml_pty_get_master_fd( ml_pty_ptr_t  pty) ;

int  ml_pty_get_slave_fd( ml_pty_ptr_t  pty) ;

char *  ml_pty_get_slave_name( ml_pty_ptr_t  pty) ;

int  ml_start_config_menu( ml_pty_ptr_t  pty , char *  cmd_path ,
	int  x , int  y , char *  display) ;

#ifdef  USE_LIBSSH2
void *  ml_search_ssh_session( const char *  host , const char *  port , const char *  user) ;

int  ml_pty_set_use_loopback( ml_pty_ptr_t  pty , int  use) ;

int  ml_pty_ssh_scp( ml_pty_ptr_t  pty , ml_char_encoding_t  pty_encoding ,
	ml_char_encoding_t  path_encoding , char *  dst_path , char *  src_path ,
	int  use_scp_full) ;

void  ml_pty_ssh_set_cipher_list( const char *  list) ;

void  ml_pty_ssh_set_keepalive_interval( u_int  interval_sec) ;

int  ml_pty_ssh_keepalive( u_int  spent_msec) ;

void  ml_pty_ssh_set_use_x11_forwarding( void *  session , int  use_x11_forwarding) ;

int  ml_pty_ssh_poll( void *  fds) ;

u_int  ml_pty_ssh_get_x11_fds( int **  fds) ;

int  ml_pty_ssh_send_recv_x11( int  idx , int  bidirection) ;
#endif


#endif
