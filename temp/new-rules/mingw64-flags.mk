# 

EXE_POSTFIX:=.exe
BAT_PREFIX:=cmd.exe /c
BAT_POSTFIX:=.bat

AUX_PATH:=-B$(PLATFORM_PACKAGES_DIR)/bin

PLATFORM_LD_PREFLAGS:= $(AUX_PATH)
# BJF - need to add -static-libgcc as it's not the default in mingw64
PLATFORM_LD_POSTFLAGS:= -mconsole -lole32 -loleaut32 -luuid -lpsapi -static-libgcc -static

# BJF - Even though mignw64 is gcc 4.4.1, keep -Wno-uninitialized
# just to avoid the warnings. Would be worth coming back and trying w/out
# to see if it's still needed.
# 
# Use -Wno-deprecated to ignore warning about hash_map being deprecated:
# prevent-5.0-trunk/mingw64-packages/bin/../lib/gcc/x86_64-w64-mingw32/4.4.1/../../../../x86_64-w64-mingw32/include/c++/4.4.1/backward/backward_warning.h:28:2: warning: #warning This file includes at least one deprecated or antiquated header which may be removed without further notice at a future date. Please use a non-deprecated interface with equivalent functionality instead. For a listing of replacement headers and interfaces, consult the file backward_warning.h. To disable this warning use -Wno-deprecated.

PLATFORM_SHARED_CFLAGS:=\
$(AUX_PATH) \
-D__MC_MINGW__ \
-D__MC_MINGW64__ \
-Wno-uninitialized \
-Wno-deprecated \

IS_WINDOWS:=1

BUILD_FE_CLANG:=1

#HAS_CCACHE=0
export CCACHE_DIR=$(call SHELL_TO_NATIVE,$(TMP))/.ccache

GLOBAL_RESOURCE_FILE:=$(OBJ_DIR)/objs/global.res

MSBUILD_PLATFORM:=x64
