/*
 *	$Id$
 */

#ifndef __EF_ISO2022_INTERN_H__
#define __EF_ISO2022_INTERN_H__

#define ESC 0x1b

#define SS2 0x8e
#define SS3 0x8f

/* these should be preceded by ESC */
#define SS2_7 0x4e
#define SS3_7 0x4f

#define LS0 0x0f
#define LS1 0x0e

/* these should be preceded by ESC */
#define LS2 0x6e
#define LS3 0x6f
#define LS1R 0x7e
#define LS2R 0x7d
#define LS3R 0x7c

/* this should be preceded by ESC */
#define MB_CS '$'

/* these should be preceded by ESC(+MB_CS) */
#define CS94_TO_G0 '('
#define CS94_TO_G1 ')'
#define CS94_TO_G2 '*'
#define CS94_TO_G3 '+'
#define CS96_TO_G1 '-'
#define CS96_TO_G2 '.'
#define CS96_TO_G3 '/'

/* this should be preceded by ESC */
#define CS_REV '&'

/* this should be preceded by ESC+CS_REV */
#define REV_NUM(c) ((u_char)(c) - '@' + 1)

/* these should be preceded by ESC */
#define NON_ISO2022_CS '%'

/* these should be preceded by ESC + NON_ISO2022_CS */
#define NON_ISO2022_CS_2 '/'

/* MSB (most significant bit) on/off */
#define MAP_TO_GR(c) (((u_char)c) | 0x80)
#define UNMAP_FROM_GR(c) (((u_char)c) & 0x7f)

#endif
