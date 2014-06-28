# 

PLATFORM_LD_POSTFLAGS:=-Wl,-bbigtoc 
# Building -static on 5.3 crashes during startup on 6.1
# -static -lcrypt

PLATFORM_SHARED_CFLAGS:=\
-mminimal-toc \
-mcpu=common \
-D_LINUX_SOURCE_COMPAT \
-D__MC_AIX__ \
-D__MC_AIX_PPC__

NO_JAVA:=1
NO_CSHARP:=1

# AIX is the only platform on which we don't support analysis.
# Ugh.
NO_ANALYSIS:=1

# AIX will consider folders included via -isystem as C, so use -I instead.
SYSINC_FLAG:=-I
