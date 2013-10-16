#!/usr/pkg/bin/perl

my %props ;

sub  parse_line($)
{
	$_ = shift ;
	
	if(/^([^;]*);([^;]*);([^;]*);[^;]*;([^;]*);[^;]*;[^;]*;[^;]*;[^;]*;[^;]*;[^;]*;[^;]*;[^;]*;[^;]*;[^;]*/)
	{
		# Unicode Data
		# code , prop , comment
		return ( $1 , [$3 , "DIR_$4"] , $2) ;
	}
	elsif( /^([0-9A-F]*);([^ ]*) . (.*)$/)
	{
		# East Asian Width
		# code , prop , comment
		return  ( $1 , ["EAW_$2"] , $3) ;
	}
	elsif( /^([0-9A-F]*)..([0-9A-F]*);([^ ]*) . (.*)$/)
	{
		# East Asian Width(code..code , prop , comment..comment)
		# code<first> , prop , code<last> , comment
		return  ( $1 , ["EAW_$3"] , $2 , $4) ;
	}
	else
	{
		return my @dummy ;
	}
}

sub  parse($)
{
	my $all_props = shift ;

	my $first = 0 ;
	my $last = 0 ;

	foreach $line (@{$all_props})
	{
		my $prop ;

		if( $line =~ /^#.*/)
		{
			# comment

			next ;
		}
		
		if( (my @ret = parse_line( $line)))
		{
			my $code = oct "0x${ret[0]}" ;

			$prop = 0 ;

			foreach $_prop (@{${ret[1]}})
			{
				$_prop = uc $_prop ;

				if( $_prop =~ /^M[CEN]$/)
				{
					$_prop = "MKF_COMBINING" ;
				}
				elsif( $_prop =~ /^EAW_[WF]$/)
				{
					$_prop = "MKF_FULLWIDTH" ;
				}
				elsif( $_prop eq "EAW_A")
				{
					$_prop = "MKF_AWIDTH" ;
				}
				else
				{
					next ;
				}
				
				if( $prop)
				{
					$prop = "${prop} | ${_prop}" ;
				}
				else
				{
					$prop = "${_prop}" ;
				}
			}

			if( ! $prop)
			{
				next ;
			}

			if( @ret == 4)
			{
				$first = $code ;
				$last = oct "0x${ret[2]}" ;
			}
			else # ( @ret == 3)
			{
				$comment = ${ret[2]} ;

				if( $first ne 0)
				{
					if( $comment =~ /<.* Last>/)
					{
						$last = $code ;
					}
					else
					{
						die  "illegal format.\n" ;
					}
				}
				else
				{
					$first = $code ;

					if( $comment =~ /<.* First>/)
					{
						$first = $code ;

						# $last is not set and process continued ...

						next ;
					}
					else
					{
						$last = $code ;
					}
				}
			}
		}
		else
		{
			next ;
		}

		for( my $code = $first ; $code <= $last ; $code ++)
		{
			if( exists $props{$code})
			{
				$props{$code} = "$props{$code} | ${prop}" ;
			}
			else
			{
				$props{$code} = "${prop}" ;
			}
		}

		$first = 0 ;
		$last = 0 ;
	}
}

sub  output()
{
	open TO , ">mkf_ucs_property.table" ;

	print  TO << "EOF" ;
/*
 *	mkf_ucs_property.table
 */

#ifndef  __MKF_UCS_PROPERTY_TABLE__
#define  __MKF_UCS_PROPERTY_TABLE__


#include  <kiklib/kik_types.h>


typedef struct mkf_ucs_property
{
	u_int32_t  first ;
	u_int32_t  last ;
	u_int8_t  prop ;	/* mkf_property_t */
	
} mkf_ucs_property_t ;


static mkf_ucs_property_t  ucs_property_table[] =
{
	{ 0x00000000 , 0x00000000 , 0 } ,
EOF

	my $start = 0 ;
	my $prev_code = 0 ;

	my @array = sort {$a <=> $b} keys %props ;
	
	foreach $code (@array)
	{
		if( $prev_code == 0)
		{
			$start = $code ;
			$prev_code = $code ;
			next ;
		}
		elsif( $prev_code + 1 == $code && $props{$prev_code} eq $props{$code})
		{
			$prev_code ++ ;
			next ;
		}

		printf TO "	{ 0x%.8x , 0x%.8x , $props{$prev_code} } ,\n" ,
			$start , $prev_code ;
		
		$start = $code ;
		$prev_code = $code ;
	}

	printf TO "	{ 0x%.8x , 0x%.8x , $props{$prev_code} } ,\n" ,
		$start , $prev_code ;

	print TO "	{ 0xffffffff , 0xffffffff , 0 } ,\n" ;
	print TO "} ; \n\n" ;

	print TO "\n\n#endif\n" ;

	close TO ;
}

my @all_lines = <STDIN> ;
parse( \@all_lines) ;

output ;

print "Add 0x1160-0x11ff (Jamo medial vowels and final consonants) = MKF_COMBINING | MKF_FULLWIDTH manually.\n"
