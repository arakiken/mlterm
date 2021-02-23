#
#	ucs parser module.
#

package  ucs_mappings ;


$start = 0 ;
$end = 0 ;

%to_ucs ;
%ucs_to ;

$max_ucs = 0 ;
$max_nonucs = 0 ;

sub  reset()
{
	%to_ucs = 0 ;
	%ucs_to = 0 ;
}

sub  parse($$;$)
{
	my $regexp = shift ;
	my $all_lines = shift ;

	#
	# property
	# 0x1 = NON UCS decimal
	# 0x2 = UCS decimal
	# 0x4 = UCS/NON_UCS position swap($1 is ucs , $2 is non ucs)
	#
	my $prop = 0 ;
	if( @_)
	{
		$prop = shift ;
	}
	
	%to_ucs = () ;
	%ucs_to = () ;
	
	foreach $line (@$all_lines)
	{
		if( $line =~ /^#.*$/)
		{
			next ;
		}
	
		if( $line =~ /$regexp/)
		{
			my $_code ;
			my $_ucs ;
			my $code ;
			my $ucs ;

			if( ($prop & 0x4) == 0)
			{
				$_code = $1 ;
				$_ucs = $2 ;
			}
			else
			{
				$_code = $2 ;
				$_ucs = $1 ;
			}

			if( ($prop & 0x1) == 0)
			{
				if( ! ($_code =~ /0x.*/))
				{
					$code = oct "0x${_code}" ;
				}
				else
				{
					$code = oct $_code ;
				}
			}
			else
			{
				$code = $_code ;
			}

			if( ($prop & 0x2) == 0)
			{
				if( ! ($_ucs =~ /0x.*/))
				{
					$ucs = oct "0x${_ucs}" ;
				}
				else
				{
					$ucs = oct $_ucs ;
				}
			}
			else
			{
				$ucs = $_ucs ;
			}

			$to_ucs{$code} = ${ucs} ;
			if( $ucs > $max_ucs) {
				$max_ucs = $ucs ;
			}
			$ucs_to{$ucs} = $code ;
			if( $code > $max_nonucs) {
				$max_nonucs = $code ;
			}
		}
	}
}

sub output($$$$)
{
	my $fromcs = shift ;
	my $tocs = shift ;
	my $tocs_bytelen = shift ;
	my $hash = shift ;

	my $tocs_bits = $tocs_bytelen * 8 ;

	my @keylist = keys %{$hash} ;
	if( @keylist eq 0)
	{
		print "table [${fromcs} to ${tocs}] has no keys.\n" ;

		return ;
	}

	my $fromcs_uc = uc $fromcs ;
	my $tocs_uc = uc $tocs ;
	
	open TO , ">ef_${fromcs}_to_${tocs}.table" ;

	print TO << "EOF" ;
/*
 *	ef_${fromcs}_to_${tocs}.table
 */

#ifndef  __EF_${fromcs_uc}_TO_${tocs_uc}_TABLE__
#define  __EF_${fromcs_uc}_TO_${tocs_uc}_TABLE__


#include  <pobl/bl_types.h>		/* u_xxx */


#ifdef  REMOVE_MAPPING_TABLE

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch)  0x0

#else

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch) \\
	( (ch) < ${fromcs}_to_${tocs}_beg || ${fromcs}_to_${tocs}_end < (ch) ? \\
		0 : ${fromcs}_to_${tocs}_table[ (ch) - ${fromcs}_to_${tocs}_beg])


EOF

	print TO  "static u_int${tocs_bits}_t  ${fromcs}_to_${tocs}_table[] = \n{" ;

	my $code = -1 ;
	my $start = 0 ;
	foreach $key ( sort {$a <=> $b} keys %$hash)
	{
		if( $code == -1)
		{
			$start = $code = $key ;
		}
	
		while( $code < $key)
		{
			if( $code % 16 == $start % 16)
			{
				printf TO  "\n	/* 0x%x */\n" , $code ;
			}

			print TO "	0x00 ,\n" ;

			$code ++ ;
		}

		if( $code % 16 == $start % 16)
		{
			printf TO  "\n	/* 0x%x */\n" , $code ;
		}

		if( $tocs_bytelen eq 4)
		{
#			printf TO  "	0x%.8x ,\n" , $$hash{$key} & 0xffffffff ;
			printf TO  "	0x%.4x ,\n" , $$hash{$key} & 0xffffffff ;
		}
		elsif( $tocs_bytelen eq 2)
		{
			printf TO  "	0x%.4x ,\n" , $$hash{$key} & 0xffff ;
		}
		elsif( $tocs_bytelen eq 1)
		{
			printf TO  "	0x%.2x ,\n" , $$hash{$key} & 0xff ;
		}

		$code ++ ;
	}

	print TO  "} ;\n\n" ;

	printf TO  "static u_int  ${fromcs}_to_${tocs}_beg = 0x%x ;\n\n" , $start ;

	printf TO  "static u_int  ${fromcs}_to_${tocs}_end = 0x%x ;\n\n" , $code - 1 ;

	printf TO  "#endif\n\n\n#endif\n" ;
}

sub output_separated($$$$$)
{
	my $fromcs = shift ;	# SB charset is not supported.
	my $tocs = shift ;
	my $fromcs_bytelen = shift ;
	my $tocs_bytelen = shift ;
	my $hash = shift ;

	my $fromcs_bits = $fromcs_bytelen * 8 ;
	if( $fromcs_bytelen eq 3)
	{
		$fromcs_bits = 32 ;
	}

	my $tocs_bits = $tocs_bytelen * 8 ;
	if( $tocs_bytelen eq 3)
	{
		$tocs_bits = 8 ;
	}

	my @keylist = keys %{$hash} ;
	if( @keylist eq 0)
	{
		print "table [${fromcs} to ${tocs}] has no keys.\n" ;

		return ;
	}

	my $fromcs_uc = uc $fromcs ;
	my $tocs_uc = uc $tocs ;
	
	open TO , ">ef_${fromcs}_to_${tocs}.table" ;

	print TO << "EOF" ;
/*
 *	ef_${fromcs}_to_${tocs}.table
 */

#ifndef  __EF_${fromcs_uc}_TO_${tocs_uc}_TABLE__
#define  __EF_${fromcs_uc}_TO_${tocs_uc}_TABLE__


#include  <stdio.h>			/* NULL */
#include  <pobl/bl_types.h>		/* u_xxx */


#ifdef  REMOVE_MAPPING_TABLE

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch)  0x0

#else

typedef struct ${fromcs}_to_${tocs}_table
{
	u_int${tocs_bits}_t *  table ;
	u_int${fromcs_bits}_t  beg;
	u_int${fromcs_bits}_t  end;

} ${fromcs}_to_${tocs}_table_t ;

EOF

	my @indexes ;
	my @start_list ;
	my @end_list ;
	my $code = -1 ;
	my $start = 0 ;
	my $_start = 0 ;
	my $hi_code = 0 ;
	foreach $key ( sort {$a <=> $b} keys %{$hash})
	{
		my $next_hi_code = ($key >> 7) & 0x1ffffff ;

		if( $code == -1)
		{
			$_start = $start = $code = $key ;
			$hi_code = $next_hi_code ;
			printf TO  "static u_int${tocs_bits}_t ${fromcs}_to_${tocs}_table_%x[] =\n{" ,
				${hi_code} ;
		}

		if( $hi_code < $next_hi_code)
		{
			print TO "} ;\n\n" ;
			push( @start_list, $_start) ;
			push( @end_list, $code - 1) ;
			push( @indexes , $hi_code) ;
			
			$hi_code = $next_hi_code ;

			printf TO  "static u_int${tocs_bits}_t ${fromcs}_to_${tocs}_table_%x[] =\n{" ,
				${hi_code} ;
			$_start = $code = $key ;
		}

		while( $code < $key)
		{
			if( $code % 16 == $_start % 16)
			{
				printf TO  "\n	/* 0x%x */\n" , $code ;
			}

			if( $tocs_bytelen eq 3)
			{
				print TO "	0x00 , 0x00 , 0x00 ,\n" ;
			}
			else
			{
				print TO "	0x00 ,\n" ;
			}

			$code ++ ;
		}

		if( $code % 16 == $_start % 16)
		{
			printf TO  "\n	/* 0x%x */\n" , $code ;
		}

		if( $tocs_bytelen eq 4)
		{
#			printf TO  "	0x%.8x ,\n" , ${$hash}{$key} & 0xffffffff ;
			printf TO  "	0x%.4x ,\n" , ${$hash}{$key} & 0xffffffff ;
		}
		elsif( $tocs_bytelen eq 3)
		{
			$c = ${$hash}{$key} & 0xffffffff ;
			printf TO  "	0x%.2x , 0x%.2x , 0x%.2x ,\n" ,
						 ($c >> 16) & 0xff , ($c >> 8) & 0xff , $c & 0xff ;
		}
		elsif( $tocs_bytelen eq 2)
		{
			printf TO  "	0x%.4x ,\n" , ${$hash}{$key} & 0xffff ;
		}
		elsif( $tocs_bytelen eq 1)
		{
			printf TO  "	0x%.2x ,\n" , ${$hash}{$key} & 0xff ;
		}

		$code ++ ;
	}
	print TO  "} ;\n\n" ;

	push( @start_list, $_start) ;
	push( @end_list, $code - 1) ;
	push( @indexes , $hi_code) ;

	printf TO  "static u_int32_t  ${fromcs}_to_${tocs}_beg = 0x%x ;\n\n" , $start ;
	printf TO  "static u_int32_t  ${fromcs}_to_${tocs}_end = 0x%x ;\n\n" , $code - 1 ;

	my $prev_idx = -1 ;
	print TO "static ${fromcs}_to_${tocs}_table_t ${fromcs}_to_${tocs}_tables[] =\n{\n" ;
	foreach my $idx (@indexes)
	{
		if( $prev_idx == -1)
		{
			$prev_idx = $idx ;
		}

		while( $idx > $prev_idx + 1)
		{
			print TO "	{ NULL , 0x0 , 0x0 , } ,\n" ;
			$prev_idx ++ ;
		}

		printf TO "	{ ${fromcs}_to_${tocs}_table_%x , 0x%x, 0x%x } ,\n" ,
			$idx , shift @start_list, shift @end_list ;
		$prev_idx = $idx ;
	}
	print TO "} ;\n\n" ;

	if( ${fromcs} eq "ucs4")
	{
		print TO << "EOF" ;
/* 'i' is UCS4(32bit) */
#define  HI(i)  ( ((i)>>7) & 0x1ffffff )
EOF
	}
	else  # if( keys %${hash} > 256)
	{
		# MB charset (SB charset is not supposed.)
		print TO << "EOF" ;
#define  HI(i)  ( ((i)>>7) & 0x1ff )
EOF
	}

	if( $tocs_bytelen eq 3) {
		print TO << "EOF" ;

static u_int32_t
CONV_${fromcs_uc}_TO_${tocs_uc}(
	u_int32_t  ch
	)
{
	u_int32_t  hi_ch ;
	u_int32_t  hi_beg ;
	u_int32_t  idx ;
	u_int8_t *  table ;

	if( ch < ${fromcs}_to_${tocs}_beg || ${fromcs}_to_${tocs}_end < ch)
	{
		return  0 ;
	}
	
	hi_ch = HI(ch) ;
	hi_beg = HI(${fromcs}_to_${tocs}_beg) ;

	idx = hi_ch - hi_beg;
	if( ! ( table = ${fromcs}_to_${tocs}_tables[ idx].table))
	{
		return  0 ;
	}

	if( ch < ${fromcs}_to_${tocs}_tables[ idx].beg ||
	    ${fromcs}_to_${tocs}_tables[ idx].end < ch)
	{
		return  0 ;
	}

	idx = (ch - ${fromcs}_to_${tocs}_tables[ idx].beg) * 3 ;

	return  (table[idx] << 16) | (table[idx + 1] << 8) | table[idx + 2]  ;
}

#undef  HI

EOF
	}
	else
	{
		print TO << "EOF" ;

static u_int${tocs_bits}_t
CONV_${fromcs_uc}_TO_${tocs_uc}(
	u_int32_t  ch
	)
{
	u_int32_t  hi_ch ;
	u_int32_t  hi_beg ;
	u_int32_t  idx ;
	u_int${tocs_bits}_t *  table ;

	if( ch < ${fromcs}_to_${tocs}_beg || ${fromcs}_to_${tocs}_end < ch)
	{
		return  0 ;
	}
	
	hi_ch = HI(ch) ;
	hi_beg = HI(${fromcs}_to_${tocs}_beg) ;

	idx = hi_ch - hi_beg;
	if( ! ( table = ${fromcs}_to_${tocs}_tables[ idx].table))
	{
		return  0 ;
	}

	if( ch < ${fromcs}_to_${tocs}_tables[ idx].beg ||
	    ${fromcs}_to_${tocs}_tables[ idx].end < ch)
	{
		return  0 ;
	}

	return  table[ ch - ${fromcs}_to_${tocs}_tables[ idx].beg] ;
}

#undef  HI

EOF
	}

	printf TO  "#endif	/* REMOVE_MAPPING_TABLE */\n\n\n#endif\n" ;
}

sub output_table_to_ucs($)
{
	$fromcs_bytelen = int(log($max_nonucs) / log(256)) + 1 ;
	$tocs_bytelen = int(log($max_ucs) / log(256)) + 1 ;

	if( keys %to_ucs > 256)
	{
		# MB charset
		output_separated( shift , "ucs4" , $fromcs_bytelen , $tocs_bytelen , \%to_ucs) ;
	}
	else
	{
		# SB charset
		output( shift , "ucs4" , $tocs_bytelen , \%to_ucs) ;
	}
}

sub output_table_ucs_to($)
{
	$fromcs_bytelen = int(log($max_ucs) / log(256)) + 1 ;
	$tocs_bytelen = int(log($max_nonucs) / log(256)) + 1 ;

	output_separated( "ucs4" , shift , $fromcs_bytelen , $tocs_bytelen , \%ucs_to) ;
}

# module return

1 ;
