#!/usr/pkg/bin/perl

# source file is unicode.html

use ucs_mappings () ;

my $cs = "viscii" ;

my @all_lines = <STDIN> ;

# NON UCS is decimal
ucs_mappings::parse( '^([0-9]+)[	 ]+[^	 ]+[	 ]+([0-9A-Z]+).*$' , \@all_lines , 0x1) ;

ucs_mappings::output_table_to_ucs( ${cs} , 2) ;
ucs_mappings::output_table_ucs_to( ${cs} , 1) ;
