#!/usr/local/bin/perl

use ucs_mappings () ;

my $cs = "jisx0208_1983" ;

my @all_lines = <stdin> ;

ucs_mappings::parse( '[^	 ]*[	 ]*([^	 ]*)[	 ]*([^	 ]*).*$' , \@all_lines) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 2) ;
