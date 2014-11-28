#!/usr/bin/perl -w

chomp($root=$ARGV[0]);
opendir(DIR, $root) or die "Can not open $root";
my @benches = readdir(DIR);
closedir(DIR);
while (my $bench = shift @benches) {
    print "$bench";
}
