/*
 *	$Id$
 */

#include  "ml_term_manager.h"

#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_util.h>	/* KIK_DIGIT_STR */


/*
 * max_ptys is 32 , which is the limit of dead_mask(32bit).
 */
#define  MAX_TERMS  32


/* --- static variables --- */

static u_int32_t  dead_mask ;

static ml_term_t *  terms[MAX_TERMS] ;
static u_int  num_of_terms ;
static char *  pty_list ;


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

	free( pty_list) ;

	return  1 ;
}

ml_term_t *
ml_create_term(
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
	int  use_bidi ,
	int  use_bce ,
	int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode ,
	ml_vertical_mode_t  vertical_mode ,
	ml_iscii_lang_type_t  iscii_lang_type
	)
{
	if( num_of_terms == MAX_TERMS)
	{
		return  NULL ;
	}

	if( ( terms[num_of_terms] = ml_term_new( cols , rows , tab_size , log_size , encoding ,
				not_use_unicode_font , only_use_unicode_font , col_size_a ,
				use_char_combining , use_multi_col_char , use_bidi , use_bce ,
				use_dynamic_comb , bs_mode , vertical_mode , iscii_lang_type)) == NULL)
	{
		return  NULL ;
	}

	return  terms[num_of_terms++] ;
}

ml_term_t *
ml_get_term(
	char *  dev
	)
{
	int  count ;

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( ( dev == NULL || strcmp( dev , ml_term_get_slave_name( terms[count])) == 0) &&
			! ml_term_is_attached( terms[count]))
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
				num_of_terms -- ;
			}
		}

		dead_mask = 0 ;
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

	/* the length of "/dev/..." is mostly under 20. */
	len = (20 + 2) * num_of_terms + 1 ;
	
	if( ( pty_list = malloc( len)) == NULL)
	{
		return  "" ;
	}

	p = pty_list ;
	
	for( count = 0 ; count < num_of_terms ; count ++)
	{
		kik_snprintf( p , len , "%s:%d;" ,
			ml_term_get_slave_name( terms[count]) , ml_term_is_attached( terms[count])) ;
		p += strlen( p) ;
	}

	return  pty_list ;
}
