#!/usr/pkg/bin/perl

use  ucs_mappings () ;

my $cs = "gbk" ;	# or CP936

my @gbk_lines ;

my @all_lines = <stdin> ;
for $line (@all_lines)
{
	if( $line =~ /^(0x[0-9A-Z][0-9A-Z][0-9A-Z][0-9A-Z])[	 ]+[^	 ]+.*$/)
	{
		push( @gbk_lines , $line) ;
	}
}

ucs_mappings::parse( '^([^	 ]+)[	 ]+([^	 ]+).*$' , \@gbk_lines) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 2) ;
