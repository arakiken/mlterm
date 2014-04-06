/*
 *	$Id$
 */

#include  "x_event_source.h"

#include  <stdio.h>	/* sprintf */
#include  <kiklib/kik_debug.h>
#include  <ml_char_encoding.h>
#include  <ml_term_manager.h>

#include  "x_display.h"


/* --- static variables --- */

static char *  commit_text ;
static char *  preedit_text ;
static ALooper *  looper ;
static mkf_parser_t *  utf8_parser ;


/* --- static functions --- */

int  ml_vt100_parser_preedit( ml_vt100_parser_t * , u_char * , size_t) ;

static void
update_ime_text(
	ml_term_t *  term
	)
{
	u_char  buf[128] ;
	size_t  len ;

	if( ! utf8_parser && ! ( utf8_parser = ml_parser_new( ML_UTF8)))
	{
		return ;
	}

	(*utf8_parser->init)( utf8_parser) ;

	ml_term_set_use_local_echo( term , 0) ;

	if( preedit_text)
	{
		ml_term_set_use_local_echo( term , 1) ;

		(*utf8_parser->set_str)( utf8_parser , preedit_text , strlen(preedit_text)) ;
		while( ! utf8_parser->is_eos &&
		       ( len = ml_term_convert_to( term , buf , sizeof(buf) , utf8_parser)) > 0)
		{
			ml_vt100_parser_preedit( term->parser , buf , len) ;
		}

		x_window_update( term->parser->xterm_listener->self , 3) ;

		free( preedit_text) ;
		preedit_text = NULL ;
	}
	else /* if( commit_text) */
	{
		(*utf8_parser->set_str)( utf8_parser , commit_text , strlen(commit_text)) ;
		while( ! utf8_parser->is_eos &&
		       ( len = ml_term_convert_to( term , buf , sizeof(buf) , utf8_parser)) > 0)
		{
			ml_term_write( term , buf , len , 0) ;
		}

		free( commit_text) ;
		commit_text = NULL ;
	}
}

static void
ALooper_removeFds(
	int *  fds ,
	u_int  num_of_fds
	)
{
	u_int  count ;

	for( count = 0 ; count < num_of_fds ; count++)
	{
		ALooper_removeFd( looper , fds[count]) ;
	}
}


/* --- global functions --- */

int
x_event_source_init(void)
{
	return  1 ;
}

int
x_event_source_final(void)
{
	return  1 ;
}

int
x_event_source_process(void)
{
	int  ident ;
	int  events ;
	struct android_poll_source *  source ;
	ml_term_t **  terms ;
	u_int  num_of_terms ;
	u_int  count ;
	static u_int  prev_num_of_terms ;
	static int *  fds ;

	looper = ALooper_forThread() ;

	if( ( num_of_terms = ml_get_all_terms( &terms)) != prev_num_of_terms)
	{
		void *  p ;

		ALooper_removeFds( fds , prev_num_of_terms) ;
		prev_num_of_terms = 0 ;

		if( num_of_terms == 0)
		{
			x_display_final() ;
		}
		else if( ( p = realloc( fds , sizeof(int) * num_of_terms)))
		{
			fds = p ;
			prev_num_of_terms = num_of_terms ;

			for( count = 0 ; count < num_of_terms ; count++)
			{
				fds[count] = ml_term_get_master_fd( terms[count]) ;
				ALooper_addFd( looper , fds[count] , 1000 + fds[count] ,
					ALOOPER_EVENT_INPUT , NULL , NULL) ;
			}
		}
	}

	/*
	 * Read all pending events.
	 * Don't block ALooper_pollAll because commit_text or preedit_text can
	 * be changed from main thread.
	 */
	if( ( ident = ALooper_pollAll(
				100 , /* milisec. -1 blocks forever waiting for events */
				NULL , &events, (void**)&source)) >= 0)
	{
		if( ! x_display_process_event( source , ident))
		{
			ALooper_removeFds( fds , prev_num_of_terms) ;
			prev_num_of_terms = 0 ;
			free( fds) ;
			fds = NULL ;

			return  0 ;
		}

		for( count = 0 ; count < num_of_terms ; count ++)
		{
			if( ml_term_get_master_fd( terms[count]) + 1000 == ident)
			{
				ml_term_parse_vt100_sequence( terms[count]) ;

				/*
				 * Don't break here because some terms can have
				 * the same master fd.
				 */
			}
		}
	}

	if( preedit_text || commit_text)
	{
		for( count = 0 ; count < num_of_terms ; count++)
		{
			if( ml_term_is_attached( terms[count]))
			{
				update_ime_text( terms[count]) ;

				break ;
			}
		}
	}

	x_display_idling( NULL) ;

	x_display_unlock() ;

	ml_close_dead_terms() ;

	return  1 ;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int
x_event_source_add_fd(
	int  fd ,
	void  (*handler)(void)
	)
{
	return  0 ;
}

int
x_event_source_remove_fd(
	int  fd
	)
{
	return  0 ;
}

void
Java_mlterm_native_1activity_MLActivity_commitText(
	JNIEnv *  env ,
	jobject  this ,
	jstring  jstr
	)
{
	const char *  str ;

	free( commit_text) ;
	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
	commit_text = strdup( str) ;
	(*env)->ReleaseStringUTFChars( env , jstr , str) ;

	free( preedit_text) ;
	preedit_text = NULL ;

	ALooper_wake( looper) ;
}

#if  0
void
Java_mlterm_native_1activity_MLActivity_preeditText(
	JNIEnv *  env ,
	jobject  this ,
	jstring  jstr1 ,
	jstring  jstr2
	)
{
	const char *  str1 ;
	const char *  str2 ;

	str1 = (*env)->GetStringUTFChars( env , jstr1 , NULL) ;
	str2 = (*env)->GetStringUTFChars( env , jstr2 , NULL) ;

	free( preedit_text) ;
	if( ( preedit_text = malloc( strlen(str1) + 2 + strlen(str2) + 2 + 1)))
	{
		sprintf( preedit_text , "%s\x1b\x37%s\x1b\x38" , str1 , str2) ;
	}

	(*env)->ReleaseStringUTFChars( env , jstr1 , str1) ;
	(*env)->ReleaseStringUTFChars( env , jstr2 , str2) ;

	ALooper_wake( looper) ;
}
#else
void
Java_mlterm_native_1activity_MLActivity_preeditText(
	JNIEnv *  env ,
	jobject  this ,
	jstring  jstr
	)
{
	const char *  str ;

	free( preedit_text) ;
	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
	preedit_text = strdup( str) ;
	(*env)->ReleaseStringUTFChars( env , jstr , str) ;

	ALooper_wake( looper) ;
}
#endif
