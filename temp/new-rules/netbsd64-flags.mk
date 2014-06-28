# 

# Use -fno-inline-small-functions due to compiler bug - See BZ 65228
PLATFORM_SHARED_CFLAGS:=\
-D__MC_NETBSD__ \
-D__MC_NETBSD64__ \

PLATFORM_SHARED_OPTIMIZER_CFLAGS+=\
-fno-inline-small-functions

PLATFORM_LD_POSTFLAGS:= -static \

NO_JAVA:=1
NO_CSHARP:=1
