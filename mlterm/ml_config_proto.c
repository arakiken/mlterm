/*
 *	$Id$
 */

#include  "ml_config_proto.h"

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <fcntl.h>	/* open/creat */
#include  <unistd.h>	/* close */
#include  <time.h>	/* time */
#include  <sys/stat.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_util.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static char *  challenge ;
static char *  path ;


/* --- static functions --- */

static int
read_challenge(void)
{
	FILE *  file ;
	struct stat  st ;

	if( ( file = fopen( path , "r")) == NULL)
	{
		return  0 ;
	}

	fstat( fileno( file) , &st) ;

	if( st.st_size > DIGIT_STR_LEN(int))
	{
		return  0 ;
	}

	free( challenge) ;
	
	if( ( challenge = malloc( DIGIT_STR_LEN(int) + 1)) == NULL)
	{
		return  0 ;
	}

	fread( challenge , st.st_size , 1 , file) ;
	challenge[st.st_size] = '\0' ;

	return  1 ;
}

static int
challenge_it(
	char *  chal
	)
{
	if( challenge && strcmp( chal , challenge) == 0)
	{
		return  1 ;
	}
	
	/*
	 * Challenge could be re-generated.
	 */
	if( read_challenge() && challenge && strcmp( chal , challenge) == 0)
	{
		return  1 ;
	}
	
	return  0 ;
}


/* --- global functions --- */

int
ml_config_proto_init(void)
{
	if( ( path = kik_get_user_rc_path( "mlterm/challenge")) == NULL)
	{
		return  0 ;
	}

	return  ml_gen_proto_challenge() ;
}

int
ml_gen_proto_challenge(void)
{
	int  fd ;

	if( ( fd = creat( path , 0600)) == -1)
	{
		return  0 ;
	}

	free( challenge) ;
	
	if( ( challenge = malloc( DIGIT_STR_LEN(int) + 1)) == NULL)
	{
		return  0 ;
	}

	srand( (u_int)(time(NULL) + challenge)) ;
	sprintf( challenge , "%d" , rand()) ;

	write( fd , challenge , strlen(challenge)) ;

	close( fd) ;

	return  1 ;
}

/*
 * Returns 0 if error happens.
 * Returns -1 if do_challenge is 1 and challenge failed.
 */
int
ml_parse_proto(
	char **  dev ,
	char **  key ,
	char **  val ,
	char **  str ,
	int  do_challenge
	)
{
	char *  p ;

	p = *str ;
	
	if( do_challenge)
	{
		char *  chal ;

		chal = p ;

		if( ( p = strchr( p , ';')) == NULL)
		{
			/* Illegal format */

		#ifndef  KIK_DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Illegal protocol format.\n") ;
		#endif

			return  0 ;
		}

		*(p ++) = '\0' ;

		if( ! challenge_it( chal))
		{
			kik_msg_printf( "Protocol 5380 is not permitted "
				"because client password is wrong.\n") ;

			return  -1 ;
		}
	}

	if( ( *str = strchr( p , ';')))
	{
		*((*str) ++) = '\0' ;
	}

	if( strncmp( p , "/dev" , 4) == 0)
	{
		if( dev)
		{
			*dev = p ;
		}

		if( ( p = strchr( p , ':')) == NULL)
		{
			/* Illegal format */

		#ifndef  KIK_DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Illegal protocol format.\n") ;
		#endif

			return  0 ;
		}

		*(p ++) = '\0' ;
	}
	else
	{
		if( dev)
		{
			*dev = NULL ;
		}
	}
	
	if( key)
	{
		*key = p ;
	}

	if( ( p = strchr( p , '=')))
	{
		*(p ++) = '\0' ;

		if( val)
		{
			*val = p ;
		}
	}
	else
	{
		if( val)
		{
			*val = NULL ;
		}
	}
	
#ifdef  __DEBUG
	kik_debug_printf( "%s %s %s\n" , key ? *key : NULL , val ? *val : NULL , dev ? *dev : NULL) ;
#endif

	return  1 ;
}
