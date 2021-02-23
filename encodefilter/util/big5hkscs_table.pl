#!/usr/pkg/bin/perl


use  ucs_mappings () ;


my @all_lines = <stdin> ;

my @hkscs_lines ;

foreach $line (@all_lines)
{
	if( $line =~ /^(0x[0-9A-F]*)[	]*(0x[0-9A-F]*).*$/)
	{
		my $ucs = $2 ;
		my $big5_str = $1 ;
		my $big5 = oct $big5_str ;

		if( $big5 <= 0x7f or ( 0xa140 <= $big5 and $big5 <= 0xa3bf) or
			( 0xa440 <= $big5 and $big5 <= 0xc67e) or
			( 0xc940 <= $big5 and $big5 <= 0xf9d5))
		{
			# excluded
		}
		else
		{
			push( @hkscs_lines , "$big5_str	$ucs") ;
		}
	}
}

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@hkscs_lines) ;

ucs_mappings::output_table_to_ucs( "hkscs") ;
ucs_mappings::output_table_ucs_to( "hkscs") ;
