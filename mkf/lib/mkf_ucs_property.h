/*
 *	$Id$
 */

#ifndef  __MKF_UCS_PROPERTY_H__
#define  __MKF_UCS_PROPERTY_H__


#include  <kiklib/kik_types.h>	/* u_int32_t */

#include  "mkf_property.h"


#define  UCSPROP_GENERAL(prop)  ((prop) & 0x1f)
#define  UCSPROP_EAST_ASIAN_WIDTH(prop)  ((prop) & 0xe0)
#define  UCSPROP_DIRECTION(prop)  ((prop) & 0x1f00)


/*
 * 16 bit.
 *
 * !! Notice !!
 * don't forget to update mkf_ucs_property_intern_t(defined in mkf_ucs_property.c)
 * if new enums are added.
 *
 * XXX
 * direction property is not supported.
 */
typedef enum mkf_ucs_property
{
	/*
	 * general category
	 * 0000 0001 - 0001 1110
	 */
	MKF_UCSPROP_CC = 0x01u ,
	MKF_UCSPROP_CF = 0x02u ,
	MKF_UCSPROP_CN = 0x03u ,
	MKF_UCSPROP_CO = 0x04u ,
	MKF_UCSPROP_CS = 0x05u ,
	MKF_UCSPROP_LL = 0x06u ,
	MKF_UCSPROP_LM = 0x07u ,
	MKF_UCSPROP_LO = 0x08u ,
	MKF_UCSPROP_LT = 0x09u ,
	MKF_UCSPROP_LU = 0x0au ,
	MKF_UCSPROP_MC = 0x0bu ,
	MKF_UCSPROP_ME = 0x0cu ,
	MKF_UCSPROP_MN = 0x0du ,
	MKF_UCSPROP_ND = 0x0eu ,
	MKF_UCSPROP_NL = 0x0fu ,
	MKF_UCSPROP_NO = 0x10u ,
	MKF_UCSPROP_PC = 0x11u ,
	MKF_UCSPROP_PD = 0x12u ,
	MKF_UCSPROP_PE = 0x13u ,
	MKF_UCSPROP_PF = 0x14u ,
	MKF_UCSPROP_PI = 0x15u ,
	MKF_UCSPROP_PO = 0x16u ,
	MKF_UCSPROP_PS = 0x17u ,
	MKF_UCSPROP_SC = 0x18u ,
	MKF_UCSPROP_SK = 0x19u ,
	MKF_UCSPROP_SM = 0x1au ,
	MKF_UCSPROP_SO = 0x1bu ,
	MKF_UCSPROP_ZL = 0x1cu ,
	MKF_UCSPROP_ZP = 0x1du ,
	MKF_UCSPROP_ZS = 0x1eu ,

	/*
	 * east asian width property
	 * 0010 0000 - 1100 0000
	 */
	MKF_UCSPROP_EAW_N = 0x20u , /* Narrow */
	MKF_UCSPROP_EAW_H = 0x60u , /* Half */
	MKF_UCSPROP_EAW_W = 0x80u , /* Wide */
	MKF_UCSPROP_EAW_F = 0xa0u , /* Full */
	MKF_UCSPROP_EAW_A = 0x40u , /* Ambiguous */
	MKF_UCSPROP_EAW_NA = 0xc0u ,

#if  0
	/*
	 * direction property
	 * 0000 0001 0000 0000 - 0001 0111 0000 0000
	 */
	MKF_UCSPROP_DIR_L = 0x100u ,
	MKF_UCSPROP_DIR_LRE = 0x200u ,
	MKF_UCSPROP_DIR_LRO = 0x300u ,
	MKF_UCSPROP_DIR_R = 0x400u ,
	MKF_UCSPROP_DIR_AL = 0x500u ,
	MKF_UCSPROP_DIR_RLE = 0x600u ,
	MKF_UCSPROP_DIR_RLO = 0x700u ,
	MKF_UCSPROP_DIR_PDF = 0x800u ,
	MKF_UCSPROP_DIR_EN = 0x900u ,
	MKF_UCSPROP_DIR_ES = 0xa00u ,
	MKF_UCSPROP_DIR_ET = 0xb00u ,
	MKF_UCSPROP_DIR_AN = 0xc00u ,
	MKF_UCSPROP_DIR_CS = 0xd00u ,
	MKF_UCSPROP_DIR_NSM = 0xe00u ,
	MKF_UCSPROP_DIR_BN = 0xf00u ,
	MKF_UCSPROP_DIR_B = 0x1000u ,
	MKF_UCSPROP_DIR_S = 0x1100u ,
	MKF_UCSPROP_DIR_WS = 0x1200u ,
	MKF_UCSPROP_DIR_ON = 0x1300u ,

	/* internal */
	MKF_UCSPROP_DIR_N = 0x1400u ,
	MKF_UCSPROP_DIR_E = 0x1500u ,
	MKF_UCSPROP_DIR_SOP = 0x1600u ,
	MKF_UCSPROP_DIR_EOP = 0x1700u ,
#else
	/*
	 * XXX
	 * not supported yet.
	 */
	MKF_UCSPROP_DIR_L = 0x0u ,
	MKF_UCSPROP_DIR_LRE = 0x0u ,
	MKF_UCSPROP_DIR_LRO = 0x0u ,
	MKF_UCSPROP_DIR_R = 0x0u ,
	MKF_UCSPROP_DIR_AL = 0x0u ,
	MKF_UCSPROP_DIR_RLE = 0x0u ,
	MKF_UCSPROP_DIR_RLO = 0x0u ,
	MKF_UCSPROP_DIR_PDF = 0x0u ,
	MKF_UCSPROP_DIR_EN = 0x0u ,
	MKF_UCSPROP_DIR_ES = 0x0u ,
	MKF_UCSPROP_DIR_ET = 0x0u ,
	MKF_UCSPROP_DIR_AN = 0x0u ,
	MKF_UCSPROP_DIR_CS = 0x0u ,
	MKF_UCSPROP_DIR_NSM = 0x0u ,
	MKF_UCSPROP_DIR_BN = 0x0u ,
	MKF_UCSPROP_DIR_B = 0x0u ,
	MKF_UCSPROP_DIR_S = 0x0u ,
	MKF_UCSPROP_DIR_WS = 0x0u ,
	MKF_UCSPROP_DIR_ON = 0x0u ,
	
	MKF_UCSPROP_DIR_N = 0x0u ,
	MKF_UCSPROP_DIR_E = 0x0u ,
	MKF_UCSPROP_DIR_SOP = 0x0u ,
	MKF_UCSPROP_DIR_EOP = 0x0u ,
#endif
	
} mkf_ucs_property_t ;


mkf_ucs_property_t  mkf_get_raw_ucs_property( u_int32_t  ucs) ;

mkf_property_t  mkf_get_ucs_property( u_int32_t  ucs) ;


#endif
