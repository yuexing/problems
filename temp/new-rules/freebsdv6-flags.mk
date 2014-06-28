# 

PLATFORM_SHARED_CFLAGS:=\
-D__MC_FREEBSD__ \
-D__MC_FREEBSD32__ \
-D__MC_FREEBSDv6__ \

#PLATFORM_LD_POSTFLAGS:=-nodefaultlibs \
#-static \
#-lstdc++ \
#-lgcc_eh \
#-lgcc \
#-Wl,-Bstatic -lm -lc \
#-lgcc \

PLATFORM_LD_POSTFLAGS:=-static

BUILD_FE_CLANG:=0

NO_JAVA:=1
NO_CSHARP:=1
