#!/usr/pkg/bin/perl

use ucs_mappings () ;

my $cs = "jisx0212_1990" ;

my @all_lines = <stdin> ;

ucs_mappings::parse( '^([^	 ]*)[	 ]*([^	 ]*).*$' , \@all_lines) ;

ucs_mappings::output_table_to_ucs( ${cs}) ;
ucs_mappings::output_table_ucs_to( ${cs}) ;
