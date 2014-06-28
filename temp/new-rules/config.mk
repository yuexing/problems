# 

ifeq (,$(CONFIG_MK_INCLUDED))
CONFIG_MK_INCLUDED:=1

RULES:=$(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

top_srcdir:=$(RULES)/..

ifeq (,$(mc_platform))
CONFIG_GUESS=$(shell $(RULES)/../rules/config.guess)
mc_platform:=$(shell cd $(RULES)/..; sh get_platform.sh $(CONFIG_GUESS))

ifneq ($(filter mingw mingw64,$(mc_platform)),)
IS_WINDOWS:=1
export IS_WINDOWS

#BJF use /tmp for all windows builds, better than the default user dir which has spaces
export TEMP=/tmp
export TMP=/tmp

ifeq (i686-pc-cygwin,$(CONFIG_GUESS))
USING_CYGWIN:=1
# Export, it needs to pass through run-testsuite
export USING_CYGWIN
else
USING_MINGW:=1
# Export, it needs to pass through run-testsuite
export USING_MINGW
endif # cygwin

else  # mingw
IS_WINDOWS:=
endif # mingw

endif # $(mc_platform)

# Variable representing the user for connecting to remote machines
# (cvs, git) for checking out files
PACKAGE_USER?=$(USER)

include $(RULES)/config-common.mk

top_srcdir:=$(call normalize_path,$(top_srcdir))

# "Native" (i.e. windows as opposed to cygwin) name for the top src dir. Can be useful e.g. to pass arguments to windows commands.
native_top_srcdir:=$(patsubst %/,%,$(call SHELL_TO_NATIVE,$(top_srcdir)))


endif
