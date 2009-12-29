#!/usr/local/bin/perl

use ucs_mappings () ;

my $cs = "cp1253" ;

my @all_lines = <stdin> ;
my @cp1253 ;

foreach $line (@all_lines)
{
	if( $line =~ /^(0x[0-9A-F]*)[	]*(0x[0-9A-F]*).*$/)
	{
		$code = oct "$1" ;
		if( $code >= 0x80)
		{
			push( @cp1253 , "$1 $2") ;
		}
	}
}

ucs_mappings::parse( '^(0x[0-9A-F]*) (0x[0-9A-F]*)$' , \@cp1253) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
