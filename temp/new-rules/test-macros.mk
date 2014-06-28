# test-macros.mk
# 

# Some macros to help writing Makefile-based tests.  Separate from
# test.mk because test.mk has that 'complain' target that I do not
# like.

# This command is used in targets to remove core files.  It has to
# handle the variety of ways that core files get named, but avoid
# accidentally removing things that have "core" as a substring.
RM_CORE_FILES := rm -f core core.* *.core

# Sorting: "sort" is locale-dependent; in particular comparing a '.'
# to an alphanumeric is not necessarily consistent.
# Use $(SORT)
SORT:=LC_ALL=C sort

# Joining: "join" is locale-dependent. Use $(JOIN)
JOIN:=LC_ALL=C join

# fgrep can be locale-dependent, rejecting encoded characters
FGREP:=LC_ALL=C fgrep

# sed can be locale-dependent, rejecting encoded characters
SED:=LC_ALL=C sed

# Append "REMOVE_CR" to a command so that its output doesn't
# contain CR characters ('\r')
ifeq ($(IS_WINDOWS),1)
REMOVE_CR:=| dos2unix
else
REMOVE_CR:=
endif

# Portable md5sum command.
# Usage:
# $(call MD5,FILE)
# Will print the md5, with a newline.

# For all the commands below, the md5 hash is at the beginning of the
# line, and uses lowercase digits.
SED_MD5:=sed -n s/'^\([0-9a-f]\{32\}\).*$$/\1/p'

ifneq ($(filter macosx freebsd freebsdv6 freebsd64,$(mc_platform)),)
# On freebsd/MacOSX, we have md5 -r
# Use $(value SED_MD5) to avoid evaluating that $ again.
MD5=md5 -r $(1) | $(value SED_MD5)
else ifneq ($(filter netbsd,$(mc_platform)),)
# Something different on netbsd
MD5=md5 -n $(1) | $(value SED_MD5)
else
# The default; works on Linux, cygwin, HP/UX
MD5=md5sum $(1) | $(value SED_MD5)
endif

# EOF
