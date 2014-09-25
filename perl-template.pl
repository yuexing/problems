use strict;
use warnings;

my $os = "$^O";
print $os;

my @arr = sort { $a cmp $b } qw(
    filter
    by 
    the 
    white_spaces
);

if((scalar @arr) > 1) {
    print "@arr \n";
}

my %hash = ('host' => 'localhost',
            'user' => 'yxing');

# The foreach keyword is actually a synonym for the for keyword, so you can
# use foreach for readability or for for brevity.
while (my ($k,$v)=each %hash){print "$k $v\n"}
foreach my $k (keys %hash) {
    print "$k, $hash{$k}\n";
}

# parse command line
sub print_usage_and_die {
    die "@_";
}

foreach my $arg (@ARGV) {
    if($arg =~ /^--control=(.*)/) {
        print $1;
    } elsif($arg eq '--verbose') {
        print $arg;
    } else {
        print_usage_and_die "ERROR: unknown arg $arg";
    }
}

# write to a file
my $file = "test";
# open FILEHANDLE[, MODE], EXPR
open(FD, '>', $file) or die "open readonly $file: $!";
print FD "stuff... \n";
close(FD);

open(FD, '<', $file) or die "open writeonly $file: $!";
# my @lines = <FILE>
# while (my $line = <FD>)
while(<FD>) {
    print;
}
close(FD);

unlink($file);

# pipe
open(LS, "ls|") or die "ls failed: $!";
my @lines = <LS>;
printf "", (scalar @lines), "\n";

# small functions
sub fun {
    my $arg0 = shift;
    print "$arg0, @_ \n";
}
fun 1, 2, 3;

# pass as reference
sub calc {
    my @arg0 = @{$_[0]};
    my $arg1 = $_[1];
    my %arg2 = %{$_[2]};
    print "@arg0, $arg1 \n";
    foreach (%arg2) {
        print;
    }
    print "\n";
}
# passing func(@arr): in the sub, @_'s elements start off aliased to the 
# elements passed to the sub.
# implies: no need to pass by taking reference.
calc(\@arr, 1, \%hash);

# finally more about strings
# single quote: no interpolate
# double ~: interpolate, then need escape, and more escape to escape the escape

# qq works like a special double quote while q works like a single quote.
print qq(for the case where " is used);
print qq{what if ) ( are used};
print q[The )name} is "$name"\n];

# variable declare:
# my:
# our:
# state:

# &callsite, help eliminate conflict with builtin function.
