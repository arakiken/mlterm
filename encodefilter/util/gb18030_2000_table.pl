#!/usr/pkg/bin/perl

sub print_range
{
	my $u_first = shift ;
	my $u_last = shift ;
	my $b_first = shift ;
	my $b_last = shift ;
	
	printf TO  "	{ 0x%.4x , 0x%.4x , { $b_first } , { $b_last } } ,\n" ,
		$u_first , $u_last ;
}

my @all_lines = <STDIN> ;

open TO , ">ef_gb18030_2000_range.table" ;

print TO << "EOF" ;
/*
 *	ef_gb18030_2000_range.table
 */

#ifndef  __EF_GB18030_2000_RANGE_TABLE__
#define  __EF_GB18030_2000_RANGE_TABLE__


/* ---> static variables <--- */

static gb18030_range_t gb18030_ranges[] =
{
EOF

my $u_first = -1 ;
my $u_last = 0 ;
my $b_first = 0 ;
my $b_last = 0 ;

foreach $line (@all_lines)
{
	if( $line =~ /<a u="([0-9A-Z]*)" b="([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z])"\/>/)
	{
		if( $u_first eq 0 or $u_first eq -1)
		{
			$u_first = oct "0x$1" ;
			$b_first = "0x$2 , 0x$3 , 0x$4 , 0x$5" ;
		}
		else
		{
			$u_last = oct "0x$1" ;
			$b_last = "0x$2 , 0x$3 , 0x$4 , 0x$5" ;
		}
	}
	elsif( $line =~ /<range uFirst="([0-9A-Z]*)" uLast="([0-9A-Z]*)"  bFirst="([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z])" bLast="([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z]) ([0-9A-Z][0-9A-Z])".*\/>/)
	{
		my $u_first = oct "0x$1" ;
		my $u_last = oct "0x$2" ;
		my $b_first = "0x$3 , 0x$4 , 0x$5 , 0x$6" ;
		my $b_last = "0x$7 , 0x$8 , 0x$9 , 0x$10" ;

		print_range( $u_first , $u_last , $b_first , $b_last) ;
	}
	else
	{
		if( $u_first eq -1)
		{
			next ;
		}
		elsif( $u_first ne 0)
		{
			if( $u_last eq 0)
			{
				$u_last = $u_first ;
				$b_last = $b_first ;
			}

			print_range( $u_first , $u_last , $b_first , $b_last) ;

			# reset
			$u_first = $u_last = $b_first = $b_last = 0 ;
		}
	}
}

print TO "} ;\n\n\n" ;
print TO "#endif\n" ;


close  TO ;
