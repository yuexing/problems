include new-rules/all-rules.mk

# DR15248: only package and build/test Extend on supported platforms
# This variable is set in new-rules/config-common.mk.
ifeq ($(PLATFORM_HAS_EXTEND),1)

# Copy all files for extend

EXTEND_ROOT_DIR:=$(RELEASE_ROOT_DIR)/sdk
EXTEND_COMPILER_DIR:=$(EXTEND_ROOT_DIR)/compiler

# check if the doc-dev directory exist
#DOC_REP_DIR=$(shell ls | grep "^$(DOC_REP)$$" )

EXTEND_FILES:=

# args:  srcdir targetdir filename
define extend_setup_file_copy_2_dirs
SOURCE:=$(value 1)$(value 3)
TARGET:=$$(EXTEND_ROOT_DIR)/$(value 2)$(value 3)

$$(TARGET): $$(SOURCE)
	@mkdir -p "$$(dir $$@)"
	@rm -f "$$@"
	$(COVCP) "$$<" "$$@"

ALL_TARGETS+=$$(TARGET)
all:$$(TARGET)

EXTEND_FILES+=$$(TARGET)

endef

EXTEND_HEADER_DIR:=headers

# Headers in analysis
ANALYSIS_HEADERS:=\
  errrep/event-stream.hpp \
  extend/extend-checker-types.hpp \
  extend/extend-lang.hpp \
  extend/extend-lang.h \
  extend/extend-store.hpp \
  traversal/abstract-interp-fwd.hpp

$(foreach file,$(ANALYSIS_HEADERS),$(eval $(call extend_setup_file_copy_2_dirs,analysis/,$(EXTEND_HEADER_DIR)/,$(file))))

# Headers in analysis/ast
ANALYSIS_AST_HEADERS:=\
  ast/ast-print-ops.hpp \
  ast/astflatten-fwd.hpp \
  ast/cc_flags.hpp \
  ast/cc.ast-fwd.hpp \
  ast/cfg-fwd.hpp \
  ast/astnode.hpp \
  ast/cc.ast.hpp \
  ast/astnode-fwd.hpp \
  ast/cc.ast \
  ast/exception-handling-fwd.hpp \
  ast/extend-only-api.hpp \
  ast/precedence.hpp \
  interp/denv-fwd.hpp \
  interp/dvalue-fwd.hpp \
  emit/emit-db-fwd.hpp \
  emit/emitsrcloc.hpp \
  emit/tu-index.hpp \
  scope/scope-fwd.hpp \
  symbols/symbol-fwd.hpp \
  symbols/symbol.hpp \
  symbols/variable.hpp \
  symbols/field.hpp \
  symbols/function.hpp \
  types/scalar-types.hpp \
  types/extend-types.hpp \
  types/types-fwd.hpp \
  types/type-gen-fwd.hpp \
  types/type-names.hpp \
  patterns/expression-patterns.hpp \
  patterns/extend-patterns-fwd.hpp \
  patterns/type-patterns.hpp \
  patterns/symbol-patterns.hpp \
  patterns/statement-patterns.hpp \
  patterns/astnode-patterns.hpp \
  patterns/extend-patterns.hpp

$(foreach file,$(ANALYSIS_AST_HEADERS),$(eval $(call extend_setup_file_copy_2_dirs,analysis/ast/,$(EXTEND_HEADER_DIR)/,$(file))))

# Headers in libs
ANALYSIS_LIBS_HEADERS:=\
  allocator/allocator-fwd.hpp \
  allocator/allocator.hpp \
  allocator/ownervectora.hpp \
  allocator/vectora.hpp \
  arena/arena-fwd.hpp \
  asthelp/asthelp-aux.hpp \
  bit-masks-alt.hpp \
  bit-masks.hpp \
  bits/bits.hpp \
  char-enc/encoding-fwd.hpp \
  compiler-extensions.hpp \
  containers/array-fwd.hpp \
  containers/array.hpp \
  containers/bitmap-byte.hpp \
  containers/container-functions.hpp \
  containers/container-utils.hpp \
  containers/enumset.hpp \
  containers/foreach.hpp \
  containers/iterator-utils.hpp \
  containers/iterator.hpp \
  containers/list-fwd.hpp \
  containers/list.hpp \
  containers/map-fwd.hpp \
  containers/map.hpp \
  containers/output.hpp \
  containers/pair.hpp \
  containers/recursive-iterator.hpp \
  containers/set-fwd.hpp \
  containers/set.hpp \
  containers/vector-fwd.hpp \
  containers/vector.hpp \
  db/sql-base-types.hpp \
  exceptions/assert-lean.hpp \
  exceptions/backtrace-data-fwd.hpp \
  exceptions/exceptions.hpp \
  exceptions/source-location.hpp \
  exceptions/xmsg.hpp \
  file/filename-class.hpp \
  file/filename-class-fwd.hpp \
  file/transform-filename-fwd.hpp \
  istring/istring-fwd.hpp \
  istring/istring.hpp \
  file/path-fwd.hpp \
  flatten/flatten-fwd.hpp \
  languages/tu-lang.hpp \
  macros.hpp \
  optional/optional-nonnegative-int.hpp \
  owner.hpp \
  parse-errors/parse-errors-fwd.hpp \
  regex/regex-fwd.hpp \
  scoped-ptr.hpp \
  sized-types.hpp \
  smbase/srcloc.hpp \
  smbase/locinfile.hpp \
  structured-text/structured-text-fwd.hpp \
  summary-server-data/analysis-handle.ast-fwd.hpp \
  system/is_windows.hpp \
  text-flatten/text-flatten-fwd.hpp \
  text/char-traits-fwd.hpp \
  text/debug-print.hpp \
  text/iostream-fwd.hpp \
  text/iostream.hpp \
  text/md5-fwd.hpp \
  text/sstream.hpp \
  text/stream-utils.hpp \
  text/string-allocator-fwd.hpp \
  text/string-compare.hpp \
  text/string-fwd.hpp \
  text/string.hpp \
  text/stringb.hpp \
  tribool.hpp \

$(foreach file,$(ANALYSIS_LIBS_HEADERS),$(eval $(call extend_setup_file_copy_2_dirs,libs/,$(EXTEND_HEADER_DIR)/,$(file))))

EXTEND_LIBS:=\
cppunit \
allocator \
analysis \
analysis-metrics \
analysis-parse \
arena \
ast \
asthelp \
auth-key \
base64 \
bits \
bytecode-dump \
build \
cache \
char-enc \
cic \
constraint \
containers \
cov-crypto \
crypto \
cstream \
datablock \
db \
debug \
debug-heap \
distributor \
external-version \
file \
flatten \
global-dataflow \
graph-search \
hostid \
i18n \
icudata \
icuuc \
icui18n \
import-results \
internal-version \
istring \
iter-emit \
itype \
java-framework-analyzer \
java-runtime \
java-security-da \
jsoncpp \
kvs \
languages \
license \
lock \
logging \
lossy-trie \
message \
minisat-p_v1.14 \
new-merge-key \
optional \
options \
pool \
properties-file \
program-metrics \
regex \
script-data \
script \
smbase \
sqlite \
os-cmd-context \
sql-context \
structured-text \
summary-server-data \
system \
testanalysis-data \
testanalysis-policy-parser \
text \
text-flatten \
time \
unmangle \
varwidth \
xss-prover \
xss-context \
xml \
xml2 \
xmlwrapp-xml \
zip  \
unzip \
minizip \
minizip-wrapper \

ifneq ($(filter linux linux64,$(mc_platform)),)
  # FNP 11.11
  EXTEND_LIBS+=lm_new lmgr_nomt crvs sb lmgr_dongle_stub noact flexnet-stubs
endif

ifeq (macosx,$(mc_platform))
  # FNP 11.11
  EXTEND_LIBS+=lm_new lmgr_nomt crvs sb lmgr_dongle_stub noact
endif

ifneq ($(filter solaris-x86 solaris-sparc hpux-pa,$(mc_platform)),)
  # FNP 11.11, but needs no stubs
  EXTEND_LIBS+=lm_new lmgr_nomt crvs sb noact
endif

ifeq ($(mc_platform),mingw)
  # FNP 11.5
  EXTEND_LIBS+=lm_new lmgr_md crvs  noact  sb  chkstk
endif

# we don't really need these here, since they are all copied over anyway
ifeq ($(mc_platform),mingw64)
  # FNP 11.5
  EXTEND_LIBS+=lm_new lmgr_md crvs noact sb chkstk cov_msvc_gs cov_import_symbols lmgr_dongle_stub	
endif

LIBEXTEND:=\
$(EXTEND_ROOT_DIR)/libs/libextend.a

$(LIBEXTEND): EXTEND_TMP_DIR:=$(OBJ_DIR)/extend-tmp-dir-libextend
$(LIBEXTEND): TARG_DIR:=$(EXTEND_ROOT_DIR)/libs

$(OBJ_DIR)/libs/liblm_new.a: $(PLATFORM_PACKAGES_DIR)/lib/liblm_new.a
	cp -f $^ $@ 

$(OBJ_DIR)/libs/liblmgr_nomt.a: $(PLATFORM_PACKAGES_DIR)/lib/liblmgr_nomt.a
	cp -f $^ $@ 

$(OBJ_DIR)/libs/libcrvs.a: $(PLATFORM_PACKAGES_DIR)/lib/libcrvs.a
	cp -f $^ $@ 

$(OBJ_DIR)/libs/libsb.a: $(PLATFORM_PACKAGES_DIR)/lib/libsb.a
	cp -f $^ $@ 

$(OBJ_DIR)/libs/libnoact.a: $(PLATFORM_PACKAGES_DIR)/lib/libnoact.a
	cp -f $^ $@ 

$(OBJ_DIR)/libs/libflexnet-stubs.a: $(PLATFORM_PACKAGES_DIR)/lib/libflexnet-stubs.a
	cp -f $^ $@

$(OBJ_DIR)/libs/libchkstk.a: $(PLATFORM_PACKAGES_DIR)/lib/libchkstk.a
	cp -f $^ $@

$(OBJ_DIR)/libs/liblmgr_md.a: $(PLATFORM_PACKAGES_DIR)/lib/liblmgr_md.a
	cp -f $^ $@

$(OBJ_DIR)/libs/liblmgr_dongle_stub.a: $(PLATFORM_PACKAGES_DIR)/lib/liblmgr_dongle_stub.a
	cp -f $^ $@

$(OBJ_DIR)/libs/libcov_import_symbols.a: $(PLATFORM_PACKAGES_DIR)/lib/libcov_import_symbols.a
	cp -f $^ $@

$(OBJ_DIR)/libs/libcov_msvc_gs.a: $(PLATFORM_PACKAGES_DIR)/lib/libcov_msvc_gs.a
	cp -f $^ $@

# Extract all the needed libraries into one
# ar only take objects, not other archives, so extract first.
# Use ar q because it's not guaranteed the object file names are unique
# -> Modified to use "ar r" and to rename object files with colliding names
# Also ar x doesn't take a target directory
# and the source is probably relative
# We assume it is and complete with $PWD
# Then cd to the target directory and back

# freebsd doesn't like pushd, so use cd / cd -
# 
# dchai 
# XXX: we use mingw's 'ar' when appropriate, so we need to call SHELL_TO_NATIVE
# XXX: the 'ar' command fails when repackaging Macrovision libraries on
#      Windows because the consituent object files have an 'obj' extension.
#      Hence the FLEXNET_ARCHIVES_TO_COPY stuff below.
#      Fixing this will require a Release Note because some lib*.a files will
#      go away, and customers might have manually written Makefiles of their
#      own.  Hold off on this until we upgrade Macrovision (if ever).
$(LIBEXTEND): \
$(foreach lib,$(EXTEND_LIBS),$(OBJ_DIR)/libs/lib$(lib).a)
	rm -f $@
	mkdir -p $(TARG_DIR)
	set -e; \
	rm -rf $(EXTEND_TMP_DIR); \
	mkdir -p $(EXTEND_TMP_DIR); \
	for l in $^; do \
		lib=$(call SHELL_TO_NATIVE,$(CURDIR))/$$l; \
		rm -rf $(EXTEND_TMP_DIR)2; \
		mkdir -p $(EXTEND_TMP_DIR)2; \
		pushd $(EXTEND_TMP_DIR)2 > /dev/null; \
		$(ABS_AR) x $$lib; \
		FILES=`echo *.o`; \
		if [ "$$FILES" == "*.o" ]; then \
			FILES=""; \
		fi; \
		popd > /dev/null; \
		for FILE in $$FILES; do \
			while [ -e "$(EXTEND_TMP_DIR)/$$FILE" ]; do \
				mv $(EXTEND_TMP_DIR)2/$$FILE $(EXTEND_TMP_DIR)2/_$$FILE; \
				FILE=_$$FILE; \
			done; \
			[ -e "$(EXTEND_TMP_DIR)2/$$FILE" ] && mv $(EXTEND_TMP_DIR)2/$$FILE $(EXTEND_TMP_DIR)/; \
		done; \
	done; \
	rm -rf $(EXTEND_TMP_DIR)2; \
	pushd $(EXTEND_TMP_DIR) > /dev/null; \
	$(ABS_AR) r tmp.a *.o ; \
	popd > /dev/null; \
	cp -p $(EXTEND_TMP_DIR)/tmp.a $@; \
	rm -rf $(EXTEND_TMP_DIR)

libextend: $(LIBEXTEND)

ALL_TARGETS+=$(LIBEXTEND)
all:$(LIBEXTEND)

EXTEND_FILES+=$(LIBEXTEND)


# Get list of $(EXTEND_SAMPLES)
include extend/samples/list_of_samples.mk

define extend_copy_sample

SAMPLE_DIR:=extend/samples/$(value 1)
FILES:=\
$$(SAMPLE_DIR)/Makefile \
$$(SAMPLE_DIR)/checker.mk \
$$(SAMPLE_DIR)/$(value 1).cpp \
$$(wildcard $$(SAMPLE_DIR)/test*/Makefile) \
$$(wildcard $$(SAMPLE_DIR)/test*/*.c*) \
$$(wildcard $$(SAMPLE_DIR)/test*/*.h*) \
$$(wildcard $$(SAMPLE_DIR)/test*/*.java) \
$$(wildcard $$(SAMPLE_DIR)/test*/*utput*) \

$$(foreach file,$$(FILES),$$(eval $$(call extend_setup_file_copy_2_dirs,extend/,,$$(patsubst extend/%,%,$$(file)))))

endef

$(foreach sample,$(EXTEND_SAMPLES),$(eval $(call extend_copy_sample,$(sample))))


# Get list of $(EXTEND_SOLUTIONS)
include extend/solutions/list_of_solutions.mk

define extend_copy_soln

SOLN_DIR:=extend/solutions/$(value 1)
FILE_NAME:=$(value 1)_test.cpp
SOLN_TEST_DIRS:=\
$$(wildcard $$(SOLN_DIR)/test*) \

$$(foreach soln_testdir,$$(SOLN_TEST_DIRS),$$(eval $$(call extend_setup_file_copy_2_dirs,$$(soln_testdir)/,tests/,$$(FILE_NAME))))

endef

#copy all solutions test files to extend/tests/
$(foreach soln,$(EXTEND_SOLUTIONS),$(eval $(call extend_copy_soln,$(soln))))

# copy the output comparison script into the Extend directory,
# and make it read-only to avoid accidental editing
extend/samples/compare-output-with-source.pl: utilities/compare-output-with-source.pl
	cp -f $^ $@
	chmod a-w $@

EXTEND_SAMPLE_MAKEFILES:=\
Makefile \
include.mk \
test.mk \
compare-output-with-source.pl \
list_of_samples.mk

$(foreach file,$(EXTEND_SAMPLE_MAKEFILES),$(eval $(call extend_setup_file_copy_2_dirs,extend/,,samples/$(file))))

# Ignore all the files in the extend/scripts directory, since they 
# appear to be for training and are not polished.
# I'm only going to ship the template_for_new_checkers.cpp.
$(eval $(call extend_setup_file_copy_2_dirs,extend/scripts/,samples/,template_for_new_checkers.cpp))

# DR 9224: Simple mechanism to detect changes in files that are
# reproduced in the documentation.  Do this only on linux64 to limit
# the issues with nonportable 'md5sum'.
ifeq (linux64,$(mc_platform))

EXTEND_COPIED_SAMPLES := sign sign2 sign3 print_types print_tree
EXTEND_COPIED_SAMPLES_FULL_NAMES := $(foreach name,$(EXTEND_COPIED_SAMPLES),extend/samples/$(name)/$(name).cpp)
$(OBJ_DIR)/gensrc/extend-samples-check-unmodified: $(EXTEND_COPIED_SAMPLES_FULL_NAMES) \
                                                   extend/copied-samples-checksum
	@if md5sum $(EXTEND_COPIED_SAMPLES_FULL_NAMES) | diff extend/copied-samples-checksum -; then \
	  echo ok > $@; \
	else \
	  echo "extend.mk: An example that is copied in the documentation has been modified."; \
	  echo "Please file a doc bug to get the change into the docs.  See bug 9224."; \
	  echo "Then modify extend/copied-samples-checksum correspondingly."; \
	  exit 2; \
	fi

all: $(OBJ_DIR)/gensrc/extend-samples-check-unmodified

endif # linux64


# Per Joel in Bug 10086: Use doc tarball for extend instead
#ifeq ($(DOC_REP_DIR),$(DOC_REP))
#DOCBOOK_FILES:=\
#extend_ug.html \
#extend_ug.pdf \
#cov.css \
#extend_ref.html \
#extend_ref.pdf \

#resources/extend_process.png \
#resources/extend_hello.jpg \
#resources/types.er.png \
#resources/patterns.er.png \
#resources/note.gif \
#resources/caution.gif \
#resources/warning.gif

#$(foreach file,$(DOCBOOK_FILES),$(eval $(call extend_setup_file_copy_2_dirs,$(DOC_REP)/output/,extend/docs/,$(file))))

#endif

# Another doc file
#$(eval $(call extend_setup_file_copy_2_dirs,extend/,extend/docs/,EXTEND_training.ppt))

# .PLATFORM_DIR is none other than the result of config.guess
$(EXTEND_ROOT_DIR)/.PLATFORM_DIR:
	echo $(CONFIG_GUESS) > $@

# win32 uses build-checker.bat, win64 uses build-checker-win64.bat
# everything else uses build-checker
ifeq (1, $(IS_WINDOWS))
BUILD_CHECKER_SCRIPT:=build-checker.bat
else
BUILD_CHECKER_SCRIPT:=build-checker
endif

$(eval $(call extend_setup_file_copy_2_dirs,extend/scripts/,,$$(BUILD_CHECKER_SCRIPT)))

ifeq (1,$(IS_WINDOWS))
FLEXNET_ARCHIVES_TO_COPY:=libcrvs.a libsb.a libnoact.a liblmgr_md.a libchkstk.a
ifeq ($(mc_platform), mingw64)
FLEXNET_ARCHIVES_TO_COPY:=libcrvs.a libsb.a libnoact.a liblmgr_md.a libchkstk.a \
	 libcov_import_symbols.a libcov_msvc_gs.a liblm_new.a liblmgr_dongle_stub.a
endif
$(foreach file,$(FLEXNET_ARCHIVES_TO_COPY),$(eval $(call extend_setup_file_copy_2_dirs,$(PLATFORM_PACKAGES_DIR)/lib/,libs/,$(file))))
endif

.PHONY: extend-files extend-compiler

# Build the extend compiler directory from the platform packages.

define extend_copy_compiler_file

SOURCE:=$$(PLATFORM_PACKAGES_DIR)/$(value 1)
TARGET:=$$(EXTEND_COMPILER_DIR)/$(value 1)

$$(TARGET): $$(SOURCE)
	@mkdir -p "$$(dir $$@)"
	@rm -rf "$$@"
	$(COVCP) -r "$$<" "$$@"

ALL_TARGETS+=$$(TARGET)
all:$$(TARGET)

EXTEND_FILES+=$$(TARGET)

endef

define extend_copy_compiler_file_wildcard

$$(foreach i,$$(wildcard $$(PLATFORM_PACKAGES_DIR)/$(value 1)), $$(eval $$(call extend_copy_compiler_file,$$(patsubst $$(PLATFORM_PACKAGES_DIR)/%,%,$$i))))

endef

COMPILER_FILES:=
COMPILER_FILES+= bin/g++$(DOT_EXE)
COMPILER_FILES+= bin/ld$(DOT_EXE)
COMPILER_FILES+= bin/as$(DOT_EXE)
COMPILER_FILES+= libexec
COMPILER_FILES+= lib/libz.a
COMPILER_FILES+= lib/libgmp.a
COMPILER_FILES+= lib/libnettle.a
COMPILER_FILES+= lib/gcc
COMPILER_FILES+= include
COMPILER_FILES+= lib/libboost*gcc.a
ifeq (solaris-x86,$(mc_platform))
# The compiler we have for solaris-x86 does not match the OS version on
# Solaris 11 but we still use it, hence this exception:
COMPILER_FILES+= i386-pc-solaris2.10
else ifeq (mingw,$(mc_platform))
COMPILER_FILES+=i686-w64-mingw32
else
COMPILER_FILES+= $(CONFIG_GUESS)
endif

# Directory where libstc++, libc, etc. are stored.
COMPILER_LIB_DIR:=lib

-include $(mc_platform)-compiler-files.mk

COMPILER_FILES+=$(COMPILER_LIB_DIR)/*.o
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libstdc++.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libc.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libm.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libgcc.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libgcc_eh.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libgcc_s.a
COMPILER_FILES+=$(COMPILER_LIB_DIR)/libiconv.a


$(foreach i,$(COMPILER_FILES), $(eval $(call extend_copy_compiler_file_wildcard,$i)))

# Remove "cc1" (the C compiler) from the distribution.
# We don't use it, and it's pretty big.
# Note: I don't like using "rm -f", but xargs -r is not portable.

# Although on HP/UX, somehow cc1 is necessary.

ifneq ($(mc_platform),hpux-pa)

extend-remove-cc1: $(EXTEND_COMPILER_DIR)/libexec
	find "$^" -name cc1 | xargs rm -f
extend-remove-files: extend-remove-cc1

endif

# Remove some includes that we add to out compiler "include" directory
# but that are not useful for Extend
extend-remove-extra-includes: $(EXTEND_COMPILER_DIR)/include
	rm -rf "$^"/{clang*,cyassl,llvm*}

extend-remove-files: extend-remove-extra-includes


# Remove precompiled headers, for the same reason.
extend-remove-pch: $(EXTEND_COMPILER_DIR)/include
	find "$^" -type d -name '*.gch' | xargs rm -rf

extend-remove-files: extend-remove-pch

# Remove symlinks. None of them are useful, and some are broken,
# confusing "tar".
extend-remove-links: $(EXTEND_FILES)
	find "$(EXTEND_COMPILER_DIR)" -type l | xargs rm -f

extend-remove-files: extend-remove-links

extend-files: $(EXTEND_FILES) extend-remove-files

all: extend-files

define build_extend_checker

# tested only on linux: setting this affects the build-checker script, coaxing
# it to link instrumented libraries.
ifeq ($(BUILD_TYPE),COVERAGE)
  export EXTRA_ARGS = -fprofile-arcs -ftest-coverage \
                      -L$$(top_srcdir)/$$(PLATFORM_PACKAGES_DIR)/lib64 \
                      -L$$(top_srcdir)/$$(PLATFORM_PACKAGES_DIR)/lib
endif
CHECKER_BIN:=$$(UNIT_TEST_BIN_DIR)/extend-$(value 1)$$(EXE_POSTFIX)

# build-checker sucks, the binary always goes to the same directory as
# the source
# Work around that using copies in a temp dir we can CD to
$$(CHECKER_BIN): EXTEND_TMP_DIR:=$(OBJ_DIR)/extend-tmp-dir-$(value 1)

# Copy headers, if any
CHECKER_HEADERS:=$$(wildcard $$(SOURCE_DIR)/*.h*)
$$(CHECKER_BIN): CHECKER_HEADERS:=$$(CHECKER_HEADERS)

$$(CHECKER_BIN): $$(SOURCE_DIR)/$(value 1).cpp $$(CHECKER_HEADERS) $$(EXTEND_FILES)
	mkdir -p $$(UNIT_TEST_BIN_DIR)
	rm -rf $$(EXTEND_TMP_DIR)
	mkdir -p $$(EXTEND_TMP_DIR)
	$(COVCP) $$< $$(CHECKER_HEADERS) $$(EXTEND_TMP_DIR)
	cd $$(EXTEND_TMP_DIR); \
	unset PREVENT_ROOT; \
    $$(call normalize_path,$$(EXTEND_ROOT_DIR)/$$(BUILD_CHECKER_SCRIPT)) $(value 1)
	mv $$(EXTEND_TMP_DIR)/$(value 1)$(EXE_POSTFIX) $$@
	rm -rf $$(EXTEND_TMP_DIR)

all: $$(CHECKER_BIN)

endef


# Should we build the Extend sample checkers?  (This logic has to be
# factored out into a flag because GNU make lacks AND and OR!)
SHOULD_BUILD_EXTEND_SAMPLES := 1

# Cannot build the Extend samples w/COVERAGE build type b/c libextend.a
# will have dependencies on (e.g.) __gcov_init, but the build-checker
# script does not know to use the right link flags.
ifeq ($(BUILD_TYPE),COVERAGE)
  SHOULD_BUILD_EXTEND_SAMPLES := 1
endif

# Bug 14861: Similarly, skip them when using debug heap.
ifeq ($(ENABLE_DEBUG_HEAP),1)
  SHOULD_BUILD_EXTEND_SAMPLES := 0
endif

ifeq ($(SHOULD_BUILD_EXTEND_SAMPLES),1)
include extend/tests/progs.mk
endif


endif # PLATFORM_HAS_EXTEND ... top of file

# EOF
