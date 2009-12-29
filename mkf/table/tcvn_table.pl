#!/usr/local/bin/perl

# source file is vscii.txt

use ucs_mappings () ;

my $cs = "tcvn5712_1993" ;

my @all_lines = <STDIN> ;
my @tcvn ;

foreach my $line (@all_lines)
{
	if( $line =~ /^(0x[0-9a-z]*).*$/)
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

ucs_mappings::parse( '^(0x[0-9a-z]+)[	 ]+(0x[0-9a-z]+).*$' , \@tcvn) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
