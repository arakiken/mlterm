#!/usr/pkg/bin/perl

# KSC5601.TXT

use ucs_mappings () ;

my $cs = "uhc" ;

my @all_lines = <stdin> ;

ucs_mappings::parse( '^([^	 ]+)[	 ]+([^	 ]+).*$' , \@all_lines) ;

ucs_mappings::output_table_to_ucs( ${cs}) ;
ucs_mappings::output_table_ucs_to( ${cs}) ;
