# 

PLATFORM_SHARED_CFLAGS:=\
-D__MC_FREEBSD__ \
-D__MC_FREEBSD32__ \

PLATFORM_LD_POSTFLAGS:=-nodefaultlibs \
-Wl,-Bstatic,-lstdc++,-lgcc_eh,-lgcc,-lm,-Bdynamic,-lc\

BUILD_FE_CLANG:=1

NO_JAVA:=1
NO_CSHARP:=1
