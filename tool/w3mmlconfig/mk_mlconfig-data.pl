
while(<>) {
	/^#/ && next;
	chop;
	($s, $c, $r, $t) = split(" ", $_, 4);
	push(@section, $s);
	$section_attr->{$s} = {
		col => $c,
		row => $r,
		title => $t,
		key => [],
	};
	&read_section($s);
}

print "\@section = (\n";
for $s (@section) {
	print "\t'$s',\n";
}
print ");\n";
print "\$section_attr = {\n";
for $s (@section) {
	print "\t$s => {\n";
	print "\t\tcol => $section_attr->{$s}{col},\n";
	print "\t\trow => $section_attr->{$s}{row},\n";
	print "\t\ttitle => '$section_attr->{$s}{title}',\n";
	print "\t\tkey => [\n";
	for $k (@{$section_attr->{$s}{key}}) {
		print "\t\t\t'$k',\n";
	}
	print "\t\t],\n";
	print "\t},\n";
}
print "};\n";
print "\$key_attr = {\n";
for $s (@section) {
for $k (@{$section_attr->{$s}{key}}) {
	print "\t$k => {\n";
	print "\t\ttype => '$key_attr->{$k}{type}',\n";
	print "\t\tdefault => '$key_attr->{$k}{default}',\n";
	print "\t\tcol => $key_attr->{$k}{col},\n";
	print "\t\ttitle => '$key_attr->{$k}{title}',\n";
	if (@{$key_attr->{$k}{item}}) {
		print "\t\titem => [\n";
		for $i (@{$key_attr->{$k}{item}}) {
			print "\t\t\t'$i',\n";
		}
		print "\t\t],\n";
	}
	print "\t},\n";
}
}
print "};\n";
print "\$item_attr = {\n";
for $s (@section) {
for $k (@{$section_attr->{$s}{key}}) {
	@{$key_attr->{$k}{item}} || next;
	print "\t$k => {\n";
	for $i (@{$key_attr->{$k}{item}}) {
		print "\t\t'$i' => '$item_attr->{$k}{$i}',\n";
	}
	print "\t},\n";
}
}
print "};\n";

sub read_section {
	local($s) = @_;
	local($_, $k, $T, $c, $t);

	open(S, "section/$s");
	while(<S>) {
		/^#/ && next;
		chop;
		($k, $T, $d, $c, $t) = split(" ", $_, 5);
		push(@{$section_attr->{$s}{key}}, $k);
		$key_attr->{$k} = {
			type => $T,
			default => $d,
			col => $c,
			title => $t,
			item => [],
		};
		if ($T =~ /^select/ || $T =~ /^radio/) {
			&read_item($k);
		}
	}
	close(S);
}

sub read_item {
	local($k) = @_;
	local($_, $v, $t);

	open(K, "key/$k");
	while(<K>) {
		/^#/ && next;
		chop;
		($v, $t) = split(" ", $_, 2);
		push(@{$key_attr->{$k}{item}}, $v);
		$item_attr->{$k}{$v} = $t;
	}
	close(K);
}
