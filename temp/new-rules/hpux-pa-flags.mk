# 

PLATFORM_LD_POSTFLAGS:=\
-nodefaultlibs -static -Wl,-a,archive_shared -lstdc++ -lgcc_eh -lgcc -lcl \
-Wl,-a,shared_archive -Wl,-lm -Wl,-lc -Wl,-ldld -Wl,-a,archive_shared

PLATFORM_SHARED_CFLAGS:=\
-D__MC_HPUX__ \
-D__MC_HPUX_PA__ \
-DHAVE_STAT64 \
-D_LARGEFILE64_SOURCE \
-D_LARGEFILE_SOURCE \

NO_JAVA:=1
NO_CSHARP:=1
