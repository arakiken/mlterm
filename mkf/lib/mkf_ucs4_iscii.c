/*
 *	$Id$
 */

#include  "mkf_ucs4_iscii.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_iscii.c"

#else

mkf_map_func( 8bits , mkf_map_ucs4_to_iscii , 32)

mkf_map_func( 8bits , mkf_map_iscii_assamese_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_bengali_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_gujarati_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_hindi_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_kannada_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_malayalam_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_oriya_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_punjabi_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_roman_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_tamil_to_ucs4 , 16)
mkf_map_func( 8bits , mkf_map_iscii_telugu_to_ucs4 , 16)


#endif
