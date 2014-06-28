# 

PLATFORM_SHARED_CFLAGS:=\
-D__MC_NETBSD__ \
-D__MC_NETBSD32__ \

PLATFORM_LD_POSTFLAGS:=-nodefaultlibs \
-static \
-lstdc++ \
-lgcc_eh \
-lgcc \
-Wl,-Bstatic -lm -lc \
-lgcc \

NO_JAVA:=1
NO_CSHARP:=1
