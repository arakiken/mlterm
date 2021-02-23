#!/usr/pkg/bin/perl

use ucs_mappings () ;

my $cs1 = "jisx0213_2000_1" ;
my $cs2 = "jisx0213_2000_2" ;

my @all_lines = <stdin> ;
my @jisx0213_1 ;
my @jisx0213_2 ;

foreach $line (@all_lines)
{
	if( $line =~ /^#.*/)
	{
		next ;
	}

	if( $line =~ /^3-.*$/)
	{
		push( @jisx0213_1 , $line) ;
	}
	elsif( $line =~ /^4-.*$/)
	{
		push( @jisx0213_2 , $line) ;
	}
}

ucs_mappings::parse( '^3-([^	]*)	U\+([^	]*).*$' , \@jisx0213_1) ;

ucs_mappings::output_table_to_ucs( ${cs1}) ;
ucs_mappings::output_table_ucs_to( ${cs1}) ;
ucs_mappings::reset ;

ucs_mappings::parse( '^4-([^	]*)	U\+([^	]*).*$' , \@jisx0213_2) ;

ucs_mappings::output_table_to_ucs( ${cs2}) ;
ucs_mappings::output_table_ucs_to( ${cs2}) ;
