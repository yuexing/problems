# 


PLATFORM_LD_POSTFLAGS :=

# This is necessary so that getsockname (and perhaps other functions)
# understand that socklen_t is a 64-bit quantity.  One thread
# discussing the problem (and our observed symptoms):
#
# http://unix.derkeiler.com/Newsgroups/comp.sys.hp.hpux/2005-02/0186.html
PLATFORM_LD_POSTFLAGS += -lxnet

PLATFORM_LD_POSTFLAGS += \
-mlp64 -nodefaultlibs -Wl,-a,archive_shared -Wl,-lstdc++ -Wl,-lunwind -Wl,-lgcc_eh -Wl,-lgcc \
-Wl,-a,shared_archive -Wl,-lm -Wl,-lc -Wl,-ldld -Wl,-a,archive_shared

PLATFORM_SHARED_CFLAGS:=\
-mlp64 \
-D__MC_HPUX__ \
-D__MC_HPUX_IA64__ \

USE_HOST_COMPILER:=1

PLATFORM_SHARED_OPTIMIZER_CFLAGS:=\
-O1 \

NO_JAVA:=1
NO_CSHARP:=1
