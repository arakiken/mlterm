/*
 *	$Id$
 */

#include  "ml_term_manager.h"

#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_util.h>	/* KIK_DIGIT_STR */

#include  "ml_config_proto.h"


/* --- static variables --- */

static u_long  dead_mask ;
#define  MAX_TERMS  (8*sizeof(dead_mask))

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
	int  is_auto_encoding ,
	ml_unicode_font_policy_t  policy ,
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
				is_auto_encoding ,
				policy , col_size_a , use_char_combining , use_multi_col_char ,
				use_bidi , use_bce , use_dynamic_comb , bs_mode , vertical_mode ,
				iscii_lang_type)) == NULL)
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
		if( dev == NULL || strcmp( dev , ml_term_get_slave_name( terms[count])) == 0)
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
			ml_term_get_slave_name( terms[count]) , ml_term_is_attached( terms[count])) ;

		len -= strlen( p) ;
		p += strlen( p) ;
	}

	return  pty_list ;
}
