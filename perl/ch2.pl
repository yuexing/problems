#!/usr/bin/perl -w
print "question 1: ".2 * 12.5 * 3.1415926."\n";
chomp($line=<STDIN>);
print "question 2: ".2 * $line * 3.1415926."\n";
chomp($line=<STDIN>);
print "question 3: ". 2 * ($line<0?0:$line)  * 3.1415."\n";
chomp($line=<STDIN>);
chomp($line2=<STDIN>);
print "question 4: ".$line * $line2."\n";
$line=<STDIN>;
chomp($line2=<STDIN>);
print "question 5: ".$line x $line2;


