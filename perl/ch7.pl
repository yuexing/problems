while(<>){
	if(/^freds?.*al$/ || /^\..*/) {
		print $_;
	}
}
