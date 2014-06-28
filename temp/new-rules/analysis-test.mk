
# 

# Must include test.mk beforehand
include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))test.mk

include $(RULES)/$(mc_platform)-flags.mk

ifeq (1,$(USE_HOST_COMPILER))
# REAL_CC and REAL_CXX are pre-set for Mac OS X 10.7,
# don't blow away these pre-set values here!
ifeq ($(REAL_CC),)
REAL_CC:=$(shell which gcc)
endif
ifeq ($(REAL_CXX),)
REAL_CXX:=$(shell which g++)
endif
else
REAL_CC:=$(PREVENT_SRC_ROOT)/$(mc_platform)-packages/bin/gcc
REAL_CXX:=$(PREVENT_SRC_ROOT)/$(mc_platform)-packages/bin/g++
endif

# Several tests need a compiler that can actually create binaries.
# The "platform" gcc (gcc) can always do that (not necessarily the "packages" gcc, e.g. on Linux it requires special link flags)
# However on cygwin it's unbearably slow. But in that case the
# packages gcc is acceptable.
ifeq (1,$(USING_CYGWIN))
FAST_CC:=$(REAL_CC)
FAST_CXX:=$(REAL_CXX)
else
FAST_CC:=$(shell which gcc)
FAST_CXX:=$(shell which g++)
endif
# This is often used in a sub-makefile e.g. "$(MAKE) -C src", so export.
export FAST_CC
export FAST_CXX

# always try --use-fast-jre unless specified for non-slow-testsuite
ifneq (1,$(TESTSUITE_EMIT_CORE_LIBRARIES_BY_DEFAULT))
ifeq (0,$(USE_FAST_JRE))
    USE_FAST_JRE_OPT:=
else
    USE_FAST_JRE_OPT:=--use-fast-jre
endif
else
    USE_FAST_JRE_OPT:=
endif

# If EMIT_BOOTCLASSPATH is set in the test, do
# not use --skip-emit-bootclasspath.
# Otherwise, use --skip-emit-bootclasspath.
# Must be set before this file is processed.
ifeq (0,$(EMIT_BOOTCLASSPATH))
	SKIP_EMIT_BOOTCLASSPATH:=--skip-emit-bootclasspath
else ifeq (1,$(EMIT_BOOTCLASSPATH))
	SKIP_EMIT_BOOTCLASSPATH:=
# ahurst 1/16/13: Attempted to allow the default
# to by changed with an environment variable, but this
# negatively impacted many non-checker tests.  Disabled.
#else ifeq (1,$(TESTSUITE_EMIT_CORE_LIBRARIES_BY_DEFAULT))
#	SKIP_EMIT_BOOTCLASSPATH:=
else
	SKIP_EMIT_BOOTCLASSPATH:=--skip-emit-bootclasspath
endif

# For tests that absolutely require skipping the bootclasspath,
# this command line option can be used to override the env var
ifeq (,$(SKIP_EMIT_BOOTCLASSPATH))
	FORCE_SKIP_EMIT_BOOTCLASSPATH:=--skip-emit-bootclasspath
else
	FORCE_SKIP_EMIT_BOOTCLASSPATH:=
endif

# We want to use --skip-war-sanity-check with cov-emit-java almost
# everywhere, so include it unless WAR_SANITY_CHECK=1 (must be set
# before this file is processed)
ifneq (1,$(WAR_SANITY_CHECK))
	SKIP_WAR_SANITY_CHECK:=--skip-war-sanity-check
endif

ifeq (1,$(GENERATE_COMPILER_OUTPUTS))
	NO_COMPILER_OUTPUTS:=--generate-compiler-outputs $(REAL_JAVAC)
	NO_OUT_CS:=--generate-compiler-outputs-not-yet-supported-for-cs
else
ifneq (1,$(REQUIRE_COMPILER_OUTPUTS))
	NO_COMPILER_OUTPUTS:=--no-compiler-outputs
	NO_OUT_CS:=--no-out
else
	NO_COMPILER_OUTPUTS:=
	NO_OUT_CS:=
endif
endif

# Pass --no-restart unless told otherwise.
ifeq (1,$(COVERITY_ENABLE_CEJ_WATCHDOG))
	CEJ_NO_RESTART := --enable-java-parse-error-recovery
else
	CEJ_NO_RESTART ?= --no-restart # don't override if already set
endif

COV_TRANSLATE:=$(COV_BIN_DIR)/cov-translate
COV_ANALYZE:=$(COV_BIN_DIR)/cov-analyze
COV_MANAGE_EMIT:=$(COV_BIN_DIR)/cov-manage-emit
COV_EMIT:=$(COV_BIN_DIR)/cov-emit
COV_EMIT_CLANG:=$(COV_BIN_DIR)/cov-internal-emit-clang
COV_RUN_DESKTOP:=$(COV_BIN_DIR)/cov-run-desktop

# Run cov-manage-emit list; use case-preserved filenames for
# consistency across platforms (this is the default although it didn't
# use to be)
# use $(call LIST_TUS, opts)
# e.g.
# $(call LIST_TUS,-tp 'has_code()')
LIST_TUS=$(COV_MANAGE_EMIT) \
	--dir $(IDIR) $(1) list


ifeq ($(NO_JAVA),)

COV_ANALYZE_JAVA:=$(COV_BIN_DIR)/cov-analyze-java
CEJ_BIN:=$(COV_BIN_DIR)/cov-emit-java

COV_EMIT_JAVA_PERMISSIVE:=$(CEJ_BIN) $(NO_COMPILER_OUTPUTS) $(SKIP_EMIT_BOOTCLASSPATH) $(SKIP_WAR_SANITY_CHECK) $(CEJ_NO_RESTART) --disable-workaround per_method_error_recovery $(USE_FAST_JRE_OPT)
COV_EMIT_JAVA:=$(CEJ_BIN) $(NO_COMPILER_OUTPUTS) $(SKIP_EMIT_BOOTCLASSPATH) $(SKIP_WAR_SANITY_CHECK) $(CEJ_NO_RESTART) $(USE_FAST_JRE_OPT)
endif

ifeq ($(NO_CSHARP),)

CS_LIB_VERSION:=4.0
USE_GENERICS:=1

# Note: utilities/run-testsuite/analysis-tool.cpp replicates this logic
# to compile C# DLLs for "EMIT_AS_BYTECODE".
ifneq ($(filter mingw mingw64,$(mc_platform)),)
    CSHARP_BIN_DIR:=$(PREVENT_BIN_DIR)
    CSC:=$(shell ls /cygdrive/c/Windows/Microsoft.NET/Framework/v{3,4}*/csc.exe | sort | tail -1)
    ILASM:=$(shell ls /cygdrive/c/Windows/Microsoft.NET/Framework/v{3,4}*/ilasm.exe 2>/dev/null | sort | tail -1)
    MSCORLIB:=$(call SHELL_TO_NATIVE,$(wildcard /cygdrive/c/WINDOWS/Microsoft.NET/Framework/v$(CS_LIB_VERSION).*/mscorlib.dll))
else
ifneq ($(filter linux64,$(mc_platform)),)
    CSHARP_BIN_DIR:=$(TEST_BIN_DIR)
    # This appears to only be used in tests, so I'm following the example of
    # the JAVAC make variable, and just pointing to the one in platform
    # packages rather than copying this to TEST_BIN_DIR.
    ifeq ($(USE_GENERICS),1)
    CSC_PATH:=$(PLATFORM_PACKAGES_DIR)/mono/bin/gmcs
    CSC:=$(CSC_PATH) -codepage:65001
    else
    CSC_PATH:=$(PLATFORM_PACKAGES_DIR)/mono/bin/mcs
    CSC:=$(CSC_PATH) -codepage:65001
    endif
    ILASM:=$(PLATFORM_PACKAGES_DIR)/mono/bin/ilasm -codepage:65001
    MSCORLIB:=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/mono/lib/mono/$(CS_LIB_VERSION)/mscorlib.dll
endif

endif

# If EMIT_SYSTEM_REFERENCES is set in the test, do
# not use --skip-emit-system-references -noconfig.
# Otherwise, use --skip-emit-system-references.
# Must be set before this file is processed.
ifeq (1,$(EMIT_SYSTEM_REFERENCES))
	SKIP_EMIT_SYSTEM_REFERENCES:=
else
	SKIP_EMIT_SYSTEM_REFERENCES:=--skip-emit-system-references --noconfig
endif

# Ditto for EMIT_BYTECODE_BODIES
ifeq (1,$(EMIT_BYTECODE_BODIES))
	SKIP_EMIT_BYTECODE_BODIES:=
else
	SKIP_EMIT_BYTECODE_BODIES:=--decompilation=metadata
endif

ifeq (1,$(SILENCE_XREF_PROBLEM_WARNINGS))
	EXPOSE_XREF_PROBLEMS:=
else
	EXPOSE_XREF_PROBLEMS:=--expose-xref-problems
endif

ifeq (1,$(METHOD_LEVEL_CS_RECOVERY))
    DISABLE_PERMISSIVE_ERROR_RECOVERY:=
else
    DISABLE_PERMISSIVE_ERROR_RECOVERY:=--disable-permissive-error-recovery
endif

COV_ANALYZE_CSHARP:=$(COV_BIN_DIR)/cov-analyze
CES_BIN:=$(CSHARP_BIN_DIR)/cov-emit-cs

COV_EMIT_CSHARP:=$(CES_BIN) $(NO_OUT_CS) $(SKIP_EMIT_SYSTEM_REFERENCES) $(SKIP_EMIT_BYTECODE_BODIES) $(EXPOSE_XREF_PROBLEMS) $(DISABLE_PERMISSIVE_ERROR_RECOVERY)
endif

COV_CONFIGURE:=$(COV_BIN_DIR)/cov-configure
COV_BUILD:=$(COV_BIN_DIR)/cov-build
COV_REDUCE:=$(COV_BIN_DIR)/cov-internal-reduce
COV_COLLECT_MODELS:=$(COV_BIN_DIR)/cov-collect-models

.PHONY: compile-source-c compile-source-cxx compile-source-gcc

compile-source-c:
	$(COV_EMIT) --no_error_recovery --dir $(IDIR) --c -w $(EMITOPTS) $(TESTFILE)

help::
	@echo "compile-source-c: Emits a C source file to \$$(IDIR)"
	@echo "  EMITOPTS = extra options to cov-emit"
	@echo "  TESTFILE = file to compile"
	@echo

# Compiling C is always "available", but this is for symmetry with "compile-java-if-available", etc.
compile-source-c-if-available: compile-source-c

help::
	@echo "compile-source-c-if-available: compile-source-c if C is supported on this platform, otherwise no-op"
	@echo

compile-source-cxx:
	$(COV_EMIT) --no_error_recovery --dir $(IDIR) --c++ -w $(EMITOPTS) $(TESTFILE)

help::
	@echo "compile-source-cxx: Like compile-source-c for C++"
	@echo

compile-source-cxx-if-available: compile-source-cxx

help::
	@echo "compile-source-cxx-if-available: compile-source-cxx if C++ is supported on this platform, otherwise no-op"
	@echo

# On MacOSX, "preprocess-first" is the default for gcc.
# This is not necessary in our case, and strips macro info, so don't
# do it.
NO_PREPROCESS_FIRST?=--no-preprocess-first

# Use "=", not ":=", so that GCC_OPTS and TRANSLATEOPTS can be
# modified later.
TRANSLATE_GCC=\
	$(COV_TRANSLATE) $(NO_PREPROCESS_FIRST) \
	--fail-stop --add-arg --no_error_recovery $(TRANSLATEOPTS) \
	--dir $(IDIR) -c $(GCC_CONFIG) \
	$(REAL_CC) -c $(GCC_OPTS) \
	-I $(PREVENT_SRC_ROOT)/$(mc_platform)-packages/include

compile-source-gcc:
	$(TRANSLATE_GCC) $(TESTFILE)

help::
	@echo "compile-source-gcc: Compile C/C++ sources using cov-translate on gcc"
	@echo "  TRANSLATEOPTS = extra options to cov-translate"
	@echo "  GCC_OPTS = extra options to gcc"
	@echo "  TESTFILE = file(s) to compile"
	@echo

compile-source-gcc-if-available: compile-source-gcc

help::
	@echo "compile-source-gcc-if-available: compile-source-gcc if C/C++ is supported on this platform, otherwise no-op"
	@echo

help::
	@echo "compile-source-java: Emits a Java source file to \$$(IDIR)"
	@echo "  EMITOPTS_JAVA = extra options to cov-emit-java"
	@echo "  TESTFILE_JAVA = file to compile"
	@echo "  EMIT_BOOTCLASSPATH: If set to 1 before including this file, then --skip-emit-bootclasspath will *not* be used"
	@echo "  REQUIRE_COMPILER_OUTPUTS: If set to 1 before including this file, then --no-out will *not* be used"
	@echo

help::
	@echo "compile-source-java-permissive: Same as compile-source-java without --fail-on-recoverable-error"
	@echo

help::
	@echo "compile-source-java-if-available: compile-source-java if Java is supported on this platform, otherwise no-op"
	@echo

ifeq ($(NO_JAVA),)

PERMISSIVE_JAVA_OPTS=--dir $(IDIR) $(EMITOPTS_JAVA)

compile-source-java:
	$(COV_EMIT_JAVA) --fail-on-recoverable-error $(PERMISSIVE_JAVA_OPTS) $(TESTFILE_JAVA)

compile-source-java-if-available: compile-source-java

# In the common case, this is the same thing as compile-source-java. However,
# in some cases we redefine COV_EMIT_JAVA to force usage of
# --fail-on-recoverable error. This is to circumvent that.
compile-source-java-permissive:
	$(COV_EMIT_JAVA_PERMISSIVE) $(PERMISSIVE_JAVA_OPTS) $(TESTFILE_JAVA)

compile-source-java-per-method-error-recovery:
	$(COV_EMIT_JAVA_PERMISSIVE) $(PERMISSIVE_JAVA_OPTS) --enable-workaround per_method_error_recovery $(TESTFILE_JAVA)

compile-source-java-suppress-errors:
	-$(COV_EMIT_JAVA_PERMISSIVE) $(PERMISSIVE_JAVA_OPTS)

# Compile a .jar file from the sources in "$(JAR_SOURCES)" (which may
# include wildcards) into $(JAR_FILE)
compile-jar:
	@mkdir -p $(TESTSUITE_DIR)/jar-classes
	$(JAVAC) -nowarn -d $(TESTSUITE_DIR)/jar-classes -encoding UTF-8 \
		$(JAR_COMPILE_OPTS) $(JAR_SOURCES)
	$(JAR) cf $(JAR_FILE) -C $(TESTSUITE_DIR)/jar-classes .

else # NO_JAVA

compile-source-java-if-available:
	echo "Not compiling Java file"

endif # !NO_JAVA

ifeq ($(NO_CSHARP),)


compile-source-cs:
	$(COV_EMIT_CSHARP) --dir $(IDIR) $(EMITOPTS_CSHARP) $(TESTFILE_CSHARP)

compile-source-cs-if-available: compile-source-cs

# Compile a .dll assembly file from the sources in
# "$(ASSEMBLY_SOURCES)" (which may include wildcards) into $(ASSEMBLY)
compile-assembly:
	$(CSC) /target:library /out:$(ASSEMBLY) $(ASSEMBLY_COMPILE_OPTS) $(ASSEMBLY_SOURCES)

else

compile-source-cs-if-available:
	echo "Not compiling C# file"

endif # !NO_CSHARP

.PHONY: run-test run-test-cxx run-test-gcc
.PHONY: run-test-checker run-test-cxx-checker run-test-gcc-checker


run-test: compile-source-c analyze

run-test-cxx: compile-source-cxx analyze

run-test-gcc: compile-source-gcc analyze

ifeq ($(NO_JAVA),)

run-test-java: compile-source-java analyze-java

run-test-java-permissive: compile-source-java-permissive analyze-java

run-test-java-per-method-error-recovery: \
  compile-source-java-per-method-error-recovery analyze-java

endif # !NO_JAVA

run-test-checker: compile-source-c analyze-checker

run-test-cxx-checker: compile-source-cxx analyze-checker

run-test-gcc-checker: compile-source-gcc analyze-checker

ifeq ($(NO_JAVA),)

run-test-java-checker: compile-source-java analyze-checker-java

endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

run-test-cs: compile-source-cs analyze-cs

run-test-cs-checker: compile-source-cs analyze-checker-cs

endif # !NO_JAVA

# args for cov-analyze
SANITY_CHECKS ?= --sanity-checks

.PHONY: analyze analyze-checker check-annotations
.PHONY: analyze-java analyze-checker-java check-annotations-java
.PHONY: analyze-cs analyze-checker-cs check-annotations-cs

analyze:
	$(COV_ANALYZE) $(SANITY_CHECKS) --dir $(IDIR) \
	$(TESTOPTS)

ifeq ($(NO_JAVA),)

analyze-java:
	$(COV_ANALYZE_JAVA) $(SANITY_CHECKS) --dir $(IDIR) \
	--disable-fb $(TESTOPTS)

analyze-java-with-findbugs:
	$(COV_ANALYZE_JAVA) $(SANITY_CHECKS) --dir $(IDIR) \
	--enable-fb $(TESTOPTS)

endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

analyze-cs:
	$(COV_ANALYZE_CSHARP) $(SANITY_CHECKS) --dir $(IDIR) \
	$(TESTOPTS)

endif # !NO_CSHARP

analyze-checker:
	$(COV_ANALYZE) $(SANITY_CHECKS) --dir $(IDIR) \
	--disable-default --enable $(CHECKER) $(TESTOPTS)

ifeq ($(NO_JAVA),)

analyze-checker-java:
	$(COV_ANALYZE_JAVA) $(SANITY_CHECKS) --dir $(IDIR) \
	--disable-default --disable-fb --enable $(CHECKER) $(TESTOPTS)

endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

analyze-checker-cs:
	$(COV_ANALYZE_CSHARP) $(SANITY_CHECKS) --dir $(IDIR) \
	--disable-default --enable $(CHECKER) $(TESTOPTS)

endif # !NO_CSHARP

CHECK_ANNOTATIONS:=$(TEST_BIN_DIR)/check-annotations

ifneq ($(GENERATE_EXPECTED_OUTPUT),)
CHECK_ANNOTATIONS:=$(CHECK_ANNOTATIONS) --commit
endif

check-annotations:
	$(CHECK_ANNOTATIONS) --dir $(IDIR) $(CHECK_ANNOT_OPTS)

check-annotations-java: check-annotations
check-annotations-cs: check-annotations

.PHONY: collect-models

collect-models:
	$(COV_COLLECT_MODELS) --dir $(IDIR) --text -of $(RAW_OUTPUT)

.PHONY: model-testsuite model-annot-testsuite

model-testsuite: run-test-checker collect-models process-raw-output compare-output

model-annot-testsuite: model-testsuite

.PHONY: compare-models
.PHONY: compare-models-cs

ifeq ($(NO_JAVA),)

# compares models generated from source versus models from bytecode
# - disable SWAPPED_ARGUMENTS because it relies on source for parameter names.
# - disable HIBERNATE_BAD_HASHCODE, because the read deriver is disabled on
#   bytecode (see comments there).
# - disable webapp security because it does more aggressive
#   bottom-up analysis for bytecode
# - disable RELAX_BENIGN_CLOSURE because bytecode can have generated
#   constructs that are not considered benign (for example, benign
#   source "stringValue + 0" can include instantiations of StringBuffers
#   in bytecode. These differences are irritating but I don't think they'll
#   cause us too much trouble for the benign closure heuristic.
compare-models: CHECKERS:=--all \
                          --disable-webapp-security \
                          --disable SWAPPED_ARGUMENTS \
                          --disable HIBERNATE_BAD_HASHCODE \
                          --disable RELAX_BENIGN_CLOSURE \

compare-models:
	# create models from source
	$(COV_EMIT_JAVA) --dir $(IDIR)1 $(EMITOPTS_JAVA) \
		$(TESTFILE_JAVA)
	$(COV_ANALYZE_JAVA) --dir $(IDIR)1 $(CHECKERS) --disable-da $(TESTOPTS)
	$(COV_COLLECT_MODELS) --java --dir $(IDIR)1 \
		--text -of $(TESTSUITE_DIR)/models1.xml
	# create models from bytecode
	mkdir -p $(TESTSUITE_DIR)/classes
	echo "package ignoreme;" > $(TESTSUITE_DIR)/IgnoreMe.java
	$(JAVAC) -d $(TESTSUITE_DIR)/classes $(JAVACOPTS) \
		$(TESTFILE_JAVA)
	$(JAR) cf $(TESTSUITE_DIR)/classes.jar -C $(TESTSUITE_DIR)/classes .
	mkdir -p $(IDIR)2
	$(COV_EMIT_JAVA) --dir $(IDIR)2 \
		--enable-decomp --enable-decomp-bodies \
		--classpath $(TESTSUITE_DIR)/classes.jar \
		$(TESTSUITE_DIR)/IgnoreMe.java
	$(COV_ANALYZE_JAVA) --dir $(IDIR)2 --disable-rta --disable-da $(CHECKERS) \
		$(TESTOPTS)
	$(COV_COLLECT_MODELS) --java --dir $(IDIR)2 \
		--text -of $(TESTSUITE_DIR)/models2.xml
	# compare models
	$(TEST_BIN_DIR)/check-isomorphic -i -v \
		$(TESTSUITE_DIR)/models1.xml $(TESTSUITE_DIR)/models2.xml

endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

compare-models-cs: CHECKERS:=--all \
                          --disable-webapp-security \
                          --disable SWAPPED_ARGUMENTS

create-models-from-source-cs:
	rm -rf $(IDIR)-from-source
	$(COV_EMIT_CSHARP) --dir $(IDIR)-from-source $(EMITOPTS_CSHARP) \
		--decompilation=full --disable-indirect-references --noconfig $(TESTFILE_CSHARP)
	$(COV_ANALYZE_CSHARP) --dir $(IDIR)-from-source $(CHECKERS) --disable-rta --disable-da $(TESTOPTS)
	rm -rf $(TESTSUITE_DIR)/models-from-source.xml
	$(COV_COLLECT_MODELS) --cs --dir $(IDIR)-from-source \
		--text -of $(TESTSUITE_DIR)/models-from-source.xml

create-models-from-bytecode-cs:
	rm -rf $(TESTSUITE_DIR)/A.dll
	$(CSC) /define:DEBUG /target:library /unsafe /noconfig $(CSC_OPTIONS) /out:$(TESTSUITE_DIR)/A.dll $(TESTFILE_CSHARP)
	@ # If we have 0 TUs, then we won't get bytecode.
	echo "static class IgnoreMe{}" > $(TESTSUITE_DIR)/IgnoreMe.cs
	rm -rf $(IDIR)-from-bytecode
	$(COV_EMIT_CSHARP) --dir $(IDIR)-from-bytecode \
		--decompilation=full --disable-indirect-references --noconfig --reference $(TESTSUITE_DIR)/A.dll  $(TESTSUITE_DIR)/IgnoreMe.cs
	$(COV_ANALYZE_CSHARP) --dir $(IDIR)-from-bytecode $(CHECKERS) --disable-rta --disable-da $(TESTOPTS)
	rm -rf $(TESTSUITE_DIR)/models-from-bytecode.xml
	$(COV_COLLECT_MODELS) --cs --dir $(IDIR)-from-bytecode \
		--text -of $(TESTSUITE_DIR)/models-from-bytecode.xml

compare-models-cs: create-models-from-source-cs create-models-from-bytecode-cs
	$(TEST_BIN_DIR)/check-isomorphic -i -v \
		$(TESTSUITE_DIR)/models-from-source.xml $(TESTSUITE_DIR)/models-from-bytecode.xml

endif # !NO_CSHARP

EXTRA_REGEX+= \
--strip md5 \
--strip score \
--strip-regex="<!--" --strip-regex="-->" \
 --replace-path '$(COV_EMIT)/cov-emit%$$cov-emit$$' \
 --replace-path '$(REAL_CC)%$$REAL_CC$$' \
 --replace-path '$(REAL_CXX)%$$REAL_CXX$$' \

# Remove model key, for backwards compatibility, and because it's not
# relevant to check
EXTRA_REGEX+= \
--strip-regex '<key>[0-9a-f]*</key>'

# cat the analysis log file, e.g., for grepping it
CAT_ANALYSIS_LOG := cat $(IDIR)/output/analysis-log.txt
# similar to the above, except without a Useless Use of Cat
# (our testsuite is slow enough!)
ANALYSIS_LOG := $(IDIR)/output/analysis-log.txt

DUMP_TU?= -tu 1

# Dump the emit as JSON.
dump-emit:
	$(COV_MANAGE_EMIT) \
	  --dir $(IDIR) \
	  $(DUMP_TU) dump \
	  > $(RAW_OUTPUT)

ifeq ($(NO_JAVA),)

# ...
CAT_JAVA_ANALYSIS_LOG := cat $(IDIR)/output/analysis-log.txt

# Dump the emit as JSON.
dump-java-emit:
	$(COV_MANAGE_EMIT) \
	  --java \
	  --dir $(IDIR) \
	  $(DUMP_TU) dump \
	  > $(RAW_OUTPUT)

# Omit source code.
dump-java-emit-nosource:
	$(COV_MANAGE_EMIT) \
	  --java \
	  --dir $(IDIR) \
	  $(DUMP_TU) dump \
	  --hide file_contents \
	  > $(RAW_OUTPUT)

# Dump the xrefs in the emit in a diffable format
dump-xrefs-java-diffable:
	$(COV_MANAGE_EMIT) \
	--dir $(IDIR) \
        --java \
	dump-all-xrefs --print-diffable --basename-only > $(CURRENT_OUTPUT)


# Sanity check the xrefs
sanity-check-xrefs-java:
	$(COV_MANAGE_EMIT) \
	--dir $(IDIR) \
	--java \
	sanity-check-xrefs


endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

# ...
CAT_CS_ANALYSIS_LOG := cat $(IDIR)/output/analysis-log.txt

# Dump the emit as JSON.
dump-cs-emit:
	$(COV_MANAGE_EMIT) \
	  --cs \
	  --dir $(IDIR) \
	  $(DUMP_TU) dump \
	  > $(RAW_OUTPUT)

dump-xrefs-cs-diffable:
	$(COV_MANAGE_EMIT) \
	--dir $(IDIR) \
        --cs \
	dump-all-xrefs --print-diffable --basename-only > $(CURRENT_OUTPUT)

sanity-check-xrefs-cs:
	$(COV_MANAGE_EMIT) \
	--dir $(IDIR) \
	--cs \
	sanity-check-xrefs

endif # !NO_CSHARP

# With some filtering, we might be able to make this kind of test
# less brittle.  As it is, it should only be used when the print-file
# tests do not expose a necessary facet of the AST.
# Arguments:
# 1 = language (cpp,java,csharp)
# 2 = idir
# 3 = arguments to "find" (including regex)
# PRINT_OPTS = more arguments to "find"
PRINT_DEBUG_AST=\
	($(COV_MANAGE_EMIT) --$(1) \
	--dir $(2) \
	find --print-debug $(PRINT_OPTS) $(3) \
		| grep -v 'defined in TU' \
		| sed -e 's/deepID = [0-9]*/deepID = NNN/g' \
			  -e 's/ \(size\) = [0-9]*/ \1 = <value>/g' \
			  -e 's/ \(alignment\) = [0-9]*/ \1 = <value>/g' \
		; exit $${PIPESTATUS[0]})

print-debug-ast:
	$(call PRINT_DEBUG_AST,cpp,$(IDIR),'.*') > $(RAW_OUTPUT)

ifeq ($(NO_JAVA),)
print-debug-ast-java:
	$(call PRINT_DEBUG_AST,java,$(IDIR),'.*') > $(RAW_OUTPUT)
endif

ifeq ($(NO_CSHARP),)
print-debug-ast-cs:
	$(call PRINT_DEBUG_AST,cs,$(IDIR),'.*') > $(RAW_OUTPUT)
endif

# EOF
