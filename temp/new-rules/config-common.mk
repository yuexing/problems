# 

# Set this here, since this file is usually included
# We want to use bash
# Use preferentially /usr/local/bin/bash
# Otherwise fall back to /bin/bash
# "which bash" would do path search, which would be nice, however I
# don't trust "which" on the default shell.
PREFERRED_SH:=/usr/local/bin/bash
FALLBACK_SH:=/bin/bash

SHELL:=$(shell \
if [ -f $(PREFERRED_SH) ]; \
then echo $(PREFERRED_SH); \
else echo $(FALLBACK_SH); fi)

ifeq (1,$(USING_CYGWIN))
SHELL_TO_NATIVE=$(shell cygpath -m $(1) | tr '[:upper:]' '[:lower:]')
# SHELL_TO_OS converts to the Windows back slash
SHELL_TO_OS=$(shell cygpath -w $(1) | tr '[:upper:]' '[:lower:]')
NATIVE_TO_SHELL=$(shell cygpath -u $(1))
else
SHELL_TO_NATIVE=$(1)
SHELL_TO_OS=$(1)
NATIVE_TO_SHELL=$(1)
endif

normalize_path=$(abspath $(1))

# The strings to prepend or append to a name to get the
# corresponding file name.
#   Shared library: Prepend "" on Windows, "lib" elsewhere.
#   Shared library: Append ".dll" on Windows, ".so" elsewhere.
#   Program: Append ".exe" on Windows, "" elsewhere.
ifeq ($(IS_WINDOWS),1)
SHLIB_PREFIX:=
SHLIB_POSTFIX:=.dll
EXE_POSTFIX:=.exe
BAT_PREFIX:=cmd.exe /c
BAT_POSTFIX:=.bat
SHLIB_PREFIX:=
SHLIB_POSTFIX:=.dll
else
SHLIB_PREFIX:=lib
SHLIB_POSTFIX:=.so
EXE_POSTFIX:=
BAT_PREFIX:=
BAT_POSTFIX:=
SHLIB_PREFIX:=lib
SHLIB_POSTFIX:=.so
endif

ifeq ($(mc_platform),linux64)
COVERITY_HAS_INT128:=1
endif

ifneq ($(filter linux linux64 mingw64 macosx freebsd freebsd64,$(mc_platform)),)
export CLANG_TEST_SUBSET=full
endif

# Set PLATFORM_HAS_EXTEND to 1 iff the platform supports building
# Extend checkers.
ifneq (,$(filter linux linux64 linux-ia64 \
                 freebsd freebsdv6 freebsd64 netbsd netbsd64 solaris-sparc \
                 solaris-x86 hpux-pa mingw mingw64, \
                 $(mc_platform)))
  PLATFORM_HAS_EXTEND := 1
else
  PLATFORM_HAS_EXTEND := 0
endif

# EOF
