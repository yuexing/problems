open(TRAIN_SET, $ARGV[0]) or die "can not open $ARGV[0] \n";
while(<TRAIN_SET>) {
	chomp;
	$islib=/lib/;
	open(TRAIN, "data/$_") or die "can not open $_ \n";
	while(<TRAIN>) {
		chomp;
		$dict{$_}++;
		if($islib){ $libpos++, $lib{$_}++;}
		else {$condpos++, $cond{$_}++;}
	}
	close TRAIN;
}
close TRAIN_SET;

open(TEST_SET, $ARGV[1]) or die "can not open $ARGV[1] \n";
while(<TEST_SET>) {
	chomp;
	open(TEST, "data/$_") or die "can not open $_ \n";
	$file = $_;
	my $lc_log = 0;
	while(<TEST>) {
		chomp;
		if($dict{$_}) {
			$lc_log=$lc_log+log((($lib{$_}+1)/($libpos+keys %dict))/
			(($cond{$_}+1)/($condpos+keys %dict)));
		}
	}
	close TEST;
	if($lc_log > 0){ print "$file L\n";}
	else {print "$file C\n";}
}
close TEST_SET;
