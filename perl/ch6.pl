%coins= ( "Quarter"=>25, "Dime"=>10, "Nickel"=>5);
print %coins, "\n";

@ks = keys %coins;
@vs = values %coins;

print "@ks", "\n", "@vs", "\n";
print "\n";

while(($k, $v) = each %coins) {
	print "$k => $v\n";
}

print "\n";
foreach (sort keys %coins) {
	print "$_ => $coins{$_}\n";
}

