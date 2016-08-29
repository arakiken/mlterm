#!/usr/pkg/bin/perl

#
#	this eats hangul-codes.txt and outputs tables.
#

use ucs_mappings () ;

my @all_lines = <stdin> ;

%to_johab ;
%from_johab ;
my $to_johab_start ;
my $from_johab_start ;
for $line (@all_lines)
{
	if( $line =~ /^[A-Z0-9]+[	 ]+[A-Z0-9]+[	 ]+([A-Z0-9]+)[	 ]+([A-Z0-9]+).*$/)
	{
		$_uhc = $1 ;
		$_johab = $2 ;

		if( ! ($_uhc =~ /0x.*/))
		{
			$uhc = oct "0x${_uhc}" ;
		}
		else
		{
			$uhc = oct $_uhc ;
		}

		if( ! ($_johab =~ /0x.*/))
		{
			$johab = oct "0x${_johab}" ;
		}
		else
		{
			$johab = oct $_johab ;
		}

		$to_johab{$uhc} = $johab ;
		$from_johab{$johab} = $uhc ;
	}
}

ucs_mappings::output_separated( "johab" , "uhc" , 2 , \%from_johab) ;
ucs_mappings::output_separated( "uhc" , "johab" , 2 , \%to_johab) ;
