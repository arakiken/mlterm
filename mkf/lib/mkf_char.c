/*
 *	$Id$
 */

#include  "mkf_char.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

inline u_int32_t
mkf_char_to_int(
	mkf_char_t *  ch
	)
{
	if( ch->size == 1)
	{
		return  ch->ch[0] ;
	}
	else if( ch->size == 2)
	{
		return  ( ( ch->ch[0] << 8) & 0xff00) + ch->ch[1] ;
	}
	else if( ch->size == 4)
	{
		return (( ch->ch[0] << 24) & 0xff000000) + (( ch->ch[1] << 16) & 0x00ff0000) +
			(( ch->ch[2] << 8) & 0x0000ff00) + ch->ch[3] ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " mkf_char_t couldn't be converted to int.\n") ;
	#endif
	
		return  0 ;
	}
}

inline u_char *
mkf_int_to_bytes(
	u_char *  bytes ,
	size_t  len ,
	u_int32_t  int_ch
	)
{
	if( len == 1)
	{
		bytes[0] = int_ch & 0xff ;
	}
	else if( len == 2)
	{
		bytes[0] = (int_ch >> 8) & 0xff ;
		bytes[1] = int_ch & 0xff ;
	}
	else if( len == 4)
	{
		bytes[0] = (int_ch >> 24) & 0xff ;
		bytes[1] = (int_ch >> 16) & 0xff ;
		bytes[2] = (int_ch >> 8) & 0xff ;
		bytes[3] = int_ch & 0xff ;
	}
	else
	{
		return  NULL ;
	}

	return  bytes ;
}

inline u_int32_t
mkf_bytes_to_int(
	u_char *  bytes ,
	size_t  len
	)
{
	if( len == 1)
	{
		return  bytes[0] ;
	}
	else if( len == 2)
	{
		return  (bytes[0] << 8) + bytes[1] ;
	}
	else if( len == 4)
	{
		return  (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3] ;
	}
	else
	{
		return  0 ;
	}
}
