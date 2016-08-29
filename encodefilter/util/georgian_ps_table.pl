#!/usr/pkg/bin/perl

use ucs_mappings () ;

my $cs = "georgian_ps" ;

my @all_lines = <stdin> ;
my @georgian ;

foreach $line (@all_lines)
{
	if( $line =~ /^<U([0-9A-F]*)>[ ]*\/x([0-9a-f]*).*$/)
	{
		$code = oct "0x$2" ;
		if( $code >= 0x80)
		{
			push( @georgian , "0x$2 0x$1") ;
		}
	}
}

ucs_mappings::parse( '^(0x[0-9a-f]*) (0x[0-9A-F]*)$' , \@georgian) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
