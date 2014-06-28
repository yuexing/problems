# 

PLATFORM_LD_PREFLAGS:=\
-nodefaultlibs \
-Wl,-Bstatic \

PLATFORM_LD_POSTFLAGS:=\
-lstdc++ \
-lgcc_eh \
-lgcc \
-Wl,-Bdynamic -lc -lm -Wl,-Bstatic \
-lgcc -Wl,-Bdynamic -Wl,-rpath-link,/lib \


PLATFORM_SHARED_CFLAGS:=\
-D__MC_SOLARIS__ \
-D__MC_SOLARIS_X86__ \
-DHAVE_STAT64 \
-D_LARGEFILE64_SOURCE \
-D_LARGEFILE_SOURCE \

NO_CSHARP:=1
