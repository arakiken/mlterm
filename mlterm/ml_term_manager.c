/*
 *	$Id$
 */

#include  "ml_term_manager.h"

#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_sig_child.h>


/*
 * max_ptys is 32 , which is the limit of dead_mask(32bit).
 */
#define  MAX_TERMS  32


typedef struct  term_wrapper
{
	ml_term_t *  term ;
	int8_t  is_active ;
	
} term_wrapper_t ;


/* --- static variables --- */

static u_int32_t  dead_mask ;

static ml_term_t *  terms[MAX_TERMS] ;
static int8_t  is_active[MAX_TERMS] ;
static u_int  num_of_terms ;


/* --- static functions --- */

static void
sig_child(
	void *  p ,
	pid_t  pid
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( pid == ml_term_get_child_pid( terms[count]))
		{
			dead_mask |= (1 << count) ;

			return ;
		}
	}
}


/* --- global functions --- */

int
ml_term_manager_init(void)
{
	kik_add_sig_child_listener( NULL , sig_child) ;

	return  1 ;
}

int
ml_term_manager_final(void)
{
	int  count ;

	kik_remove_sig_child_listener( NULL , sig_child) ;

	for( count = num_of_terms - 1 ; count >= 0 ; count --)
	{
		ml_term_delete( terms[count]) ;
	}

	return  1 ;
}

ml_term_t *
ml_pop_term(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  log_size ,
	ml_char_encoding_t  encoding ,
	int  not_use_unicode_font ,
	int  only_use_unicode_font ,
	int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	int  use_bce
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( ! is_active[count])
		{
		#if  0
			ml_term_resize( terms[count] , cols , rows) ;
			ml_term_change_log_size( terms[count] , log_size) ;
		#endif
		#if  0
			ml_term_change_encoding( terms[count] , encoding) ;
			ml_term_set_char_combining_flag( terms[count] , use_char_combining) ;
			ml_term_set_multi_col_char_flag( terms[count] , use_multi_col_char) ;
		#endif
		
			is_active[count] = 1 ;

			return  terms[count] ;
		}
	}

	if( num_of_terms == MAX_TERMS)
	{
		return  NULL ;
	}

	if( ( terms[num_of_terms] = ml_term_new( cols , rows , tab_size , log_size , encoding ,
				not_use_unicode_font , only_use_unicode_font , col_size_a ,
				use_char_combining , use_multi_col_char , use_bce)) == NULL)
	{
		return  NULL ;
	}

	is_active[num_of_terms] = 1 ;

	return  terms[num_of_terms++] ;
}

int
ml_put_back_term(
	ml_term_t *  term
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( terms[count] == term)
		{
			is_active[count] = 0 ;

			break ;
		}
	}

	return  1 ;
}

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
	if( dead_mask)
	{
		int  count ;

		for( count = num_of_terms - 1 ; count >= 0 ; count --)
		{
			if( dead_mask & (0x1 << count))
			{
				ml_term_delete( terms[count]) ;

				terms[count] = terms[num_of_terms - 1] ;
				is_active[count] = is_active[num_of_terms - 1] ;
				num_of_terms -- ;
			}
		}

		dead_mask = 0 ;
	}
	
	return  1 ;
}
