/*
 *	$Id$
 */

#include  "ml_term_manager.h"

#include  <stdio.h>		/* sprintf/sscanf */
#include  <unistd.h>		/* fork/exec */
#include  <signal.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_util.h>	/* KIK_DIGIT_STR */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>	/* kik_file_unset_cloexec */

#include  "ml_config_proto.h"


#define  MAX_TERMS  (MTU * max_terms_multiple)	/* Default MAX_TERMS is 32. */
#define  MTU        (8 * sizeof(*dead_mask))	/* MAX_TERMS unit */

#ifndef  BINDIR
#define  BINDIR "/usr/local/bin"
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static u_int  max_terms_multiple ;
static u_int32_t *  dead_mask ;

/*
 * 'terms' pointer must not be changed because ml_get_all_terms returns it directly.
 * So 'terms' array must be allocated only once.
 */
static ml_term_t **  terms ;
static u_int  num_of_terms ;

static char *  pty_list ;

static int  zombie_pty ;

static char *  auto_restart_cmd ;


/* --- static functions --- */

#if  ! defined(USE_WIN32API) && ! defined(DEBUG)
static void
sig_error(
	int  sig
	)
{
	u_int  count ;
	char  env[1024] ;
	size_t  len ;
	
	env[0] = '\0' ;
	len = 0 ;
	for( count = 0 ; count < num_of_terms ; count++)
	{
		int  master ;

		if( ( master = ml_term_get_master_fd( terms[count])) >= 0)
		{
			int  slave ;
			size_t  n ;
		
			slave = ml_term_get_slave_fd( terms[count]) ;
			snprintf( env + len , 1024 - len , "%d %d %d," ,
				master , slave , ml_term_get_child_pid( terms[count])) ;
			n = strlen( env + len) ;
			if( n + len >= 1024)
			{
				env[len] = '\0' ;
				break ;
			}
			else
			{
				len += n ;
			}

			kik_file_unset_cloexec( master) ;
			kik_file_unset_cloexec( slave) ;
		}
	}

	if( len > 0)
	{
		if( fork() > 0)
		{
			/* child process */
			setenv( "INHERIT_PTY_LIST" , env , 1) ;

			if( auto_restart_cmd)
			{
				execlp( auto_restart_cmd , auto_restart_cmd , NULL) ;
			}

			execl( BINDIR "/mlterm" , BINDIR "/mlterm" , NULL) ;

			kik_error_printf( "Failed to restart mlterm.\n") ;
		}
	}

	exit(1) ;
}
#endif

static void
sig_child(
	void *  p ,
	pid_t  pid
	)
{
	u_int  count ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " SIG_CHILD received [PID:%d].\n", pid) ;
#endif

	if( pid <= 0)
	{
		/*
		 * Note:
		 * If term->pty is NULL, ml_term_get_child_pid() returns -1.
		 */

		return ;
	}

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( pid == ml_term_get_child_pid( terms[count]))
		{
			u_int  idx ;
			
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " pty %d is dead.\n" , count) ;
		#endif

			idx = count / MTU ;
			dead_mask[idx] |= (1 << (count - MTU * idx)) ;

			return ;
		}
	}
}


/* --- global functions --- */

int
ml_term_manager_init(
	u_int  multiple
	)
{
	if( multiple > 0)
	{
		max_terms_multiple = multiple ;
	}
	else
	{
		max_terms_multiple = 1 ;
	}

	if( ( terms = malloc( sizeof( ml_term_t*) * MAX_TERMS)) == NULL)
	{
		return  0 ;
	}

	if( ( dead_mask = calloc( sizeof( *dead_mask) , max_terms_multiple)) == NULL)
	{
		free( terms) ;
		terms = NULL ;

		return  0 ;
	}

	kik_add_sig_child_listener( NULL , sig_child) ;
	ml_config_proto_init() ;

	return  1 ;
}

int
ml_term_manager_final(void)
{
	int  count ;

	kik_remove_sig_child_listener( NULL , sig_child) ;
	ml_config_proto_final() ;

	for( count = num_of_terms - 1 ; count >= 0 ; count --)
	{
	#if  0
		/*
		 * All windows may be invalid before ml_term_manager_final() is called.
		 * Without this ml_term_detach(), if terms[count] is not detached,
		 * pty_listener::pty_closed() which is called in ml_pty_delete() can
		 * operate invalid window.
		 */
		ml_term_detach( terms[count]) ;
	#endif
	
		ml_term_delete( terms[count]) ;
	}

	free( terms) ;
	
	free( dead_mask) ;
	free( pty_list) ;
	free( auto_restart_cmd) ;

	return  1 ;
}

int
ml_set_auto_restart_cmd(
	char *  cmd
	)
{
#if  ! defined(USE_WIN32API) && ! defined(DEBUG)
	if( cmd && *cmd)
	{
		if( ! auto_restart_cmd)
		{
			struct  sigaction  act ;

		#if  0
			/*
			 * sa_sigaction which is called instead of sa_handler
			 * if SA_SIGINFO is set to sa_flags is not defined in
			 * some environments.
			 */
			act.sa_sigaction = NULL ;
		#endif
			act.sa_handler = sig_error ;
			sigemptyset( &act.sa_mask) ;	/* Not blocking any signals for child. */
			act.sa_flags = SA_NODEFER ;	/* Not blocking any signals for child. */
			sigaction( SIGBUS , &act , NULL) ;
			sigaction( SIGSEGV , &act , NULL) ;
			sigaction( SIGFPE , &act , NULL) ;
			sigaction( SIGILL , &act , NULL) ;

			free( auto_restart_cmd) ;
		}

		auto_restart_cmd = strdup( cmd) ;
	}
	else if( auto_restart_cmd)
	{
		signal( SIGBUS , SIG_DFL) ;
		signal( SIGSEGV , SIG_DFL) ;
		signal( SIGFPE , SIG_DFL) ;
		signal( SIGILL , SIG_DFL) ;
		free( auto_restart_cmd) ;
		auto_restart_cmd = NULL ;
	}
#endif

	return  1 ;
}

ml_term_t *
ml_create_term(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  log_size ,
	ml_char_encoding_t  encoding ,
	int  is_auto_encoding ,
	ml_unicode_policy_t  policy ,
	int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	int  use_bidi ,
	ml_bidi_mode_t  bidi_mode ,
	int  use_ind ,
	int  use_bce ,
	int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode ,
	ml_vertical_mode_t  vertical_mode
	)
{
	char *  list ;

	if( num_of_terms == MAX_TERMS)
	{
		return  NULL ;
	}

#if  ! defined(USE_WIN32API) && ! defined(DEBUG)
	if( ( list = getenv( "INHERIT_PTY_LIST")) && ( list = kik_str_alloca_dup( list)))
	{
		int  master ;
		int  slave ;
		pid_t  child_pid ;
		char *  p ;

		while( ( p = kik_str_sep( &list , ",")))
		{
			ml_pty_ptr_t  pty ;

			if( sscanf( p , "%d %d %d" , &master , &slave , &child_pid) == 3)
			{
				/*
				 * cols + 1 is for redrawing screen by ml_set_pty_winsize() below.
				 */
				if( ( pty = ml_pty_new_with( master , slave , child_pid ,
							cols + 1 , rows)))
				{
					if( ( terms[num_of_terms] = ml_term_new( cols , rows ,
						tab_size , log_size , encoding , is_auto_encoding ,
						policy , col_size_a , use_char_combining ,
						use_multi_col_char , use_bidi , bidi_mode ,
						use_ind , use_bce ,
						use_dynamic_comb , bs_mode , vertical_mode)))
					{
						ml_term_plug_pty( terms[num_of_terms++] , pty) ;
						ml_set_pty_winsize( pty , cols , rows) ;

						continue ;
					}
					else
					{
						ml_pty_delete( pty) ;
					}
				}

				close( master) ;
				close( slave) ;
			}
		}

		unsetenv( "INHERIT_PTY_LIST") ;

		if( num_of_terms > 0)
		{
			return  terms[num_of_terms - 1] ;
		}
	}
#endif

	/*
	 * Before modifying terms and num_of_terms, do ml_close_dead_terms().
	 */
	ml_close_dead_terms() ;
	
	/*
	 * XXX
	 * If sig_child here...
	 */

	if( ( terms[num_of_terms] = ml_term_new( cols , rows , tab_size , log_size , encoding ,
				is_auto_encoding ,
				policy , col_size_a , use_char_combining , use_multi_col_char ,
				use_bidi , bidi_mode , use_ind , use_bce , use_dynamic_comb ,
				bs_mode , vertical_mode)) == NULL)
	{
		return  NULL ;
	}

	return  terms[num_of_terms++] ;
}

int
ml_destroy_term(
	ml_term_t *  term
	)
{
	int  count ;

	/*
	 * Before modifying terms and num_of_terms, do ml_close_dead_terms().
	 */
	ml_close_dead_terms() ;

	/*
	 * XXX
	 * If sig_child here...
	 */

	for( count = 0 ; count < num_of_terms ; count++)
	{
		if( terms[count] == term)
		{
			terms[count] = terms[--num_of_terms] ;

			break ;
		}
	}

	ml_term_delete( term) ;
	
	return  1 ;
}

ml_term_t *
ml_get_term(
	char *  dev
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( dev == NULL ||
		    strcmp( dev , ml_term_get_slave_name( terms[count])) == 0)
		{
			return  terms[count] ;
		}
	}

	return  NULL ;
}

ml_term_t *
ml_get_detached_term(
	char *  dev
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( ! ml_term_is_attached( terms[count]) &&
		    ( dev == NULL ||
		      strcmp( dev , ml_term_get_slave_name( terms[count])) == 0))
		{
			return  terms[count] ;
		}
	}

	return  NULL ;
}

ml_term_t *
ml_next_term(
	ml_term_t *  term	/* is detached */
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( terms[count] == term)
		{
			int  old ;
			
			old = count ;

			for( count ++ ; count < num_of_terms ; count ++)
			{
				if( ! ml_term_is_attached(terms[count]))
				{
					return  terms[count] ;
				}
			}

			for( count = 0 ; count < old ; count ++)
			{
				if( ! ml_term_is_attached(terms[count]))
				{
					return  terms[count] ;
				}
			}

			return  NULL ;
		}
	}
	
	return  NULL ;
}

ml_term_t *
ml_prev_term(
	ml_term_t *  term	/* is detached */
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( terms[count] == term)
		{
			int  old ;
			
			old = count ;

			for( count -- ; count >= 0 ; count --)
			{
				if( ! ml_term_is_attached(terms[count]))
				{
					return  terms[count] ;
				}
			}

			for( count = num_of_terms - 1 ; count > old ; count --)
			{
				if( ! ml_term_is_attached(terms[count]))
				{
					return  terms[count] ;
				}
			}

			return  NULL ;
		}
	}
	
	return  NULL ;
}

/*
 * Return value: Number of opened terms. Don't trust it after ml_create_term(),
 * ml_destroy_term() or ml_close_dead_terms() which can change it is called.
 */
u_int
ml_get_all_terms(
	ml_term_t ***  _terms
	)
{
	*_terms = terms ;

	return  num_of_terms ;
}

int
ml_close_dead_terms(void)
{
	if( num_of_terms > 0)
	{
		int  idx ;

		for( idx = (num_of_terms - 1) / MTU ; idx >= 0 ; idx --)
		{
			if( dead_mask[idx])
			{
				int  count ;

				for( count = MTU - 1 ; count >= 0 ; count --)
				{
					if( dead_mask[idx] & (0x1 << count))
					{
						ml_term_t *  term ;

					#ifdef  __DEBUG
						kik_debug_printf( KIK_DEBUG_TAG
							" closing dead term %d." , count) ;
					#endif

						term = terms[idx * MTU + count] ;
						/*
						 * Update terms and num_of_terms before
						 * ml_term_delete, which calls
						 * ml_pty_event_listener::pty_close in which
						 * ml_term_manager can be used.
						 */
						terms[idx * MTU + count] = terms[--num_of_terms] ;
						if( zombie_pty)
						{
							ml_term_zombie( term) ;
						}
						else
						{
							ml_term_delete( term) ;
						}

					#ifdef  __DEBUG
						kik_msg_printf( " => Finished.\n") ;
					#endif
					}
				}

				memset( &dead_mask[idx] , 0 , sizeof(dead_mask[idx])) ;
			}
		}
	}
	
	return  1 ;
}

char *
ml_get_pty_list(void)
{
	int  count ;
	char *  p ;
	size_t  len ;

	free( pty_list) ;

	/* The length of pty name is under 50. */
	len = (50 + 2) * num_of_terms ;
	
	if( ( pty_list = malloc( len + 1)) == NULL)
	{
		return  "" ;
	}

	p = pty_list ;

	*p = '\0' ;
	
	for( count = 0 ; count < num_of_terms ; count ++)
	{
		kik_snprintf( p , len , "%s:%d;" ,
			ml_term_get_slave_name( terms[count]) ,
			ml_term_is_attached( terms[count])) ;

		len -= strlen( p) ;
		p += strlen( p) ;
	}

	return  pty_list ;
}

void
ml_term_manager_enable_zombie_pty(void)
{
	zombie_pty = 1 ;
}
