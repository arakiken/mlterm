/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_CONV_H__
#define  __MKF_UCS4_CONV_H__


#include  "mkf_conv.h"


mkf_conv_t *  mkf_ucs4_conv_new(void) ;

/* alias of mkf_ucs4_conv_new */
mkf_conv_t *  mkf_utf32_conv_new(void) ;

#if  0
/* not implemented yet */
mkf_conv_t *  mkf_ucs4le_conv_new(void) ;

mkf_conv_t *  mkf_utf32le_conv_new(void) ;
#endif


#endif
