#!/usr/local/bin/perl

use  ucs_mappings () ;

my $cs = "cns11643_1992" ;

my @all_lines = <stdin> ;
my @cns1 ;
my @cns2 ;
my @cns3 ;

foreach $line (@all_lines)
{
	if( $line =~ /^0x1(.*)$/)
	{
		push( @cns1 , "0x$1") ;
	}
	elsif( $line =~ /^0x2(.*)$/)
	{
		push( @cns2 , "0x$1") ;
	}
	elsif( $line =~ /^0xE(.*)$/)
	{
		push( @cns3 , "0x$1") ;
	}
}

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns1) ;

ucs_mappings::output_table_to_ucs( "${cs}_1" , 2) ;
ucs_mappings::output_table_ucs_to( "${cs}_1" , 2) ;
ucs_mappings::reset ;

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns2) ;

ucs_mappings::output_table_to_ucs( "${cs}_2" , 2) ;
ucs_mappings::output_table_ucs_to( "${cs}_2" , 2) ;
ucs_mappings::reset ;

ucs_mappings::parse( '([^	 ]*)[	 ]*([^	 ]*).*$' , \@cns3) ;

ucs_mappings::output_table_to_ucs( "${cs}_3" , 2) ;
ucs_mappings::output_table_ucs_to( "${cs}_3" , 2) ;
