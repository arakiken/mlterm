/*
 *	$Id$
 */

#ifndef  __MKF_CHAR_H__
#define  __MKF_CHAR_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_property.h"
#include  "mkf_charset.h"


/*
 * use UNMAP_FROM_GR or MAP_TO_GR to operate gr byte.
 * these are for 8bit cs(ISO8859-R...).
 */
#define  SET_MSB(ch)  ((ch) |= 0x80)
#define  UNSET_MSB(ch)  ((ch) &= 0x7f)

/* UCS-4 is the max. */
#define  MAX_CS_BYTELEN  4


/*
 * this should be kept as small as possible.
 */
typedef struct  mkf_char
{
	u_char  ch[MAX_CS_BYTELEN] ;	/* Big Endian */

	u_int8_t  size ;
	u_int8_t  property ;	/* mkf_property_t */
	int16_t  cs ;		/* mkf_charset_t */
	
} mkf_char_t ;


u_int32_t  mkf_char_to_int( mkf_char_t *  ch) ;

u_char *  mkf_int_to_bytes( u_char *  bytes , size_t  len , u_int32_t  int_ch) ;

u_int32_t  mkf_bytes_to_int( u_char *  bytes , size_t  len) ;


#endif
