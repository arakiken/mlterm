/*
 *	$Id$
 */

#include  "x_event_source.h"

#include  <stdio.h>	/* sprintf */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <ml_char_encoding.h>
#include  <ml_term_manager.h>

#include  "x_display.h"


/* --- static variables --- */

static mkf_parser_t *  utf8_parser ;
/* main and native activity threads changes commit_text/preedit_text from at the same time. */
static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER ;
static char *  cur_preedit_text ;


/* --- static functions --- */

static void
update_ime_text(
	ml_term_t *  term ,
	char *  preedit_text ,
	char *  commit_text
	)
{
	u_char  buf[128] ;
	size_t  len ;

	if( ! utf8_parser && ! ( utf8_parser = ml_parser_new( ML_UTF8)))
	{
		return ;
	}

	(*utf8_parser->init)( utf8_parser) ;

	ml_term_set_config( term , "use_local_echo" , "false") ;

	if( preedit_text)
	{
		if( *preedit_text == '\0')
		{
			preedit_text = NULL ;
		}
		else
		{
			if( kik_compare_str( preedit_text , cur_preedit_text) == 0)
			{
				return ;
			}

			ml_term_set_config( term , "use_local_echo" , "true") ;

			(*utf8_parser->set_str)( utf8_parser , preedit_text ,
				strlen(preedit_text)) ;
			while( ! utf8_parser->is_eos &&
			       ( len = ml_term_convert_to( term , buf , sizeof(buf) ,
						utf8_parser)) > 0)
			{
				ml_vt100_parser_preedit( term->parser , buf , len) ;
			}

			x_window_update( term->parser->xterm_listener->self , 3) ;
		}
	}
	else /* if( commit_text) */
	{
		(*utf8_parser->set_str)( utf8_parser , commit_text , strlen(commit_text)) ;
		while( ! utf8_parser->is_eos &&
		       ( len = ml_term_convert_to( term , buf , sizeof(buf) , utf8_parser)) > 0)
		{
			ml_term_write( term , buf , len) ;
		}
	}

	free( cur_preedit_text) ;
	cur_preedit_text = preedit_text ? strdup( preedit_text) : NULL ;
}

static void
update_ime_text_on_active_term(
	char *  preedit_text ,
	char *  commit_text
	)
{
	ml_term_t **  terms ;
	u_int  num_of_terms ;
	u_int  count ;

	num_of_terms = ml_get_all_terms( &terms) ;

	for( count = 0 ; count < num_of_terms ; count++)
	{
		if( ml_term_is_attached( terms[count]))
		{
			update_ime_text( terms[count] , preedit_text , commit_text) ;

			return ;
		}
	}
}

static void
ALooper_removeFds(
	ALooper *  looper ,
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
	ALooper *  looper ;
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

		ALooper_removeFds( looper , fds , prev_num_of_terms) ;
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
	ident = ALooper_pollAll(
				100 , /* milisec. -1 blocks forever waiting for events */
				NULL , &events, (void**)&source) ;

	pthread_mutex_lock( &mutex) ;

	if( ident >= 0)
	{
		if( ! x_display_process_event( source , ident))
		{
			ALooper_removeFds( looper , fds , prev_num_of_terms) ;
			prev_num_of_terms = 0 ;
			free( fds) ;
			fds = NULL ;

			pthread_mutex_unlock( &mutex) ;

			return  0 ;
		}

		for( count = 0 ; count < num_of_terms ; count ++)
		{
			if( ml_term_get_master_fd( terms[count]) + 1000 == ident)
			{
				if( cur_preedit_text)
				{
					ml_term_set_config( terms[count] ,
						"use_local_echo" , "false") ;
				}

				ml_term_parse_vt100_sequence( terms[count]) ;

				if( cur_preedit_text)
				{
					char *  preedit_text ;

					preedit_text = cur_preedit_text ;
					cur_preedit_text = NULL ;
					update_ime_text( terms[count] , preedit_text , NULL) ;
				}

				/*
				 * Don't break here because some terms can have
				 * the same master fd.
				 */
			}
		}
	}
	else
	{
		x_display_idling( NULL) ;
	}

	ml_close_dead_terms() ;

	pthread_mutex_unlock( &mutex) ;

	x_display_unlock() ;

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
Java_mlterm_native_1activity_MLActivity_visibleFrameChanged(
	JNIEnv *  env ,
	jobject  this ,
	jint  yoffset ,
	jint  width ,
	jint  height
	)
{
	pthread_mutex_lock( &mutex) ;

	x_display_resize( yoffset , width , height) ;

	pthread_mutex_unlock( &mutex) ;
}

void
Java_mlterm_native_1activity_MLActivity_commitText(
	JNIEnv *  env ,
	jobject  this ,
	jstring  jstr
	)
{
	char *  str ;

	pthread_mutex_lock( &mutex) ;

	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
	update_ime_text_on_active_term( NULL , str) ;
	(*env)->ReleaseStringUTFChars( env , jstr , str) ;

	pthread_mutex_unlock( &mutex) ;
}

void
Java_mlterm_native_1activity_MLActivity_preeditText(
	JNIEnv *  env ,
	jobject  this ,
	jstring  jstr
	)
{
	char *  str ;

	pthread_mutex_lock( &mutex) ;

	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
	update_ime_text_on_active_term( str , NULL) ;
	(*env)->ReleaseStringUTFChars( env , jstr , str) ;

	pthread_mutex_unlock( &mutex) ;
}
