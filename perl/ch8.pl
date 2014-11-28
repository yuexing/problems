# (), (?:), (?<LABEL>) \1 \2 $1 $2
# .*?{1,2}
# ^$\b
# |
# []
# $word =~ /pattern/

while (<>) {
	if(/\bmatch\b/) {
		print $_;
	}
	if(/\b(.*)a\b/) {
		print "($1)a\n";
	}
}
