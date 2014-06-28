# 

# Paths used by the build system, relative to the root of the
# repository (They may also be absolute if needed, e.g. obj dir)
#
# If you need to define a path macro that's visible to both
# the build system AND tests, then put it in paths-common.mk instead.

ifeq (,$(PATHS_MK_INCLUDED))
PATHS_MK_INCLUDED:=1

RULES:=$(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

# mc_platform, top_srcdir
include $(RULES)/config.mk

# Set any platform specific PATHs required
ifeq ($(mc_platform),hpux-ia64)
PATH:=/opt/hp-gcc-4.1.2/bin:$(PATH)
export PATH
endif

# Object directory
# Allow setting on command line...
ifeq (,$(OBJ_DIR))
OBJ_DIR:=objs/$(mc_platform)
endif

# general-purpose libraries directory
LIB_DIR:=libs

# Analysis library directory
ANALYSIS_DIR:=analysis

# Utilities directory
UTILITIES_DIR:=utilities

# GUI directory
GUI_DIR:=gui

# "Release" (default) root directory
RELEASE_ROOT_DIR:=$(OBJ_DIR)/root

# By default (e.g. build), directories are relative (nicer for a
# variety of reasons, one of them being they're identical on cygwin
# and native Windows)
# But tests run from a subdirectory and need an absolute path. To do
# that, set ABSOLUTE_RELEASE_DIR to 1.
ifeq ($(ABSOLUTE_RELEASE_DIR),1)
RELEASE_ROOT_DIR:=$(native_top_srcdir)/$(RELEASE_ROOT_DIR)
endif

# "Release" (default) binary directory
RELEASE_BIN_DIR:=$(RELEASE_ROOT_DIR)/bin

# "Release" (default) jars directory
RELEASE_JARS_DIR:=$(RELEASE_ROOT_DIR)/jars

# "Release" (default) docs directory
RELEASE_DOC_DIR:=$(RELEASE_ROOT_DIR)/doc

# "Release" model sources/library directory
RELEASE_LIB_DIR:=$(RELEASE_ROOT_DIR)/library

# "Test" binary directory (for other binaries, e.g. unit tests)
#
# XXX: unit test binaries shouldn't really go in here, since they
# are packaged in a "QA" tarball which is really only intended for
# running end-to-end tests and/or debugging.
TEST_BIN_DIR:=$(RELEASE_ROOT_DIR)/test-bin

# Fake compilers binary directory
FAKE_COMPILERS_BIN_DIR:=$(TEST_BIN_DIR)/fake-compilers

# Binaries which are needed for building the product and/or unit testing,
# but which are otherwise useless afterwards.
#
# This happens to be a subdirectory of the "release" directory for
# convenience, since binaries built with the "script" library generally depend
# on being somewhere here.  Hopefully the "unit-test" part of the name will
# discourage everyone from archiving these...
UNIT_TEST_BIN_DIR:=$(RELEASE_ROOT_DIR)/unit-test-bin

# Binary directory for libraries
LIB_BIN_DIR:=$(OBJ_DIR)/libs

# Name of the binary created when compiling a library.
# (function of the library name)
LIB_BIN_NAME=$(LIB_BIN_DIR)/lib$(1).a
CSHARP_LIB_BIN_NAME=$(CSHARP_BIN_DIR)/$(1).dll

# Directory in which to place generated source files.
GENSRC_DIR:=$(OBJ_DIR)/gensrc

# Directory for Java class files.
CLASSES_DIR := $(OBJ_DIR)/classes

# The prefix for temp files that store the arguments to the link commands for all the
# executables built with create-prog.mk.
LINK_COMMAND_LOG_PREFIX := $(OBJ_DIR)/objs/link_command

# Directory for the cvs repository of platform-specific binaries
PLATFORM_PACKAGES_DIR:=$(mc_platform)-packages

# mb (2011/11/18): we are beginning to put platform-agnostic (e.g. Java)
# binaries into packages
PACKAGES_DIR:=packages

# Directories for distributed JREs (with versions)
JRE7_DIR:=$(RELEASE_ROOT_DIR)/jre7
JRE8_DIR:=$(RELEASE_ROOT_DIR)/jre8

# Coverity Tools
COV_ANALYZE:=$(RELEASE_BIN_DIR)/cov-analyze$(EXE_POSTFIX)
COV_MANAGE_EMIT:=$(RELEASE_BIN_DIR)/cov-manage-emit$(EXE_POSTFIX)
COV_MAKE_LIBRARY:=$(RELEASE_BIN_DIR)/cov-make-library$(EXE_POSTFIX)
COV_EMIT:=$(RELEASE_BIN_DIR)/cov-emit$(EXE_POSTFIX)
COV_TRANSLATE:=$(RELEASE_BIN_DIR)/cov-translate$(EXE_POSTFIX)
COV_CONFIGURE:=$(RELEASE_BIN_DIR)/cov-configure$(EXE_POSTFIX)
COV_COLLECT_MODELS:=$(RELEASE_BIN_DIR)/cov-collect-models$(EXE_POSTFIX)
COV_BUILD:=$(RELEASE_BIN_DIR)/cov-build$(EXE_POSTFIX)
DISTRIBUTOR_BINS:=$(RELEASE_BIN_DIR)/cov-internal-dm$(EXE_POSTFIX) $(RELEASE_BIN_DIR)/cov-internal-dw$(EXE_POSTFIX)
ASTGEN:=$(TEST_BIN_DIR)/astgen$(EXE_POSTFIX)
BISON:=$(TEST_BIN_DIR)/bison$(EXE_POSTFIX)
FLEX:=$(TEST_BIN_DIR)/flex$(EXE_POSTFIX)

# Behavior of tools used in build & test should not depend on this
# setting the user might have.  (Bug 48949)
unexport COVERITY_DETECT_LANGUAGE

# Now define some macros common to both paths.mk and test.mk
include $(RULES)/paths-common.mk

ifeq ($(NO_JAVA),)
COV_EMIT_JAVA := $(RELEASE_BIN_DIR)/cov-emit-java$(EXE_POSTFIX)
COV_ANALYZE_JAVA:=$(RELEASE_BIN_DIR)/cov-analyze-java$(EXE_POSTFIX)
COV_INTERNAL_EMIT_JAVA_BYTECODE := $(RELEASE_BIN_DIR)/cov-internal-emit-java-bytecode$(EXE_POSTFIX)

# Useful as a dependency for models
JAVA_PRIMITIVES:=$(RELEASE_ROOT_DIR)/library/primitives.jar
JAVA_ANNOTATIONS:=$(RELEASE_ROOT_DIR)/library/annotations.jar
JAVA_PRIMITIVES_JAVADOC_DIR:=$(RELEASE_DOC_DIR)/en/primitives
JAVA_ANNOTATIONS_JAVADOC_DIR:=$(RELEASE_DOC_DIR)/en/annotations
JAVA_RT_JAR_IDIR_DIR:=$(RELEASE_ROOT_DIR)/test-data/rt-jar-idir
# That's where we store a pre-populated emit directory for "rt.jar",
# which cov-emit-java can use instead of emitting rt.jar every time.
JAVA_RT_JAR_IDIR_TGZ:=$(JAVA_RT_JAR_IDIR_DIR).tgz
#this file will be unnecessary once --no-source works.
JAVA_RT_JAR_IDIR_SRC_FILE:=$(RELEASE_ROOT_DIR)/test-data/IgnoreMe.java

FAST_JRE_JAR_IDIR_DIR:=$(RELEASE_ROOT_DIR)/test-data/fast-rt-jar-idir
FAST_JRE_JAR_IDIR_TGZ:=$(FAST_JRE_JAR_IDIR_DIR).tgz
FAST_JRE_JAR_IDIR_SRC_FILE:=$(RELEASE_ROOT_DIR)/test-data/FastIgnoreMe.java
endif # !NO_JAVA

ifeq ($(NO_CSHARP),)

# This is still used by the utilities/cov-manage-emit/test-csharp-interp test.
# This should be refactored away as part of the C# unification project.
PROTO_COV_EMIT_CSHARP:=csharp-frontend/proto-cov-emit-cs/bin/Debug/proto-cov-emit-cs$(EXE_POSTFIX)

# Useful as a dependency for models
CSHARP_PRIMITIVES:=$(RELEASE_ROOT_DIR)/library/primitives.dll

CSHARP_MSCORLIB_DLL_IDIR_DIR:=$(RELEASE_ROOT_DIR)/test-data/mscorlib-dll-idir
# That's where we store a pre-populated emit directory for "mscorlib.dll",
# which cov-emit-cs can use instead of emitting mscorlib.dll every time.
CSHARP_MSCORLIB_DLL_IDIR_TGZ:=$(CSHARP_MSCORLIB_DLL_IDIR_DIR).tgz

CSHARP_MSCORLIB_DLL_BODIES_IDIR_DIR:=$(RELEASE_ROOT_DIR)/test-data/mscorlib-dll-bodies-idir
# The same as above, but with method bodies.
CSHARP_MSCORLIB_DLL_BODIES_IDIR_TGZ:=$(CSHARP_MSCORLIB_DLL_BODIES_IDIR_DIR).tgz

# Currently there is no way to call cov-emit-cs with no source.
CSHARP_MSCORLIB_DLL_IDIR_SRC_FILE:=$(RELEASE_ROOT_DIR)/test-data/IgnoreMe.cs
CSHARP_MSCORLIB_DLL_BODIES_IDIR_SRC_FILE:=$(RELEASE_ROOT_DIR)/test-data/IgnoreMeBodies.cs

ifeq ($(IS_WINDOWS),1)

# Path to Visual Studio 2013 version of MSBuild.
# Warn or abort build if it cannot be found and provide informative message.
# Since cygwin is a 32-bit process, the PROGRAMFILES environment variable will
# expand to the correct value of "%SYSTEMDRIVE%\Program Files (x86)".
MSBUILD := $(shell cygpath -u "$(PROGRAMFILES)")/MSBuild/12.0/Bin/MSBuild.exe
ifneq ($(shell test -x '$(MSBUILD)'; echo $$?),0)
    # A line break
    define br


    endef

    # Visual Studio 2013 is mandatory.
    $(error $(br)\
        **************************************************$(br)\
        ERROR$(br)\
        **************************************************$(br)\
        MSBuild was not found at $(MSBUILD)$(br)\
        Visual Studio 2013 is now required to build the full prevent repo under Windows$(br)\
        Please follow the upgrade instructions at https://wiki.sf.coverity.com/index.php/Visual_Studio_2013_rollout$(br)\
        **************************************************$(br))

    MSBUILD := $(OLD_MSBUILD)
endif

MSBUILD_VERSION := $(shell '$(MSBUILD)' /version | grep 'Build Engine [Vv]ersion' | sed 's/\(.* \)\([0-9]\+\)\(\.[0-9].*\)/\2/')
CSC:=$(shell ls /cygdrive/c/Windows/Microsoft.NET/Framework/v{3,4}*/csc.exe | sort | tail -1)
ILASM:=$(shell ls /cygdrive/c/Windows/Microsoft.NET/Framework/v{3,4}*/ilasm.exe 2>/dev/null | sort | tail -1)
else

XBUILD:=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/mono/bin/xbuild

ifneq ($(wildcard $(XBUILD)),)
MSBUILD:=$(XBUILD)
CSC:=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/mono/bin/mcs -codepage:65001
ILASM:=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/mono/bin/ilasm -codepage:65001
endif

endif # IS_WINDOWS

# The C# related binaries are only applicable for the Windows
# releases, but we're building them for testing purposes on
# other platforms (currently just Linux64).
ifeq ($(IS_WINDOWS),1)
    CSHARP_BIN_DIR:=$(RELEASE_BIN_DIR)
else
    CSHARP_BIN_DIR:=$(TEST_BIN_DIR)
endif

COV_EMIT_CSHARP := $(CSHARP_BIN_DIR)/cov-emit-cs$(EXE_POSTFIX)

# Dependencies for a command that invokes cov-emit-cs
COV_EMIT_CSHARP_DEPS := \
  $(COV_EMIT_CSHARP) \
  $(CSHARP_BIN_DIR)/cov-internal-emit-dotnet.exe \
  $(CSHARP_BIN_DIR)/CovEmitDotNet.dll

endif # !NO_CSHARP

GCC_CONFIG_DIR:=$(RELEASE_ROOT_DIR)/gcc-config
GCC_CONFIG:=$(GCC_CONFIG_DIR)/coverity_config.xml

TEST_DIR:=/tmp/$(USER)/coverity-testsuite

# repository for document
DOC_REP:=doc-dev

# set the PATH to make sure we don't use any system GCC's (cov-configure likes to go through the PATH)
ifeq (1,$(USING_CYGWIN))

# Do not override CYGWIN_PATH if it is already set. This would happen
# if we have called make recursively and we would end up with the packages bin
# directory in our path
ifeq ($(CYGWIN_PATH),)
CYGWIN_PATH := $(PATH)
endif

export CYGWIN_PATH
PATH:=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/bin:$(PATH)
export PATH
endif

# Java tools for use in tests
# sgoldsmith 2011-09-19: Why is the following logic duplicated in test.mk and paths.mk?

ifeq ($(NO_JAVA),)
JDK_BIN_DIR:=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk/bin/)
JAVA_BIN_DIR:=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk/jre/bin/)

JAVA_HOME:=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk)
JDK_INC_DIR:=$(PLATFORM_PACKAGES_DIR)/jdk/include
export COV_TOOLS_JAR=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk/lib/tools.jar)


ifeq ($(mc_platform),mingw)
  JDK_PLATFORM:=win32
else ifeq ($(mc_platform),mingw64)
  JDK_PLATFORM:=win32
else ifeq ($(mc_platform),linux64)
  JDK_PLATFORM:=linux
else ifeq ($(mc_platform),linux-ia64)
  JDK_PLATFORM:=linux
else ifeq ($(mc_platform),solaris-x86)
  JDK_PLATFORM:=solaris
else ifeq ($(mc_platform),solaris-sparc)
  JDK_PLATFORM:=solaris
else ifeq ($(mc_platform),macosx)
  JDK_PLATFORM:=darwin
else
  JDK_PLATFORM:=$(mc_platform)
endif

ANT_HOME:=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PACKAGES_DIR)/apache-ant/1.8.2)

REAL_JAVA=$(JAVA_BIN_DIR)java$(EXE_POSTFIX)
REAL_JAVAC=$(JDK_BIN_DIR)javac$(EXE_POSTFIX)
REAL_JAR=$(JDK_BIN_DIR)jar$(EXE_POSTFIX)
REAL_JAVAP=$(JDK_BIN_DIR)javap$(EXE_POSTFIX)
REAL_JAVADOC=$(JDK_BIN_DIR)javadoc$(EXE_POSTFIX)

JAVA=$(REAL_JAVA) -Xmx256m
JAVAC=$(REAL_JAVAC) -J-Xmx256m
JAR=$(REAL_JAR) -J-Xmx256m
JAVAP=$(REAL_JAVAP) -J-Xmx256m
JAVADOC=$(REAL_JAVADOC) -J-Xmx256m

ANT_OPTS=-Xmx256m
ANT=$(BAT_PREFIX)$(ANT_HOME)/bin/ant$(BAT_POSTFIX)

MVN_HOME:=$(call SHELL_TO_NATIVE,$(top_srcdir)/$(PACKAGES_DIR)/apache-maven/3.1.1)
MVN_OPTS=-Xmx512m
MVN=$(BAT_PREFIX)$(MVN_HOME)/bin/mvn$(BAT_POSTFIX)

# Don't rely on external tools
export JAVA_HOME
export ANT_HOME
export ANT_OPTS

export MVN_HOME
export MVN_OPTS

# run-testsuite wants to know about this
export JDK_BIN_DIR
export COVERITY_PACKAGED_JAVA=$(JAVA_BIN_DIR)java$(EXE_POSTFIX)

endif # !NO_JAVA

CLANG_CC=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/bin/clang
CLANG_CXX=$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/bin/clang++

endif # PATHS_MK_INCLUDED
