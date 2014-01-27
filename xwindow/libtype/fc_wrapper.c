#include  <ft2build.h>
#include  <freetype/freetype.h>
#include  <fontconfig/fontconfig.h>

FT_UInt
FcFreeTypeCharIndex(
	FT_Face  face ,
	FcChar32  ucs4
	)
{
	return  FT_Get_Char_Index( face , ucs4) ;
}

FcBool
FcCharSetHasChar(
	const FcCharSet *  fcs ,
	FcChar32  ucs4
	)
{
	return  FcTrue ;
}
