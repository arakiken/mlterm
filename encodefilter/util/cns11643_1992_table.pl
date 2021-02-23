#!/usr/pkg/bin/perl

# EUC-TW.TXT

use  ucs_mappings () ;

my $cs = "cns11643_1992" ;

my @all_lines = <stdin> ;
my @cns1 ;
my @cns2 ;
my @cns3 ;

foreach $line (@all_lines)
{
	if( $line =~ /^0x8EA1([^	]*)	(.*)/)
	{
		$cns = hex $1 ;
		push( @cns1 , sprintf("0x%x %s", $cns & 0x7f7f, $2)) ;
	}
	elsif( $line =~ /^0x8EA2([^	]*)	(.*)$/)
	{
		$cns = hex $1 ;
		push( @cns2 , sprintf("0x%x %s", $cns & 0x7f7f, $2)) ;
	}
	elsif( $line =~ /^0x8EA3([^	]*)	(.*)$/)
	{
		$cns = hex $1 ;
		push( @cns3 , sprintf("0x%x %s", $cns & 0x7f7f, $2)) ;
	}
}

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns1) ;

ucs_mappings::output_table_to_ucs( "${cs}_1") ;
ucs_mappings::output_table_ucs_to( "${cs}_1") ;
ucs_mappings::reset ;

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns2) ;

ucs_mappings::output_table_to_ucs( "${cs}_2") ;
ucs_mappings::output_table_ucs_to( "${cs}_2") ;
ucs_mappings::reset ;

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns3) ;

ucs_mappings::output_table_to_ucs( "${cs}_3") ;
ucs_mappings::output_table_ucs_to( "${cs}_3") ;
