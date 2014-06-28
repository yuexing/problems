# 

BUILD_FE_CLANG:=1

# Indicates that running 'make regenerate-models' is allowed
# (bug 57233)
ENABLE_BUILDING_LIB_MODELS:=1

PLATFORM_LD_POSTFLAGS:=-static

PLATFORM_SHARED_CFLAGS:=\
-m64 -mtune=opteron \
-D__MC_LINUX__ \
-D__MC_LINUX64__ \

# Add -W flags here so that they are stripped when building packages/* because
#   we don't want to worry about warnings in 3rd party code.
PLATFORM_WARNING_FLAGS:=\


PLATFORM_SHARED_CFLAGS+=\
-fPIC \
-fvisibility=hidden

MSBUILD_PLATFORM:=x64
