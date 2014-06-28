# 

# This file must be updated in concert with extend/scripts/build-checker.bat.


AUX_PATH:=-B$(PLATFORM_PACKAGES_DIR)/bin

PLATFORM_LD_PREFLAGS:= $(AUX_PATH) -Wl,--large-address-aware
PLATFORM_LD_POSTFLAGS:= \
  -mconsole \
  -lole32 \
  -loleaut32 \
  -luuid \
  -lpsapi \
  -static-libgcc \
  -static \

# The version of gcc we're currently using (3.4.5) sometimes will output an
# "uninitialized" warning on "foo" on code like: void *foo = bar();
# This causes an explosion of warnings (notably, one in stl_list.h)
PLATFORM_SHARED_CFLAGS:=\
$(AUX_PATH) \
-D__MC_MINGW__ \
-D__MC_MINGW32__ \
-include unistd.h \
-Wno-uninitialized \
-Wno-unused-local-typedefs \
-Wno-unknown-pragmas \
-Wno-deprecated \

# Some system headers (e.g. commdlg.h) have this warning :(
PLATFORM_CXXFLAGS:=\
-Wno-non-virtual-dtor

BUILD_FE_CLANG:=1

#HAS_CCACHE=0
export CCACHE_DIR=$(call SHELL_TO_NATIVE,$(TMP))/.ccache

GLOBAL_RESOURCE_FILE:=$(OBJ_DIR)/objs/global.res

MSBUILD_PLATFORM=x86
