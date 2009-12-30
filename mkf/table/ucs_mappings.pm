#
#	ucs parser module.
#

package  ucs_mappings ;


$start = 0 ;
$end = 0 ;

%to_ucs ;
%ucs_to ;

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
			$ucs_to{$ucs} = $code ;
		}
	}
}

sub output($$$$)
{
	my $fromcs = shift ;
	my $tocs = shift ;
	my $tocs_bytelen = shift ;
	my $hash = shift ;

	my $bits = $tocs_bytelen * 8 ;

	my @keylist = keys %{$hash} ;
	if( @keylist eq 0)
	{
		print "table [${fromcs} to ${tocs}] has no keys.\n" ;

		return ;
	}

	my $fromcs_uc = uc $fromcs ;
	my $tocs_uc = uc $tocs ;
	
	open TO , ">mkf_${fromcs}_to_${tocs}.table" ;

	print TO << "EOF" ;
/*
 *	mkf_${fromcs}_to_${tocs}.table
 */

#ifndef  __MKF_${fromcs_uc}_TO_${tocs_uc}_TABLE__
#define  __MKF_${fromcs_uc}_TO_${tocs_uc}_TABLE__


#include  <kiklib/kik_types.h>		/* u_xxx */


#ifdef  REMOVE_MAPPING_TABLE

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch)  0x0

#else

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch) \\
	( (ch) < ${fromcs}_to_${tocs}_beg || ${fromcs}_to_${tocs}_end < (ch) ? \\
		0 : ${fromcs}_to_${tocs}_table[ (ch) - ${fromcs}_to_${tocs}_beg])


EOF

	print TO  "static u_int${bits}_t  ${fromcs}_to_${tocs}_table[] = \n{" ;

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
			printf TO  "	0x%.8x ,\n" , $$hash{$key} & 0xffffffff ;
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

sub output_separated($$$$)
{
	my $fromcs = shift ;	# SB charset is not supported.
	my $tocs = shift ;
	my $tocs_bytelen = shift ;
	my $hash = shift ;

	my $bits = $tocs_bytelen * 8 ;

	my @keylist = keys %{$hash} ;
	if( @keylist eq 0)
	{
		print "table [${fromcs} to ${tocs}] has no keys.\n" ;

		return ;
	}

	my $fromcs_uc = uc $fromcs ;
	my $tocs_uc = uc $tocs ;
	
	open TO , ">mkf_${fromcs}_to_${tocs}.table" ;

	print TO << "EOF" ;
/*
 *	mkf_${fromcs}_to_${tocs}.table
 */

#ifndef  __MKF_${fromcs_uc}_TO_${tocs_uc}_TABLE__
#define  __MKF_${fromcs_uc}_TO_${tocs_uc}_TABLE__


#include  <stdio.h>			/* NULL */
#include  <kiklib/kik_types.h>		/* u_xxx */


#ifdef  REMOVE_MAPPING_TABLE

#define  CONV_${fromcs_uc}_TO_${tocs_uc}(ch)  0x0

#else

typedef struct ${fromcs}_to_${tocs}_range
{
	u_int32_t  beg ;
	u_int32_t  end ;

} ${fromcs}_to_${tocs}_range_t ;

typedef struct ${fromcs}_to_${tocs}_table
{
	u_int${bits}_t *  table ;
	${fromcs}_to_${tocs}_range_t *  range ;

} ${fromcs}_to_${tocs}_table_t ;

EOF

	my @indexes ;
	my $code = -1 ;
	my $start = 0 ;
	my $_start = 0 ;
	my $hi_code = 0 ;
	foreach $key ( sort {$a <=> $b} keys %{$hash})
	{
		my $next_hi_code = ($key >> 8) & 0xff ;
		
		if( $code == -1)
		{
			$_start = $start = $code = $key ;
			$hi_code = $next_hi_code ;
			printf TO  "static u_int${bits}_t ${fromcs}_to_${tocs}_table_%x[] =\n{" ,
				${hi_code} ;
		}
		
		if( $hi_code < $next_hi_code)
		{
			print TO "} ;\n\n" ;
			printf TO "static ${fromcs}_to_${tocs}_range_t ${fromcs}_to_${tocs}_range_%x =\n{\n" ,
				${hi_code} ;
			printf TO "	0x%x , 0x%x\n} ;\n\n" , $_start , $code - 1 ;
			push( @indexes , $hi_code) ;
			
			$hi_code = $next_hi_code ;

			printf TO  "static u_int${bits}_t ${fromcs}_to_${tocs}_table_%x[] =\n{" ,
				${hi_code} ;
			$_start = $code = $key ;
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

		if( $code % 16 == $_start % 16)
		{
			printf TO  "\n	/* 0x%x */\n" , $code ;
		}

		if( $tocs_bytelen eq 4)
		{
			printf TO  "	0x%.8x ,\n" , ${$hash}{$key} & 0xffffffff ;
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

	printf TO "static ${fromcs}_to_${tocs}_range_t ${fromcs}_to_${tocs}_range_%x =\n{\n" ,
		${hi_code} ;
	printf TO "	0x%x , 0x%x\n} ;\n\n" , $_start , $code - 1 ;
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
			print TO "	{ NULL , NULL , } ,\n" ;
			$prev_idx ++ ;
		}
		
		printf TO "	{ ${fromcs}_to_${tocs}_table_%x , &${fromcs}_to_${tocs}_range_%x } ,\n" ,
			$idx , $idx ;
		$prev_idx = $idx ;
	}
	print TO "} ;\n\n" ;

	if( ${fromcs} eq "ucs4")
	{
		print TO << "EOF" ;
/* 'i' is UCS4(32bit) */
#define  HI(i)  ( ((i)>>8) & 0xffffff )
#define  LO(i)  ( (i) & 0xff )
EOF
	}
	else  # if( keys %${hash} > 256)
	{
		# MB charset (SB charset is not supposed.)
		print TO << "EOF" ;
#define  HI(i)  ( ((i)>>8) & 0xff )
#define  LO(i)  ( (i) & 0xff )
EOF
	}
	
	print TO << "EOF" ;

inline u_int${bits}_t
CONV_${fromcs_uc}_TO_${tocs_uc}(
	u_int32_t  ch
	)
{
	u_int32_t  hi_ch ;
	u_int32_t  hi_beg ;
	u_int${bits}_t *  table ;
	${fromcs}_to_${tocs}_range_t *  range ;

	if( ch < ${fromcs}_to_${tocs}_beg || ${fromcs}_to_${tocs}_end < ch)
	{
		return  0 ;
	}
	
	hi_ch = HI(ch) ;
	hi_beg = HI(${fromcs}_to_${tocs}_beg) ;
	
	if( ! ( table = ${fromcs}_to_${tocs}_tables[ hi_ch - hi_beg].table))
	{
		return  0 ;
	}

	range = ${fromcs}_to_${tocs}_tables[ hi_ch - hi_beg].range ;
	
	if( ch < range->beg || range->end < ch)
	{
		return  0 ;
	}

	return  table[ LO(ch) - LO(range->beg)] ;
}

#undef  HI
#undef  LO

EOF

	printf TO  "#endif	/* REMOVE_MAPPING_TABLE */\n\n\n#endif\n" ;
}

sub output_table_to_ucs($$)
{
	if( keys %to_ucs > 256)
	{
		# MB charset
		output_separated( shift , "ucs4" , shift , \%to_ucs) ;
	}
	else
	{
		# SB charset
		output( shift , "ucs4" , shift , \%to_ucs) ;
	}
}

sub output_table_ucs_to($$)
{
	output_separated( "ucs4" , shift , shift , \%ucs_to) ;
}

# module return

1 ;
