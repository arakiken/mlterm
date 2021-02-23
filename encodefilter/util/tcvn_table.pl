#!/usr/pkg/bin/perl

# source file is TCVN5712-1.txt

use ucs_mappings () ;

my $cs = "tcvn5712_1993" ;

my @all_lines = <STDIN> ;
my @tcvn ;

foreach my $line (@all_lines)
{
	if( $line =~ /^(0x[0-9A-Z]*).*$/)
	{
		my $code = oct $1 ;

		if( 0x20 <= $code and $code <= 0x7e)
		{
			next ;
		}
		else
		{
			push( @tcvn , $line) ;
		}
	}
}

ucs_mappings::parse( '^(0x[0-9A-Z]+)[	 ]+(0x[0-9A-Z]+).*$' , \@tcvn) ;

ucs_mappings::output_table_to_ucs( ${cs}) ;
ucs_mappings::output_table_ucs_to( ${cs}) ;
