/*
 *	$Id$
 */

#ifndef  __MKF_UCS_PROPERTY_H__
#define  __MKF_UCS_PROPERTY_H__


#include  <kiklib/kik_types.h>


#define  UCSPROP_GENERAL_CATEGORY(prop)  ((prop) & 0x1f)
#define  UCSPROP_EAST_ASIAN_WIDTH(prop)  ((prop) & 0xe0)


typedef enum mkf_ucs_property
{
	/* general category */
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

	/* east asian width property */
	MKF_UCSPROP_N = 0x20u ,	/* Narrow */
	MKF_UCSPROP_A = 0x40u ,
	MKF_UCSPROP_H = 0x60u ,	/* Half */
	MKF_UCSPROP_W = 0x80u ,	/* Wide */
	MKF_UCSPROP_F = 0xa0u ,	/* Full */
	MKF_UCSPROP_NA = 0xc0u ,
	
} mkf_ucs_property_t ;


mkf_ucs_property_t  mkf_get_ucs_property( u_char *  ch , size_t  size) ;


#endif
