#!/usr/pkg/bin/perl

use ucs_mappings () ;

my $cs = "cp1258" ;

my @all_lines = <stdin> ;
my @cp1258 ;

foreach $line (@all_lines)
{
	if( $line =~ /^(0x[0-9A-F]*)[	]*(0x[0-9A-F]*).*$/)
	{
		$code = oct "$1" ;
		if( $code >= 0x80)
		{
			push( @cp1258 , "$1 $2") ;
		}
	}
}

ucs_mappings::parse( '^(0x[0-9A-F]*) (0x[0-9A-F]*)$' , \@cp1258) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
