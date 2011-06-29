/*
 *	$Id$
 */

#include  "ml_iscii.h"


#if  0
#define  __DEBUG
#endif


#ifdef  USE_IND

#include  <ctype.h>		/* isdigit */
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <indian.h>


#ifndef  LIBDIR
#define  INDLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  INDLIB_DIR  LIBDIR "/mlterm/"
#endif

#define  A2IMAXBUFF  30


struct  ml_isciikey_state
{
	/* used for iitkeyb */
	char  prev_key[A2IMAXBUFF] ;

	int8_t  is_inscript ;

} ;


/* --- static variables --- */

static char *  iscii_table_files[] =
{
	"ind_assamese" ,
	"ind_bengali" ,
	"ind_gujarati" ,
	"ind_hindi" ,
	"ind_kannada" ,
	"ind_malayalam" ,
	"ind_oriya" ,
	"ind_punjabi" ,
	"ind_roman" ,
	"ind_tamil" ,
	"ind_telugu" ,
} ;

static struct tabl *  (*get_iscii_tables[11])( u_int *) ;
static struct a2i_tabl *  (*get_inscript_table)( u_int *) ;
static struct a2i_tabl *  (*get_iitkeyb_table)( u_int *) ;


/* --- static functions --- */

static void *
load_symbol(
	char *  file
	)
{
	void *  handle ;
	void *  sym ;

	if( ! ( handle = kik_dl_open( INDLIB_DIR , file)))
	{
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open %s\n" , file) ;
		return  NULL ;
	}

	if( ! ( sym = kik_dl_func_symbol( handle , "libind_get_table")))
	{
		kik_dl_close( handle) ;
	}

	return  sym ;
}

static struct tabl *
get_iscii_table(
	int  idx ,
	size_t *  size
	)
{
	if( ! get_iscii_tables[idx] &&
	    ! (get_iscii_tables[idx] = load_symbol( iscii_table_files[idx])))
	{
		return  NULL ;
	}

	return  (*get_iscii_tables[idx])( size) ;
}

static struct a2i_tabl *
get_isciikey_table(
	int  is_inscript ,
	size_t *  size
	)
{
	if( is_inscript)
	{
		if( ! get_inscript_table &&
		    ! ( get_inscript_table = load_symbol( "ind_inscript")))
		{
			return  NULL ;
		}

		return  (*get_inscript_table)( size) ;
	}
	else
	{
		if( ! get_iitkeyb_table &&
		    ! ( get_iitkeyb_table = load_symbol( "ind_iitkeyb")))
		{
			return  NULL ;
		}

		return  (*get_iitkeyb_table)( size) ;
	}
}


/* --- global functions --- */

u_int
ml_iscii_shape(
	mkf_charset_t  cs ,
	u_char *  dst ,
	size_t  dst_size ,
	u_char *  src
	)
{
	struct tabl *  table ;
	size_t  size ;

	if( ! IS_ISCII(cs))
	{
		return  0 ;
	}

	if( ( table = get_iscii_table( cs - ISCII_ASSAMESE , &size)) == NULL)
	{
		return  0 ;
	}

	/*
	 * XXX
	 * iscii2font() expects dst to be terminated by zero.
	 * int iscii2font(struct tabl table[MAXLEN], char *input, char *output, int sz) {
	 *	...
	 *	bzero(output,strlen(output));
	 *	...          ^^^^^^^^^^^^^^
	 * }
	 */
	dst[0] = '\0' ;

	return  iscii2font( table , src , dst , size) ;
}


ml_iscii_state_t *
ml_iscii_new(void)
{
	ml_iscii_state_t *  state ;
	
	if( ! ( state = malloc( sizeof( ml_iscii_state_t))))
	{
		return  NULL ;
	}

	state->num_of_chars_array = NULL ;
	state->size = 0 ;

	state->has_iscii = 0 ;

	return  state ;
}

int
ml_iscii_copy(
	ml_iscii_state_t *  dst ,
	ml_iscii_state_t *  src
	)
{
	u_int8_t *  p ;

	if( ! ( p = realloc( dst->num_of_chars_array , sizeof( u_int8_t) * src->size)))
	{
		return  0 ;
	}

	dst->num_of_chars_array = p ;
	dst->size = src->size ;

	memcpy( dst->num_of_chars_array , src->num_of_chars_array ,
			sizeof( u_int8_t) * src->size) ;

	return  1 ;
}

int
ml_iscii_delete(
	ml_iscii_state_t *  state
	)
{
	free( state->num_of_chars_array) ;
	free( state) ;

	return  1 ;
}

int
ml_iscii_reset(
	ml_iscii_state_t *  state
	)
{
	state->size = 0 ;

	return  1 ;
}

int
ml_iscii(
	ml_iscii_state_t *  state ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	int  dst_pos ;
	int  src_pos ;
	u_char *  iscii_buf ;
	u_int  iscii_buf_len ;
	u_char *  font_buf ;
	u_int  font_buf_len ;
	u_int  prev_font_filled ;
	u_int  iscii_filled ;
	mkf_charset_t  cs ;

	iscii_buf_len = src_len * 4 + 1 ;
	if( ( iscii_buf = alloca( iscii_buf_len)) == NULL)
	{
		return  0 ;
	}

	font_buf_len = src_len * 4 + 1 ;
	if( ( font_buf = alloca( font_buf_len)) == NULL)
	{
		return  0 ;
	}

	if( ( state->num_of_chars_array = realloc( state->num_of_chars_array ,
						font_buf_len * sizeof(u_int8_t))) == NULL)
	{
		return  0 ;
	}

	state->has_iscii = 0 ;
	dst_pos = -1 ;
	prev_font_filled = 0 ;
	iscii_filled = 0 ;
	cs = UNKNOWN_CS ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		if( cs != ml_char_cs( src + src_pos))
		{
			prev_font_filled = 0 ;
			iscii_filled = 0 ;
		}

		cs = ml_char_cs( src + src_pos) ;
		
		if( IS_ISCII( cs))
		{
			u_int  font_filled ;
			
			iscii_buf[iscii_filled ++] = ml_char_bytes( src + src_pos)[0] ;
			iscii_buf[iscii_filled] = '\0' ;
			font_filled = ml_iscii_shape( cs , font_buf , font_buf_len , iscii_buf) ;

			if( font_filled < prev_font_filled)
			{
				if( prev_font_filled - font_filled > dst_pos)
				{
					font_filled = prev_font_filled - dst_pos ;
				}
				
				dst_pos -= (prev_font_filled - font_filled) ;
				prev_font_filled = font_filled ;
			}

			if( dst_pos >= 0 && font_filled == prev_font_filled)
			{
				state->num_of_chars_array[dst_pos] ++ ;
			}
			else
			{
				u_int  count ;

				state->num_of_chars_array[++dst_pos] = 1 ;

				for( count = 1 ; count < font_filled - prev_font_filled ; count++)
				{
					state->num_of_chars_array[++dst_pos] = 0 ;
				}
			}

			prev_font_filled = font_filled ;

			state->has_iscii = 1 ;
		}
		else
		{
			state->num_of_chars_array[++dst_pos] = 1 ;
		}
	}

	state->size = dst_pos + 1 ;

	return  1 ;
}

ml_isciikey_state_t
ml_isciikey_state_new(
	int  is_inscript
	)
{
	ml_isciikey_state_t  state ;

	if( ( state = malloc( sizeof( struct ml_isciikey_state))) == NULL)
	{
		return  NULL ;
	}

	state->is_inscript = is_inscript ;
	state->prev_key[0] = '\0' ;

	return  state ;
}

int
ml_isciikey_state_delete(
	ml_isciikey_state_t  state
	)
{
	free( state) ;

	return  1 ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_isciikey_state_t  state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	struct a2i_tabl *  table ;
	size_t  size ;
	u_char *  dup ;

	/*
	 * ins2iscii() and iitk2iscii() return 2nd argument variable whose memory
	 * is modified by converted iscii bytes.
	 * So, enough memory (* A2IMAXBUFF) should be allocated here.
	 */
	if( ( dup = alloca( ascii_len * A2IMAXBUFF)) == NULL)
	{
		goto  no_conv ;
	}

	if( ( table = get_isciikey_table( state->is_inscript , &size)) == NULL)
	{
		goto  no_conv ;
	}

	strncpy( dup , ascii , ascii_len) ;
	dup[ascii_len] = '\0' ;

	if( state->is_inscript)
	{
		kik_snprintf( iscii , iscii_len , "%s" ,
			ins2iscii( table , dup , size)) ;
	}
	else
	{
		kik_snprintf( iscii , iscii_len , "%s" ,
			iitk2iscii( table , dup , state->prev_key , size)) ;

		state->prev_key[0] = ascii[0] ;
		state->prev_key[1] = '\0' ;
	}

	return  strlen( iscii) ;
	
no_conv:
	memmove( iscii , ascii , iscii_len) ;

	return  ascii_len ;
}


#else  /* USE_IND */


/* --- global functions --- */

ml_isciikey_state_t
ml_isciikey_state_new(
	int  is_inscript
	)
{
	return  NULL ;
}

int
ml_isciikey_state_delete(
	ml_isciikey_state_t  state
	)
{
	return  0 ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_isciikey_state_t  state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	return  0 ;
}


#endif	/* USE_IND */
