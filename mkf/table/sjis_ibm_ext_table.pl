#!/usr/local/bin/perl

use  ucs_mappings () ;

my $cs = "sjis_ibm_ext" ;

my @ibm_lines ;

my @all_lines = <stdin> ;
for $line (@all_lines)
{
	if( $line =~ /^(0x[A-Z0-9]+)[	 ]+(0x[A-Z0-9]+).*$/)
	{
		push( @ibm_lines , "$2 $1") ;
	}
}

ucs_mappings::parse( '^([^	 ]+)[	 ]+([^	 ]+).*$' , \@ibm_lines) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
