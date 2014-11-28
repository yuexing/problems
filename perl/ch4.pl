#!/usr/bin/perl
use feature 'state';

sub total{
	my $sum;
	foreach(@_) {
		$sum += $_;
	}
	$sum;
}

my @fred = (1..9);
$t=&total(@fred);
print "The total of @fred: ".$t."average is: "
.$t.", ".$#fred.", ".$t/$#fred."\n";

sub greet{
	state @lasts;
	if(@lasts eq 0) {
		print $_, " you are the first\n";
	} else {
		print $_, " I have seen: @lasts \n";
	}
	push(@lasts, $_);
}

@names=qw(yue, tiant, tt, xy);
foreach(@names) {
	&greet($_);
}
