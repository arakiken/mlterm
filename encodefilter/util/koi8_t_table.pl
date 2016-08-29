#!/usr/pkg/bin/perl

use ucs_mappings () ;

my $cs = "koi8_t" ;

my @all_lines = <stdin> ;
my @koi8 ;

foreach $line (@all_lines)
{
	if( $line =~ /^<U([0-9A-F]*)>[ ]*\/x([0-9a-f]*).*$/)
	{
		$code = oct "0x$2" ;
		if( $code >= 0x80)
		{
			push( @koi8 , "0x$2 0x$1") ;
		}
	}
}

ucs_mappings::parse( '^(0x[0-9a-f]*) (0x[0-9A-F]*)$' , \@koi8) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
