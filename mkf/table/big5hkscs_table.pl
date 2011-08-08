#!/usr/pkg/bin/perl


use  ucs_mappings () ;


my @all_lines = <stdin> ;

my @big5_lines ;
my @hkscs_lines ;

foreach $line (@all_lines)
{
	# %IRREVERSIBLE% may be at the begginig of line.
	if( $line =~ /.*<U([0-9A-F]*)>[	 ]*\/x([0-9a-f]*)\/x([0-9a-f]*).*/)
	{
		my $ucs = $1 ;
		my $big5_str = "0x$2$3" ;
		my $big5 = oct $big5_str ;

		if( ( 0xa140 <= $big5 and $big5 <= 0xa3bf) or
			( 0xa440 <= $big5 and $big5 <= 0xc67e) or
			( 0xc940 <= $big5 and $big5 <= 0xf9d5))
		{
			# excluded
		}
		else
		{
			push( @hkscs_lines , "$big5_str $ucs") ;
		}
	}
}

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@hkscs_lines) ;

ucs_mappings::output_table_to_ucs( "hkscs" , 2) ;
ucs_mappings::output_table_ucs_to( "hkscs" , 2) ;
