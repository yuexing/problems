# Don't always include all.mk as it is slow to parse

# Coarse attempt to ensure any subsequent submakes
# use the same version of make that executed this top script
export MAKE

export CDPATH=

.PHONY: default packages release

# Version check.  See bug 35898.
MAKE_VERSION_SPLIT := $(subst ., ,$(MAKE_VERSION))
MAKE_VERSION_MAJOR := $(word 1,$(MAKE_VERSION_SPLIT))
MAKE_VERSION_MINOR := $(word 2,$(MAKE_VERSION_SPLIT))
MAKE_VERSION_OK := $(shell expr $(MAKE_VERSION_MAJOR) \> 3 \| \
	$(MAKE_VERSION_MAJOR) = 3 \& $(MAKE_VERSION_MINOR) \>= 81)
ifneq ($(MAKE_VERSION_OK),1)
$(error This build system requires GNU make 3.81+.  Current is "$(MAKE_VERSION)")
endif

default: all

help::
	@echo "\"make {build,test}_help\" for help on {building,testing}"

build_help::
	@echo "Building from the root is very slow due to numerous dependencies,"
	@echo "so build something more specific, such as analysis/cov-analyze,"
	@echo "when you can."
	@echo ""

# make sure 'testsuite-prereq' is run before any other testsuite target
testsuite: testsuite-prereq
continue-testsuite: testsuite-prereq

# Pull in the testsuite targets, since they are needed later.  This
# also includes personal.mk for $(RUN_DESKTOP_DURING_MAKE), and
# config.mk for $(mc_platform).
include new-rules/new-testsuite.mk

ifeq (,$(NO_PACKAGE_CHECK))
all: check-packages
endif

all: 
	$(MAKE) -f all.mk all
ifneq ($(RUN_DESKTOP_DURING_MAKE),)
	@# This is a separate command so it happens after all the
	@# build steps even in a parallel build.
	$(MAKE) -f desktop.mk
endif

.PHONY: check-packages checkout-packages

# Checkout packages / platform packages, with appropriate tag, etc.
build_help::
	@echo "{checkout,update,check,}-packages # update/checkout needed before building"

checkout-packages:
	$(MAKE) -f checkout.mk checkout

update-packages:
	$(MAKE) -f checkout.mk update

check-packages:
	$(MAKE) -f checkout.mk check

packages:
	$(MAKE) -f packages.mk packages

packages-clean:
	$(MAKE) -f packages/Makefile clean

docs:
	$(MAKE) -f checkout.mk docs

checkout-clang: checkout-packages
	$(MAKE) -f checkout.mk clang-sources
	cd $(mc_platform)-packages ; $(MAKE)

# All LLVM/Clang artifacts (headers, libs) extracted by checkout-packages
# must be removed prior to building Clang.  The Clang build relies on
# system headers from <platform>-packages, but if LLVM/Clang headers are
# present there, then the wrong LLVM/Clang headers might get referenced
# during the Clang build.
nightly-clang: checkout-clang
	rm -rf $(mc_platform)-packages/include/clang
	rm -rf $(mc_platform)-packages/include/clang-c
	rm -rf $(mc_platform)-packages/include/llvm
	rm -rf $(mc_platform)-packages/include/llvm-c
	rm -rf $(mc_platform)-packages/lib/clang
	rm -f $(mc_platform)-packages/lib/libclang*
	rm -f $(mc_platform)-packages/lib/libLLVM*
	cd clang-sources ; $(MAKE) nightly

# Doesn't depend on "all" because "all" requires a lot of parsing
# However one should make sure everything is built before trying this
release:
	$(MAKE) -f release.mk release
audit-release:
#	$(MAKE) -f release.mk audit-release
	@echo "The audit product has been deprecated."

clean:
	$(MAKE) -f clean.mk clean

# requires that 'linklint' be installed; see comments in index.html
linkcheck:
	$(RM) -r linklint-out
	./utilities/linklint.pl -db1 -db2 -error -warn -doc linklint-out /@
	touch linklint-out/.bk_skip

# Run before the main testsuite, when 'make testsuite' invoked from the
# top level of the repo.
testsuite-prereq:
	@echo "Checking prerequisites for toplevel testsuite ..."
ifeq ($(mc_platform),solaris-x86)
	@if ulimit -a | grep stack | grep unlimited; then \
		echo "Unlimited stack size on solaris/x86 is bad; see OpenSolaris bug 6374692 and GNU make bug 18396.  The simplest solution is to say 'ulimit -s -H <value>' before invoking 'make'.  The <value> must be no greater than the current hard limit."; \
		exit 2; \
	fi
endif
ifneq ($(filter linux linux64 linux-ia64,$(mc_platform)),)
	@if which strace >/dev/null 2>&1; then \
		true; \
	else \
		echo "strace is required on linux platforms, e.g., by build/cov-build/test-deps19-linux"; \
		exit 2; \
	fi
endif
	@if [ -f $(top_srcdir)/$(RELEASE_ROOT_DIR)/config/coverity_config.xml ]; then \
		echo "You should not have a coverity_config.xml in root/config. Please remove it."; \
		exit 2; \
	fi
	@echo "Done.  Proceeding with testsuite."

# Run by the central build after the main testsuite to confirm junk is
# not left behind.  See bug 18856.
#
# This is *not* run automatically by "make testsuite" because it is
# common to have some locally modified or not-checked-in files, and I
# don't want that to cause problems.  The central build, however,
# should not have any such files.
#
# 2012-05-04: Recently, a number of Windows builds have been failing
# because of .exe.stackdump files being left over, for apparently no
# good reason. The check now ignores files ending in .stackdump using
# the extra grep.  -pdillinger
#
# And now it is disabled altogether.  See bug 43583.
testsuite-post-check:
	@echo "testsuite-post-check disabled for now; see bug 43583"

look-for-test-junk:
	if git ls-files --others --exclude-standard | grep -v '[.]stackdump'; then \
	  echo "Some test left some files behind.  Please fix the test(s)."; \
	  exit 2; \
	else \
	  echo "post-check succeeded"; \
	fi

# Run prevent on prevent.  Assumes we already built Prevent (the
# analyzer, not the analyzee).
prevent-on-prevent:
	$(MAKE) -f pop.mk prevent-on-prevent

# Run this to update the build artifacts after renaming the repo
# directory if you do that.  This is done as part of the STS machinery
# for bug 31458.  The intent is that this command is both faster than
# "make clean all" and depends on a much smaller set of tools.
#
# The basic strategy is to remove anything under objs that depends
# on the repo directory name *or* the build machine host name, then
# do a full incremental "make" to rebuild as necessary.
#
# However, due to bug 40002, that normally causes spurious
# recompilation, which often fails on the test machines.  Therefore, I
# pass the names of certain generated files to make with -o, so it
# will pretend they are old, thus cutting off the problematic chains.
build_help::
	@echo "update-directory-references # use this after renaming a repo"

update-directory-references:
	rm -rf objs/$(mc_platform)/root/gcc-config
	$(RM) objs/$(mc_platform)/root/test-data/rt-jar-idir.tgz
	$(MAKE) -f all.mk \
	  -o objs/$(mc_platform)/libs/libexternal-version.a \
	  -o objs/$(mc_platform)/gensrc/ast/cc.ast.cpp \
	  all

# This target is intended to run the "full" testsuite, to be run from
# STS, whereas the "normal" testsuite will be kept smaller to ensure
# fast builds.
testsuite-for-sts:
	$(MAKE) TESTSUITE_FOR_STS=1 slow-testsuite
	# See "testsuite-for-slow-build" target for rationale for putting this test here.
	$(MAKE) TESTSUITE_FOR_STS=1 -C analysis/parallel-tests/options-combinations/slow-test-3.1b
	$(MAKE) -C analysis/checkers/models/check-shipped-models check-shipped-models

# This target is intended to run the "full" testsuite, including the
# slow tests and the Java bootclasspath / C# system assemblies by-default.
# This is associated with the "slow" continuous integration jobs.
testsuite-for-slow-build:
	TESTSUITE_EMIT_CORE_LIBRARIES_BY_DEFAULT=1 $(MAKE) slow-testsuite
	# The "slow-test-3.1b" test is here because it runs a bunch of analysis runs in
	# parallel, which would cause too much contention when run within an already
	# parallel testsuite run. Putting it here also means not passing variable
	# TESTSUITE_EMIT_CORE_LIBRARIES_BY_DEFAULT to it, which would needlessly slow
	# it down. See BZ 49357.
	$(MAKE) -C analysis/parallel-tests/options-combinations/slow-test-3.1b
	$(MAKE) -C analysis/checkers/models/check-shipped-models check-shipped-models

# The problem with using coverage is that it causes linkage errors.
# Fortunately, it is easy to recognize the objects to remove, they
# have an equivalent ending with '.gcno'
clean-coverage:
	find objs/$(mc_platform)/objs -name '*.gcno' | \
		sed 's/gcno$$/o/' | xargs rm

# Debug symbols can cause slow linking.
# This target uses "nm" to tell object files with debug symbols and delete them.
clean-debug:
	for i in `find objs/$(mc_platform)/objs -name '*.o'`; do \
		if $(mc_platform)-packages/bin/nm -a "$$i" | grep -q " N "; then \
			echo "rm $$i"; rm "$$i"; \
		fi; \
	done

# Install JPrevent pieces to support cov-build on java, etc.
copy-jprevent-build:
ifneq ($(JPREVENT_BUILD),)
	cd $(JPREVENT_BUILD); ant -Dprevent.objs.root=$(PWD)/$(RELEASE_ROOT_DIR) \
		-f build.xml prevent.build.copy
else
	$(error The target copy-jprevent-build requires JPREVENT_BUILD to be set)
endif


BUILTIN_MODEL_DB:=objs/$(mc_platform)/root/config/builtin-models.db

# Target to regenerate the JRE and Android models.
# This requires generating a different built-in model DB, so force
# rebuilding it, and delete it afterwards to make sure we don't keep a
# stale one.
build_help::
	@echo "regenerate-models # rebuild all model libraries"
	@echo "regenerate-{jre,android,cs}-models # rebuild one model library"

regenerate-models: testsuite-shipping-models-linux64-check
	rm -f $(BUILTIN_MODEL_DB)
	$(MAKE) -f cov-analyze.mk \
		GENERATE_LIB_MODELS=1 lib-models

# Regenerate only the JRE models.
# Note: after this, you'll have a stale built-in model DB (because
# it's missing C# and android models, which depend on JRE models).
# Make sure to regenerate it later.
regenerate-jre-models: testsuite-shipping-models-linux64-check
	rm -f $(BUILTIN_MODEL_DB)
	$(MAKE) -f cov-analyze.mk \
		GENERATE_LIB_MODELS=1 jre-models

# Same as above, for Android models.
# Note: As above, you'll have a stale built-in model DB
# (because it's missing C# models).
regenerate-android-models: testsuite-shipping-models-linux64-check
	rm -f $(BUILTIN_MODEL_DB)
	$(MAKE) -f cov-analyze.mk \
		GENERATE_LIB_MODELS=1 \
		GENERATE_ANDROID_ONLY=1 \
		android-models

# Same as above, for C# system models.
# Note: As above, you'll have a stale built-in model DB
# (because it's missing JRE / android models).
regenerate-cs-models: testsuite-shipping-models-linux64-check
	rm -f $(BUILTIN_MODEL_DB)
	$(MAKE) -f cov-analyze.mk \
		GENERATE_LIB_MODELS=1 cs-system-models

# Copy model DBs to the remote repository
copy-models:
	$(MAKE) -f cov-analyze.mk \
		GENERATE_LIB_MODELS=1 copy-models

# EOF
