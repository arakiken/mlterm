/*
 *	$Id$
 */

#include  "mkf_parser.h"

#include  <stdio.h>	/* NULL */
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

void
mkf_parser_init(
	mkf_parser_t *  parser
	)
{
	parser->str = NULL ;
	parser->marked_left = 0 ;
	parser->left = 0 ;
	
	parser->is_eos = 0 ;
}

inline size_t
__mkf_parser_increment(
	mkf_parser_t *  parser
	)
{
	if( parser->left <= 1)
	{
		parser->str += parser->left ;
		parser->left = 0 ;
		parser->is_eos = 1 ;
	}
	else
	{
		parser->str ++ ;
		parser->left -- ;
	}

	return  parser->left ;
}

inline size_t
__mkf_parser_n_increment(
	mkf_parser_t *  parser ,
	size_t  n
	)
{
	if( parser->left <= n)
	{
		parser->str += parser->left ;
		parser->left = 0 ;
		parser->is_eos = 1 ;
	}
	else
	{
		parser->str += n ;
		parser->left -= n ;
	}

	return  parser->left ;
}

inline void
__mkf_parser_mark(
	mkf_parser_t *  parser
	)
{
	parser->marked_left = parser->left ;
}

inline void
__mkf_parser_reset(
	mkf_parser_t *  parser
	)
{
	parser->str -= (parser->marked_left - parser->left) ;
	parser->left = parser->marked_left ;
}

/*
 * short cut function. (ignoring error)
 */
int
mkf_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	while( 1)
	{
	#if  0
		/*
		 * just to be sure...
		 * mkf_parser_mark() should be called inside [encoding]_next_char() function.
		 */
		mkf_parser_mark( parser) ;
	#endif
	
		if( (*parser->next_char)( parser , ch))
		{
			return  1 ;
		}
		else if( parser->is_eos)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" parser reached the end of string.\n") ;
		#endif

			return  0 ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" parser->next_char() returns error , continuing...\n") ;
		}
	#endif
	}
}
