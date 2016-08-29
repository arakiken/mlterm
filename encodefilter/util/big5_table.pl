#!/usr/pkg/bin/perl


use  ucs_mappings () ;


my @all_lines = <stdin> ;

my @big5_lines ;

foreach $line (@all_lines)
{
	# %IRREVERSIBLE% may be at the begginig of line.
	if( $line =~ /.*<U([0-9A-F]*)>[	 ]*\/x([0-9a-f]*)\/x([0-9a-f]*).*/)
	{
		push( @big5_lines , "0x$2$3 $1") ;
	}
}

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@big5_lines) ;

ucs_mappings::output_table_to_ucs( "big5" , 2) ;
ucs_mappings::output_table_ucs_to( "big5" , 2) ;
