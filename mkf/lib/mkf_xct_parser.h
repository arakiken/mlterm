/*
 *	update: <2001/10/19(17:52:48)>
 *	$Id$
 */

#ifndef  __MKF_XCT_PARSER_H__
#define  __MKF_XCT_PARSER_H__


#include  "mkf_iso8859_parser.h"


/*
 * XCOMPOUND_TEXT format is almost the same as ISO8859-1 encoding.
 * G0 = GL = US ASCII
 * G1 = GR = ISO8859-1
 */
#define  mkf_xct_parser_new()  mkf_iso8859_1_parser_new()


#endif
