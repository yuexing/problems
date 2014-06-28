# This sets the minimum target for the linker and also the default
# minimum target for the compiler (which can be overridden with
# -DMAC_OS_X_VERSION_MIN_REQUIRED=<version> on the compiler command
# line and -mmacosx-version-min=<version> on the link command
# line.)
# This is the earliest Mac OS X version to which we promise to be
# backwards compatible; i.e. the earliest version the binaries we
# build will run on. Currently, this is set to Mac OS X 10.6 (Snow Leopard).
MACOSX_DEPLOYMENT_TARGET:=10.6
export MACOSX_DEPLOYMENT_TARGET

# Determine the OSX version to decide what compiler to use
DARWIN_KERNEL_VERSION=$(shell uname -r | cut -d. -f1)

# While the version of LLVM-GCC in Xcode 4.1 (on 10.7) compiles our code
# properly, the LLVM-GCC in previous versions of Xcode appear to hit
# internal compiler errors when compiling our code.  The released builds
# will only be built on 10.7 with LLVM-GCC, however in order to not
# block developers who work on 10.6, we'll just use GCC on 10.6 to
# avoid the issue with LLVM-GCC.

# MacOSX 10.7: Use LLVM-GCC
ifeq ("$(DARWIN_KERNEL_VERSION)", "11")

HOST_CXX:=llvm-g++
HOST_CC:=llvm-gcc

# MacOSX 10.8: Use LLVM-GCC
else ifeq ("$(DARWIN_KERNEL_VERSION)", "12")

HOST_CXX:=llvm-g++
HOST_CC:=llvm-gcc

# MacOSX 10.9: Use LLVM-GCC
else ifeq ("$(DARWIN_KERNEL_VERSION)", "13")

HOST_CXX:=llvm-g++
HOST_CC:=llvm-gcc

# MacOSX 10.6: Use GCC (will be gcc-4.2)
else ifeq ("$(DARWIN_KERNEL_VERSION)", "10")

HOST_CXX:=g++
HOST_CC:=gcc

# Unknown
else
$(error "Build only works on MacOSX 10.6, 10.7, or 10.8!")
endif # DARWIN_KERNEL_VERSION

# Set the SDK to use
ifeq ("$(DARWIN_KERNEL_VERSION)", "12")
MACOSX_SDK:=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk
else ifeq ("$(DARWIN_KERNEL_VERSION)", "13")
MACOSX_SDK:=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk
else
MACOSX_SDK:=/Developer/SDKs/MacOSX10.6.sdk
endif
MACOSX_VER:=__MC_MACOSX_6__

# Configure default gcc, too - a number of regression tests require that.
CONFIGURE_HOST_GCC:=1

# Build Clang and Clang related binaries
BUILD_FE_CLANG:=1

PLATFORM_LD_POSTFLAGS:=\
-flat_namespace \
-isysroot$(MACOSX_SDK) \
-Wl,-search_paths_first \
-lstdc++ \
-lc \

PLATFORM_SHARED_CFLAGS:=\
-isysroot$(MACOSX_SDK) \
-D__MC_MACOSX__ \
-D$(MACOSX_VER)

ifeq ("$(DARWIN_KERNEL_VERSION)", "13")
PLATFORM_SHARED_CFLAGS+=\
-Qunused-arguments \
-Wno-bool-conversion \
-Wno-dangling-else \
-Wno-empty-body \
-Wno-header-guard \
-Wno-logical-not-parentheses \
-Wno-mismatched-tags \
-Wno-non-literal-null-conversion \
-Wno-null-conversion \
-Wno-overloaded-virtual \
-Wno-parentheses-equality \
-Wno-self-assign \
-Wno-sometimes-uninitialized \
-Wno-tautological-compare \
-Wno-undefined-internal \
-Wno-unneeded-internal-declaration \
-Wno-unused-comparison \
-Wno-unused-function \
-Wno-unused-private-field \
-Wno-unused-value \
-Wno-unused-variable \

endif


SHARED_COMPILE_ONLY_CFLAGS+=-arch i386
PLATFORM_LD_PREFLAGS+=-arch i386

USE_HOST_COMPILER:=1
CREATE_LIBRARY_ARCHIVE:=libtool -static -o

NO_CSHARP:=1
