ifeq (,$(SYMLINKS_INCLUDED))
SYMLINKS_INCLUDED:=1

include new-rules/all-rules.mk

define setup_file_copy
$(call setup_file_copy_2_dirs,,,$(value 1))
endef

# Files that have the same path in the repo and in the release
FILES_TO_COPY:=\
dtd/license.dtd \
dtd/license-trial.dtd \
dtd/license-v1.dtd \
dtd/license-trial-v1.dtd \
dtd/flexnet-trial-v1.dtd \
dtd/compile.dtd \
dtd/config.dtd \
dtd/escapes.dtd \
dtd/coverity_config.dtd \
dtd/table.dtd \
dtd/annotation.dtd \
dtd/reports.dtd \
dtd/analyze-build.dtd \
dtd/catalog.xml \
xsl/error-index.xsl \
xsl/error-summary.xsl \
$(wildcard config/default_models*.xml) \
$(wildcard config/templates/*/*.h) \
$(wildcard config/templates/*/*.xml) \
$(wildcard config/templates/*/*.dat) \
$(wildcard config/templates/*/*.json) \
$(wildcard config/templates/*/*/*.h) \
$(wildcard config/templates/*/*/*.xml) \
$(wildcard config/templates/*/*/*.dat) \
$(wildcard config/templates/*/*/*.json) \
config/analyze-build.xml \
config/user_nodefs.h \
config/cluster.conf \
config/desktop-checker-properties-en.json \
config/dist-policy.conf \
certs/ca-certs.pem \

#
# Add CIT Sources
#
FILES_TO_COPY+= \
$(wildcard config/templates/*/*.cpp) \

DOC_FILES:=\
examples/Makefile \
examples/test.c \

$(foreach file,$(DOC_FILES),$(eval $(call setup_file_copy_2_dirs,misc/,doc/,$(file))))

# Example policies for test analysis
TA_POLICY_FILES:=\
$(wildcard doc/test-analysis/policy-language/ta-examples/*) \
analysis/test-analysis/policies/one-hundred.json

$(foreach file,$(TA_POLICY_FILES),$(eval $(call setup_file_copy_2_dirs,$(dir $(file)),doc/examples/ta-policies/,$(notdir $(file)))))

# Example policies for test prioritization
TP_POLICY_FILES:=\
$(wildcard doc/test-analysis/policy-language/tp-examples/*)

$(foreach file,$(TP_POLICY_FILES),$(eval $(call setup_file_copy_2_dirs,$(dir $(file)),doc/examples/tp-policies/,$(notdir $(file)))))

# Test separation header for TA
TA_HEADERS:=\
build/capture/test-separation/coverity-test-separation.h

$(foreach file,$(TA_HEADERS),$(eval $(call setup_file_copy_2_dirs,$(dir $(file)),library/,$(notdir $(file)))))


# Copy all the examples in doc/examples to the release
MISC_DOC_EXAMPLES:=$(shell cd doc/examples; find . -type f)
$(foreach file,$(MISC_DOC_EXAMPLES),$(eval $(call setup_file_copy_2_dirs,doc/examples/,doc/examples/,$(file))))

LIC_FILES:=\

$(foreach file,$(LIC_FILES),$(eval $(call setup_file_copy_2_dirs,misc/license/,doc/,$(file))))

# Per bug 10655, remove the following file. 
# Lauren 20080205
# doc/edg_cmdline.pdf \

ifneq (1,$(BUILD_FE_SUBSET))

FILES_TO_COPY+= \
bin/license.dat

# Do not build with "NO_CSHARP"
ifeq ($(NO_CSHARP),)

$(eval $(call setup_file_copy_2_dirs,utilities/,test-bin/,unified-cs-build.sh))
$(eval $(call setup_file_copy_2_dirs,utilities/,test-bin/,unified-cs-build-here.sh))

$(eval $(call setup_file_copy_2_dirs,utilities/,test-bin/,unified_java_build.sh))
$(eval $(call setup_file_copy_2_dirs,utilities/maven-settings/,test-bin/maven-settings/,settings.xml))
$(eval $(call setup_file_copy_2_dirs,utilities/maven-settings/,test-bin/maven-settings/,settings2.xml))
$(eval $(call setup_file_copy_2_dirs,utilities/maven-settings/,test-bin/maven-settings/,settings3.xml))

ifneq ($(filter linux64,$(mc_platform)),)

LIBSTDCPP_FILES:=\
libstdc++.so \
libstdc++.so.6 \
libstdc++.so.6.0.16 \
libstdc++.so.6.0.16-gdb.py \
libgcc_s.so.1 \
libgcc_s.so \

$(foreach file,$(LIBSTDCPP_FILES),$(eval $(call setup_file_copy_2_dirs,$(PLATFORM_PACKAGES_DIR)/lib64/,test-bin/mono/lib/,$(file))))

endif # linux64

endif # NO_CSHARP

ifeq ($(NO_JAVA),)
FILES_TO_COPY+= \
$(wildcard findbugs-ext/config/*.*) \
$(wildcard findbugs-ext/*.*) \

endif # NO_JAVA

#If flexnet enabled, copy files related to flexnet licenses
ifneq ($(filter mingw mingw64 linux linux64 solaris-x86 solaris-sparc macosx hpux-pa,$(mc_platform)),) 

FILES_TO_COPY+= \
bin/sample-license.config \

FILES_TO_COPY+= \
bin/license.config \

# NOTE: All executable files should go in bin or test-bin. The installer sets
# the execute bit in a limited number of places; placing things in a new
# directory may result in them not being executable.
$(eval $(call setup_file_copy_2_dirs,bin/,test-bin/,license.config))
$(eval $(call setup_file_copy_2_dirs,utilities/,test-bin/,find-symbol.pl))
ifneq ($(mc_platform),macosx)
$(eval $(call setup_file_copy_2_dirs,$(PLATFORM_PACKAGES_DIR)/bin/,test-bin/,nm))
endif


ifeq (1,$(IS_WINDOWS))
FILES_TO_COPY+= \
bin/generate-flexnet-hostid.bat
else
FILES_TO_COPY+= \
bin/generate-flexnet-hostid
endif

endif # flexnet platform

endif # FE subset must explicitly copy in its own license(s)

ifeq (1,$(IS_WINDOWS))
FILES_TO_COPY+= \
$(wildcard scripts/vs2010/kill_msbuilds/kill_msbuilds.s*) \
$(wildcard scripts/vs2010/kill_msbuilds/prj-*/prj*.vcxproj*)
endif

$(foreach file,$(FILES_TO_COPY),$(eval $(call setup_file_copy,$(file))))

ifneq ($(filter linux linux64 solaris-x86 solaris-sparc macosx hpux-pa,$(mc_platform)),) 
FLEXNET_FILES_TO_COPY:=covlicd lmgrd lmutil
endif

ifneq ($(filter mingw mingw64,$(mc_platform)),)
FLEXNET_FILES_TO_COPY:=covlicd.exe lmgrd.exe lmutil.exe
endif

$(foreach file,$(FLEXNET_FILES_TO_COPY),$(eval $(call setup_file_copy_2_dirs,$(PLATFORM_PACKAGES_DIR)/flexnet-license-server/,bin/,$(file))))

# --------------- GUI ----------------

CONFIG_FILES:=\
mime.types \
httpd.conf.template \
default-server.crt \
default-server.key \

$(foreach file,$(CONFIG_FILES),$(eval $(call setup_file_copy_2_dirs,gui/config/,config/,$(file))))

# ---------------- ICU -------------------
# Data files (currently 1) needed by ICU and copied into 
# $RELEASE_ROOT_DIR/config.
# Mac OS X is PPC and 386, so it needs both formats

ifneq ($(filter aix hpux-pa hpux-ia64 macosx solaris-sparc,$(mc_platform)),)
  # Big-Endian
  ICU_FILES += icudt36b.dat
endif

ifneq ($(filter freebsd freebsdv6 freebsd64 linux linux64 linux-ia64 macosx mingw mingw64 netbsd netbsd64 solaris-x86,$(mc_platform)),)
  # Little-Endian
  ICU_FILES += icudt36l.dat 
endif

$(foreach file,$(ICU_FILES),$(eval $(call setup_file_copy_2_dirs,packages/icu/source/data/in/,config/,$(file))))

ifneq ($(mc_platform),aix)
ifneq (1,$(BUILD_FE_SUBSET))
# ----------WRAPPER_ESCAPE config -------
# wrapper_escape.conf copied into $RELEASE_ROOT_DIR/config.

WE_CONFIG_FILES:= wrapper_escape.conf
$(foreach file, $(WE_CONFIG_FILES),$(eval $(call setup_file_copy_2_dirs,analysis/checkers/generic/checkers/wrapper-escape/,config/,$(file))))

NULL_CHECK_FILES:= android-null-checks.conf
$(foreach file, $(NULL_CHECK_FILES),$(eval $(call setup_file_copy_2_dirs,analysis/checkers/generic/checkers/forward-null/,config/,$(file))))

# --------------- Library ----------------

# 1 = "module", 2 = file
define copy_lib_file
$(call setup_file_copy_2_dirs,analysis/checkers/$(value 1)/library/,library/$(value 1)/,$(value 2))
endef

$(eval $(call setup_file_copy_2_dirs,analysis/checkers/models/c/,library/,decls.h))
$(eval $(call setup_file_copy_2_dirs,analysis/checkers/models/c/,library/,primitives.h))

GENERIC_LIB_FILES:=\
common/gcc-builtin.c \
common/killpath.c \
common/new.cc \
libc/all/all.c \
linux/killpath.c \
linux/kmalloc.c \
linux/lock.c \
linux/taint.c \
stdcxx/stl.cc \
symbian/symbian.cc \
win32/all.c \
win32/src/a.c \
win32/src/b.c \
win32/src/c.c \
win32/src/d.c \
win32/src/e.c \
win32/src/f.c \
win32/src/g.c \
win32/src/h.c \
win32/src/i.c \
win32/src/j.c \
win32/src/k.c \
win32/src/l.c \
win32/src/m.c \
win32/src/n.c \
win32/src/o.c \
win32/src/p.c \
win32/src/q.c \
win32/src/r.c \
win32/src/s.c \
win32/src/t.c \
win32/src/u.c \
win32/src/v.c \
win32/src/w.c \
win32/src/z.c \
win32/src/sysalloc.c \
win32/src/winapis-safe.c \
win32/src/string.c \
win32-sal/sal.c \
win32-sal/win32_header.h \
win32-sal/cov_specstrings_strict.h

$(foreach file,$(GENERIC_LIB_FILES),$(eval $(call copy_lib_file,generic,$(file))))

CONCURRENCY_LIB_FILES:=\
linux.c \
pthreads.c \
threadx.c \
vxworks.c \
win32.c \
boost.c \
symbian.c

$(foreach file,$(CONCURRENCY_LIB_FILES),$(eval $(call copy_lib_file,concurrency,$(file))))

SECURITY_LIB_FILES:=\
bsd_taint.c \
gcc-2.95.3/basic_string.cc \
killpath.c \
linux_killpath.c \
linux_malloc.c \
linux_taint.c \
posix/aio.c \
posix/arpa_inet.c \
posix/ctype.c \
posix/curses.c \
posix/dirent.c \
posix/dlfcn.c \
posix/fcntl.c \
posix/fmtmsg.c \
posix/fnmatch.c \
posix/ftw.c \
posix/glob.c \
posix/grp.c \
posix/iconv.c \
posix/langinfo.c \
posix/libgen.c \
posix/locale.c \
posix/math.c \
posix/monetary.c \
posix/mqueue.c \
posix/ndbm.c \
posix/netdb.c \
posix/new.cc \
posix/nl_types.c \
posix/poll.c \
posix/pthread.c \
posix/pwd.c \
posix/re_comp.c \
posix/regex.c \
posix/regexp.c \
posix/sched.c \
posix/search.c \
posix/semaphore.c \
posix/setjmp.c \
posix/signal.c \
posix/stdio.c \
posix/stdlib.c \
posix/string.c \
posix/strings.c \
posix/stropts.c \
posix/sys_ipc.c \
posix/syslog.c \
posix/sys_mman.c \
posix/sys_msg.c \
posix/sys_resource.c \
posix/sys_sem.c \
posix/sys_shm.c \
posix/sys_socket.c \
posix/sys_stat.c \
posix/sys_statvfs.c \
posix/sys_timeb.c \
posix/sys_time.c \
posix/sys_times.c \
posix/sys_uio.c \
posix/sys_utsname.c \
posix/sys_wait.c \
posix/term.c \
posix/termios.c \
posix/time.c \
posix/ucontext.c \
posix/ulimit.c \
posix/unctrl.c \
posix/unistd.c \
posix/utime.c \
posix/utmpx.c \
posix/wchar.c \
posix/wctype.c \
posix/wordexp.c \
sql-injection/mysql.c \
sql-injection/odbc.c \
sql-injection/oracle-oci.c \
sql-injection/postgres.c \
sql-injection/sqlite.c \
sql-injection/sql-server.c \
stl/basic_string.c \
win32/all.c \
win32/src/a.c \
win32/src/b.c \
win32/src/c.c \
win32/src/d.c \
win32/src/e.c \
win32/src/f.c \
win32/src/g.c \
win32/src/h.c \
win32/src/i.c \
win32/src/j.c \
win32/src/k.c \
win32/src/l.c \
win32/src/m.c \
win32/src/n.c \
win32/src/o.c \
win32/src/p.c \
win32/src/q.c \
win32/src/r.c \
win32/src/s.c \
win32/src/t.c \
win32/src/u.c \
win32/src/v.c \
win32/src/w.c \
win32/src/winapis-secure.c \
win32/src/z.c 

$(foreach file,$(SECURITY_LIB_FILES),$(eval $(call copy_lib_file,security,$(file))))

# Copy README from generic/library/common to release_dir/library
$(eval $(call setup_file_copy_2_dirs,analysis/checkers/generic/library/common/,library/,README))

UNIFIED_TEST_FILES := \
    analysis/checkers/security/checkers/xss-injection/jtest-testbed-full/framework.conf \
    $(wildcard analysis/checkers/generic/checkers/*/jtest*/*.java) \
    $(wildcard analysis/checkers/generic/checkers/*/jtest*/*.conf) \
    $(wildcard analysis/checkers/generic/checkers/*/jtest*/*.java.bytecode) \
    $(wildcard analysis/checkers/generic/checkers/*/test_spec.xml) \
    $(wildcard analysis/checkers/generic/checkers/*/jtest*/*/*/*/*/*.java) \
# Files below were used in tests that aren't run currently:
# 	all_analysis_on_all_java (disabled BZ 45726)
#	all_analysis_on_many_compilers (inactive BZ 36318)
#    $(wildcard jfe/test-java-interp/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-enums/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-exceptions/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-finally-scalability/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-generics/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-overloading/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-preserve-subexpressions/*.java) \
#    $(wildcard analysis/cov-analyze/jtest-virtual-ai-micro/*.java) \
    $(shell find analysis/checkers/jp-testsuite -name *.java -print) \
    $(wildcard analysis/checkers/jp-testsuite/*.jar) \
    $(shell find packages -name '*.jar') \
    $(shell find analysis/checkers/security/checkers -name '*.jar') \
    $(shell find analysis/checkers/security/checkers -name *.java -print) \
    $(shell find analysis/checkers/models/java -name *.java -print)
$(foreach file,$(UNIFIED_TEST_FILES),$(eval $(call setup_file_copy_2_dirs,./,test-data/,$(file))))

endif # not needed by FE subset
endif # no checkers on emit-only platforms either

.PHONY:symlinks

# SYMLINK_FILES defined in new-rules/functions.mk
symlinks: $(SYMLINK_FILES)

endif # SYMLINKS_INCLUDED
