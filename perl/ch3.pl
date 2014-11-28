#!/usr/bin/perl

@names=qw(fred betty barney dino wilma pebble bamm-bamm);
@names = sort @names;
chomp @names;
print join(",", @names);

@reversed=reverse(@lines=<STDIN>);
print @reversed;

chomp(@inx=<STDIN>);
foreach (@inx){
	print @names[$_]."\n";
}
