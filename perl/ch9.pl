# s///
# $^I

$_="green scaly dinosaur\n";

s/(\w+) (\w+)/$2, $1/;
print $_;
s/^/huge,/;
print $_;
s/\n$/ what?\n/;
print $_;
s/\b.*een//;
print $_;
