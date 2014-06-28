# 

# Common targets for "packages" repository. Include configuration for prevent

PREVENT_UNIX=$(wildcard prevent-*.tar.gz)
PREVENT_WIN=$(wildcard prevent-*.zip)

.PHONY : all clean unzip-prevent unzip-prevent-unix unzip-prevent-win configure-prevent clean-prevent

# On MacOSX touching .a files freaks out the linker, so in that case
# we'll use "dummy" targets (i.e. targets not actually in the archive)
ifeq (,$(DUMMY_TARGETS))
CHECK_TARGET:=test -f $$@ || (echo "Tarball didn't contain target: $$@"; exit 1)
else
CHECK_TARGET:=true
endif

# Create a rule for a single package PACKAGE
# variables $(PACKAGE) and $(PACKAGE)_TARGET must have been defined
define create_package_rule

TARGET:=$$($(value 1)_TARGET)

all: $$(TARGET)

$$($(value 1)_TARGET): $$($(value 1))
	@# --no-same-owner is needed in some cases for admin-like Windows users
	@# fall back on not using --no-same-owner in case the option is unavailable
	tar -zx --no-same-owner -f $$^ 2>/dev/null || tar -zxf $$^
	@# Make sure the target was there
	@$(value CHECK_TARGETS)
	@# Touch target, since tar will preserve timestamps
	@touch $$@

endef

$(foreach package,$(PACKAGES),$(eval $(call create_package_rule,$(package))))

prevent: $(PREVENT)
	$(MAKE) unzip-prevent configure-prevent
	touch prevent

unzip-prevent-unix: $(PREVENT_UNIX)
	for pack in $(PREVENT_UNIX) -dummy- ; do \
		if [ -f $$pack ]; then \
			$(GUNZIP) -c $$pack | tar xf -; \
		fi \
	done

unzip-prevent-win: $(PREVENT_WIN)
	for pack in $(PREVENT_WIN) -dummy- ; do \
		if [ -f $$pack ]; then \
			unzip $$pack; \
		fi \
	done

configure-prevent:
	for p in prevent-* ; do if [ -d $$p ] ; then rm -rf prevent ; mv $$p prevent ; fi ; done
	if [ -d prevent ] ; then \
		cd prevent ; \
	    rm -rf config/*-config-* config/coverity_config.xml ; \
		./bin/cov-configure --compiler ../bin/ccache --comptype prefix ; \
		./bin/cov-configure --template --compiler gcc ; \
		./bin/cov-configure --template --compiler g++ ; \
	fi

clean-prevent:
	rm -rf prevent

clean:
	@# Remove all directories except CVS
	find . -mindepth 1 -maxdepth 1 \
	'(' -name CVS -prune ')' \
	-o '(' -type d -print ')' | xargs rm -r
	@# Also remove "prevent", which may be a regular file
	rm -rf prevent *-stamp
