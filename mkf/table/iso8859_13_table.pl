#!/usr/local/bin/perl

use  ucs_mappings () ;

my $cs = "iso8859_13_r" ;

my @all_lines = <stdin> ;
my @iso8859 ;

foreach $line (@all_lines)
{
	if( $line =~ /^([^	 ]*)(.*)$/)
	{
		$code = oct $1 ;

		if( $code >= 0xa1)
		{
			push( @iso8859 , $line) ;
		}
	}
}

ucs_mappings::parse( '^([^	 ]*)[	 ]*([^	 ]*).*$' , \@iso8859) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
