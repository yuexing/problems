# 

# default-flags.mk
# compiler etc. flags

ifeq (1,$(ALL_RULES_INCLUDES))

# This tells "make" that, if making a file fails, it should delete
# that file (otherwise a rebuild might think the file is up-to-date)
.DELETE_ON_ERROR:

# ------------------------------------------------------------
# This section sets up the basic configuration variables that dictate
# the values of other variables.

# When set to 1, we use whatever gcc/g++ is in the $PATH, expecting
# that to be the compiler that comes with the OS.  When set to 0, we
# use the gcc/g++ in $platform-packages/bin.
#
# We generally take the latter approach for platforms that are popular
# (such as linux) and hence there's a strong chance of different
# developers having different compilers installed, and also for
# platforms (such as windows) that often don't have a compiler.  For
# the less-popular unix-like OSes, we skate by using whatever is in
# $PATH, even though that does risk making the build hard to
# reproduce.
USE_HOST_COMPILER:=0

# Normally we build all of Prevent, however, if FE_SUBSET is set to 1,
# we only build the FE_SUBSET.
BUILD_FE_SUBSET?=0

# Clang support is disabled by default for most platforms.
BUILD_FE_CLANG?=0

# controls if host compiler can support 128bit integers
COVERITY_HAS_INT128?=0

# When set to 1, we prefix the compiler command line with $(CCACHE),
# which arranges to cache object files on the basis of identical
# preprocessed input and command line flags.  The only time ccache is
# not used is when it isn't available for a platform.
#
# 2008-04-14: We also disable ccache for COVERAGE builds.
ifeq (HAS_CCACHE,)
HAS_CCACHE:=1
endif
# This construct is equivalent to CCACHE?=... but with a simply expanded (not
# recursively expanded) variable.  GNU Make does not have "?:=".
ifeq ($(origin CCACHE), undefined)
CCACHE:=$(PLATFORM_PACKAGES_DIR)/bin/ccache
endif
export CCACHE_DIR?=/tmp/.ccache

# Option to produce a shared library.
# Default is -shared, may be overridden in platform.mk files
PLATFORM_LD_SHLIBFLAGS?=-shared

# Option to produce position independent code.
# Default is -fPIC, may be overridden in platform.mk files
PLATFORM_SHARED_PIC_CFLAGS?=-fPIC

# Most of our platforms can use level 2 of the gcc optimizer.
# However, our code breaks strict aliasing rules in many places.
# Default is -O2, may be overriden in platform.mk files
PLATFORM_SHARED_OPTIMIZER_CFLAGS?=-O2

# Most of the platforms will not implicitly consider directories included via
# isystem as C and will simply supress warnings from them. This is the desired
# behavior. If these headers are being treated like they have 'extern C' blocks
# around them then redefine this to -I in the platform's flags makefile.
SYSINC_FLAG?=-isystem

# Command to build a library archive
ifeq ($(mc_platform),mingw64)
CREATE_LIBRARY_ARCHIVE:=$(PLATFORM_PACKAGES_DIR)/bin/x86_64-w64-mingw32-ar -cr
else
CREATE_LIBRARY_ARCHIVE:=$(PLATFORM_PACKAGES_DIR)/bin/ar -cr
endif

# Finally, let the platform-specific flags makefiles override or augment
# the above platform-independent defaults
#
# Set default for values for host cc/c++
HOST_CXX:=g++
HOST_CC:=gcc
HOST_AR:=ar
include $(RULES)/$(mc_platform)-flags.mk

# I think "DOT_EXE" is simpler than "EXE_POSTFIX" and that's what we
# use in code, so make them aliases.
DOT_EXE:=$(EXE_POSTFIX)

PLATFORM_SHARED_OPTIMIZER_CFLAGS+=-fno-strict-aliasing

# --------------------------------------------------------------
# This section computes variable values for use in rules.

# This is used to select from a set of platform-dependent source
# files, as in some-module-$(OS_TYPE).cpp.  For example, libs/system
# and build/capture-java.
ifeq (1,$(IS_WINDOWS))
OS_TYPE=windows
else
OS_TYPE=unix
endif

# Shared C/C++ flags
#
# SGM 2008-05-29: I removed -D__MC_OS_TYPE__; see run-testsuite.cpp.
SHARED_CFLAGS:=\
-Wall -fno-omit-frame-pointer \
$(PLATFORM_SHARED_CFLAGS) \
-I$(PLATFORM_PACKAGES_DIR)/include \
-I$(LIB_DIR) \
-D__MC_PLATFORM__=\"$(mc_platform)\" \

# allow #includes to be relative to the entire repo root; otherwise
# I would need to use "..", which I want to avoid
SHARED_CFLAGS += -I.

# I want to be able to #include things in gensrc by saying, e.g.,
#   #include "gensrc/foo/bar.hpp"
# since then one can tell at a glance that it is a generated file.
# So, I will use a -I pointing to the parent of $(GENSRC_DIR),
# which is $(OBJ_DIR).
SHARED_CFLAGS += -I$(OBJ_DIR)

# The gcc warning "comparison between signed and unsigned" is
# completely useless, and the casts necessary to silence it create
# clutter and can hide real problems.
SHARED_CFLAGS += -Wno-sign-compare

# BZ 59538: enable/disable some warnings explicitly, since -Wall is not
# consistent across GCC versions.  We may use different versions on different
# platforms, and there's no sense in having *some* of them fail.
#
# -Wnon-virtual-dtor is enabled in 4.0, disabled in 4.6
CXXFLAGS += -Wnon-virtual-dtor

ifeq (1,$(COVERITY_HAS_INT128))
SHARED_CFLAGS += -DCOVERITY_HAS_INT128
endif

CXXFLAGS+= \
$(PLATFORM_CXXFLAGS) \

CFLAGS+= \
$(PLATFORM_CFLAGS)

ifeq ($(mc_platform),linux64)
PLATFORM_PACKAGES_DIR_LIST=-L$(PLATFORM_PACKAGES_DIR)/lib64 -L$(PLATFORM_PACKAGES_DIR)/lib
else ifeq ($(mc_platform),freebsd64)
PLATFORM_PACKAGES_DIR_LIST=-L$(PLATFORM_PACKAGES_DIR)/lib -L$(PLATFORM_PACKAGES_DIR)/lib32
else
PLATFORM_PACKAGES_DIR_LIST=-L$(PLATFORM_PACKAGES_DIR)/lib
endif

# Libraries: In order,
# - build libraries
# - "packages" libraries
# - platform packages libraries
LD_POSTFLAGS+= \
-L$(LIB_BIN_DIR) \
$(PLATFORM_PACKAGES_DIR_LIST) \
$(PLATFORM_LD_POSTFLAGS)

LD_PREFLAGS+= \
$(PLATFORM_LD_PREFLAGS)

ifeq ($(USE_HOST_COMPILER),1)

CXX:=$(HOST_CXX)
CC:=$(HOST_CC)
AR:=$(HOST_AR)

# Same without a relative path
ABS_CXX:=$(HOST_CXX)
ABS_CC:=$(HOST_CC)
ABS_AR:=$(HOST_AR)

else

ifeq ($(mc_platform),mingw64)
CXX:=$(PLATFORM_PACKAGES_DIR)/bin/x86_64-w64-mingw32-g++
CC:=$(PLATFORM_PACKAGES_DIR)/bin/x86_64-w64-mingw32-gcc
else
CXX:=$(PLATFORM_PACKAGES_DIR)/bin/g++
CC:=$(PLATFORM_PACKAGES_DIR)/bin/gcc
endif
AR:=$(PLATFORM_PACKAGES_DIR)/bin/ar

# Same without a relative path
ABS_CXX:=$(top_srcdir)/$(CXX)
ABS_CC:=$(top_srcdir)/$(CC)
ABS_AR:=$(top_srcdir)/$(AR)

endif

# We need to turn off ccache when using gcov because the metadata files
# gcov makes are named based on the names of the input files, which are
# weirdly named under ccache.
#
# It is also disabled on a per-program basis in create-prog.mk -pdillinger
ifeq ($(BUILD_TYPE),COVERAGE)
  HAS_CCACHE := 0
endif

ifeq ($(HAS_CCACHE),0)
CCACHE:=
endif

ANALYSIS_INCLUDE:= -Ianalysis -Ianalysis/ast
ANALYSIS_LIB:=analysis
ifeq ($(mc_platform),mingw64)
ERR_FLAGS+= -Wno-deprecated-declarations
endif
ifeq ($(mc_platform),mingw)
ERR_FLAGS+= -Wno-deprecated-declarations
endif
ERR_FLAGS+= -Werror

# Clang integration
ifeq ($(BUILD_FE_CLANG),1)
ifneq (,$(CLANG_INSTALL_DIR))
CLANG_INCLUDE_DIR:=$(CLANG_INSTALL_DIR)/include
CLANG_LIB_DIR:=$(CLANG_INSTALL_DIR)/lib

CLANG_EXTRA_INCLUDE:=-I$(CLANG_INCLUDE_DIR)
CLANG_EXTRA_LDFLAGS:=-L$(CLANG_LIB_DIR)
endif

# C/C++ compiler options for projects using Clang.
# -D__STDC_LIMIT_MACROS and -D__STDC_CONSTANT_MACROS are required when
#  including Clang header files.
# -DCOVERITY is required to enable Coverity's customizations in Clang
#  header files.
CLANG_SHARED_CFLAGS:=\
-D__STDC_LIMIT_MACROS \
-D__STDC_CONSTANT_MACROS \
-DCOVERITY \
$(CLANG_EXTRA_INCLUDE)

# Linker options for projects linking with Clang.
CLANG_LDFLAGS:=\
$(CLANG_EXTRA_LDFLAGS)
endif

# Include $(SORT) in regular rules, too.
include $(RULES)/test-macros.mk

else

$(error You should include all-rules.mk)

endif
