# build-targets.mk
# Creates a "default" and a "clean" target for a program, library, or a subset of sources thereof.
# Before including this, you need to set one of PROG or LIB or SHLIB (to a prog.mk or directory containing a lib.mk or shlib.mk, respectively)
# and optionally set SOURCES (the sources must be relative to the directory containing prog.mk or lib.mk
# This will create 2 rules:
# lib & clean-lib or prog & clean-prog
# If SOURCES is specified, also
# objs & clean-objs
# If SOURCES is specified and DEFAULT_FULL is not, the default will be objs and clean will be clean-objs; otherwise, the default will be lib/prog and clean clean-lib/clean-prog
# The default target will be "default-build-targets", a "::" rule that can be added to.
# The UNIT_TESTER variable can also be set to a prog.mk, in which case it will have its own targets added, which will be added to the default and clean targets.

# This file can be included once with a "PROG" and then with a "LIB", as long as SOURCES is unset in at least one case (SOURCES is reset automatically)

# If CSHARP_CLEAN is set, then a special "clean" will be used.

RULES:=$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
REPO_ROOT:=$(RULES)..

# First target is default, even if not called "default"
# We'll define "default" later
first-in-build-targets: default-build-targets

ifneq ($(PROG),)

BUILD_MAKEFILE:=new-rules/make-one-prog.mk
BUILD_ARG:=PROG=$(PROG)
RULE_NAME:=prog

else ifneq ($(LIB),)

BUILD_MAKEFILE:=new-rules/make-one-lib.mk
BUILD_ARG:=LIB=$(LIB)
RULE_NAME:=lib

else ifneq ($(SHLIB),)

BUILD_MAKEFILE:=new-rules/make-one-shlib.mk
BUILD_ARG:=SHLIB=$(SHLIB)
RULE_NAME:=shlib

else

$(error Must set PROG or LIB or SHLIB)

endif

MAKE_ARGS:=-C $(REPO_ROOT) -f $(BUILD_MAKEFILE) $(BUILD_ARG)

$(RULE_NAME): MAKE_ARGS:=$(MAKE_ARGS)

$(RULE_NAME):
	$(MAKE) $(MAKE_ARGS)

clean-$(RULE_NAME): MAKE_ARGS:=$(MAKE_ARGS)

clean-$(RULE_NAME):
	$(MAKE) $(MAKE_ARGS) clean
ifneq ($(CSHARP_CLEAN),)
	$(MAKE) $(MAKE_ARGS) MSBUILD_OPTS="/t:Clean"
endif

ifneq ($(SOURCES),)

MAKE_ARGS:=-C $(REPO_ROOT) -f new-rules/make-one-obj.mk \
	SOURCES="$(SOURCES)" \
	$(BUILD_ARG)

objs: MAKE_ARGS:=$(MAKE_ARGS)

objs:
	$(MAKE) $(MAKE_ARGS)

clean-objs: MAKE_ARGS:=$(MAKE_ARGS)

clean-objs:
	$(MAKE) $(MAKE_ARGS) clean

else

DEFAULT_FULL:=1

endif

ifneq ($(UNIT_TESTER),)

MAKE_ARGS:=-C $(REPO_ROOT) -f new-rules/make-one-prog.mk PROG=$(UNIT_TESTER)

unit-tester: MAKE_ARGS:=$(MAKE_ARGS)

unit-tester:
	$(MAKE) $(MAKE_ARGS)

default-build-targets:: unit-tester

clean-unit-tester: MAKE_ARGS:=$(MAKE_ARGS)

clean-unit-tester:
	$(MAKE) $(MAKE_ARGS) clean

clean:: clean-unit-tester

endif # UNIT_TESTER

ifneq ($(DEFAULT_FULL),)

default-build-targets:: $(RULE_NAME)

clean:: clean-$(RULE_NAME)

ifneq ($(DOX_DEFINED),1)
DOX_DEFINED:=1

.PHONY: dox
dox:
	mkdir -p dox || true;

	doxygen -g dox/Doxyfile.template;
	cat dox/Doxyfile.template | \
	  grep -v 'LAYOUT_FILE' | \
	  grep -v 'GENERATE_LATEX' \
	  > dox/Doxyfile;

	echo "INPUT = ." >> dox/Doxyfile;
	echo "HTML_OUTPUT = dox/" >> dox/Doxyfile;
	echo "LAYOUT_FILE = `pwd`/dox/DoxygenLayout.xml" >> dox/Doxyfile;
	echo "QUIET = YES" >> dox/Doxyfile;
	echo "WARN_IF_UNDOCUMENTED = NO" >> dox/Doxyfile;
	echo "WARN_LOGFILE = dox/warnings.log" >> dox/Doxyfile;
	echo "GENERATE_LATEX = NO" >> dox/Doxyfile;
	echo "EXCLUDE_PATTERNS  = " \
	       "*/SCCS/*" \
	       "*/*test*" \
	       "*/html/*" \
	     >> dox/Doxyfile

	doxygen -l dox/DoxygenLayout.xml.template;
	cat dox/DoxygenLayout.xml.template | \
	  sed 's/type="mainpage" visible="yes"/type="mainpage" visible="no"/' | \
	  sed 's/type="pages" visible="yes"/type="pages" visible="no"/' | \
	  sed 's/type="modules" visible="yes"/type="modules" visible="no"/' \
	  > dox/DoxygenLayout.xml;
	doxygen dox/Doxyfile;
	@echo "open dox/index.html in a browser to see doxygen output"

endif

else

default-build-targets:: objs

clean:: clean-objs

endif # DEFAULT_FULL

build_help::
	@$(MAKE) $(MAKE_ARGS) build_help

test_help::
	@$(MAKE) $(MAKE_ARGS) test_help

help::
	@$(MAKE) $(MAKE_ARGS) help


# Reset argument variables, to allow including this file again with
# different arguments (e.g. a prog + a lib)
PROG:=
LIB:=
SOURCES:=
UNIT_TESTER:=
CSHARP_CLEAN:=

# Usually a useful thing to include, but occasionally not.
ifeq ($(NO_TESTSUITE_TARGET),)
include $(RULES)new-testsuite.mk
endif
