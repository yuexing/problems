# packages.mk
# rules to build third-party packages

ifeq (,$(PACKAGES_INCLUDED))
PACKAGES_INCLUDED:=1

include new-rules/all-rules.mk

.PHONY:packages build-openssl build-apache

# This rule only builds those packages that must be done
# as part of every build. For those in CVS, their rules must
# be invoked explicitly
all:packages

# ----------------------------- full install -------------------------
# -- stuff that is not part of a emit-only install --
ifneq ($(mc_platform),aix)

# ----------------------------- openssl -------------------------
# -- stuff that is not part of the normal build --
# openssl in its installdir
INTERMEDIATE_SSL := $(OBJ_DIR)/packages/ssl/lib/libssl.a

# Go to "packages" and run make, with flags removed
# We don't want to pass e.g. -j as it's doubtful the packages build would support it
$(INTERMEDIATE_SSL):
	MAKEFLAGS= $(MAKE) -f packages/Makefile $@

# binary-packages-style tarball
SSL_PACKAGES_TARBALL := $(mc_platform)-packages/openssl.tar.gz

$(SSL_PACKAGES_TARBALL): $(INTERMEDIATE_SSL)
	(cd $(OBJ_DIR)/packages && tar cvf - ssl) | gzip -c > $@

# This target will rebuild openssl from source, and put a
# binary-packages-style tarball into $(mc_platform)-packages.  It is
# meant to be invoked when something in OpenSSL changes.  It must be
# done once per platform.
build-openssl: $(SSL_PACKAGES_TARBALL)

# ----------------------------- apache -------------------------
# the httpd we want to ship
COV_INTERNAL_HTTPD:=bin/cov-internal-httpd$(EXE_POSTFIX)

# ---- unix ----
ifneq ($(IS_WINDOWS),1)

# DR 6758: Let's just put 'httpd' into the binary packages
# and pull it from there.
HTTPD := $(mc_platform)-packages/bin/httpd$(EXE_POSTFIX)

# -- stuff that is not part of the normal build --
# httpd in its installdir
INTERMEDIATE_HTTPD := $(OBJ_DIR)/packages/apache/install/usr/local/apache2/bin/httpd$(EXE_POSTFIX)

# Go to "packages" and run make, with flags removed
# We don't want to pass e.g. -j as it's doubtful the packages build would support it
#
# I go ahead and strip the executable at this stage because we
# are unlikely to need symbols for apache, and it reduces the
# size by more than half.
$(INTERMEDIATE_HTTPD):
	MAKEFLAGS= $(MAKE) -f packages/Makefile $@
	strip $@

# binary-packages-style tarball
HTTPD_PACKAGES_TARBALL := $(mc_platform)-packages/apache2.tar.gz

# I just put the 'httpd' executable into the tarball because as far
# as I can tell that is all we need, and is one tenth the total size
# of the Apache install dir (even less after stripping).
$(HTTPD_PACKAGES_TARBALL): $(INTERMEDIATE_HTTPD)
	(cd $(OBJ_DIR)/packages/apache/install/usr/local/apache2 && tar cvf - bin/httpd$(EXE_POSTFIX)) | gzip -c > $@

# This target will rebuild apache/openssl from source, and put a
# binary-packages-style tarball into $(mc_platform)-packages.  It is
# meant to be invoked when something in apache changes.  It must be
# done one per platform.
build-apache: $(HTTPD_PACKAGES_TARBALL)


# -- END: stuff that is not part of the normal build --


# ---- Windows ----
# A custom Apache is not yet built on Windows, use the binaries from the repository
else

# Hack to let a target of 'objs/<platform>/bin/cov-internal-httpd'
# work on Windows and Unix (used in scripts/build_dm.sh for offshore development)
COV_INTERNAL_HTTPD_NO_EXT:=$(RELEASE_ROOT_DIR)/bin/cov-internal-httpd
$(COV_INTERNAL_HTTPD_NO_EXT): $(RELEASE_ROOT_DIR)/$(COV_INTERNAL_HTTPD)
.PHONY: $(COV_INTERNAL_HTTPD_NO_EXT)

HTTPD:=gui/bin/mingw/httpd$(EXE_POSTFIX)

packages: build-apache

build-apache:
	MAKEFLAGS= $(MAKE) -f packages/Makefile build-apache

WINHTTPD_FILES:=\
$(wildcard gui/bin/mingw/*.dll) \
$(wildcard gui/bin/mingw/*.so) \

# There is no win64 apache binary, don't copy any apache files over on that platform
ifneq ($(mc_platform),mingw64)
$(foreach file,$(WINHTTPD_FILES),$(eval $(call setup_file_copy_2_dirs,$$(dir $(file)),bin/,$$(notdir $(file)))))
endif

endif
# ---- end Windows/unix ----

# There is no win64 apache binary, don't copy it over on that platform
ifneq ($(mc_platform),mingw64)
$(eval $(call setup_file_copy_2_dirs,$(HTTPD),$(COV_INTERNAL_HTTPD),,))
endif

# ----------------------------- cyassl -------------------------
# Include ctaocrypt test tool; causes the library to be built too.
SOURCE_DIR:=packages/cyassl
include packages/cyassl/prog.mk
include $(CREATE_PROG)

endif
# ---- end full install ----

# ----------------------------- sqlite -------------------------
# Include sqlite shell tool; causes the library to be built too.
SOURCE_DIR:=packages/sqlite
include packages/sqlite/prog.mk
include $(CREATE_PROG)


# ----------------------------- bison -------------------------
# Bison tool
SOURCE_DIR:=packages/bison
include packages/bison/prog.mk
include $(CREATE_PROG)


# ----------------------------- flex --------------------------
SOURCE_DIR:=packages/flex
include packages/flex/prog.mk
include $(CREATE_PROG)


# ----------------------------- apache ant --------------------

# Do not copy with "NO_JAVA"
ifeq ($(NO_JAVA),)

$(eval $(call setup_file_copy_2_dirs,packages/apache-ant/1.8.2/lib/,jars/,$$(notdir ant.jar)))

endif
# ---- end NO_JAVA ----


# ----------------------------- jasper ------------------------

# Do not copy with "NO_JAVA"
ifeq ($(NO_JAVA),)

JASPER_FILES:=\
packages/tomcat7/lib/jstl-1.2.jar \
packages/apache-maven/3.1.1/lib/slf4j-simple-1.7.5.jar \
packages/apache-maven/3.1.1/lib/slf4j-api-1.7.5.jar \
packages/framework-analyzer/maven-repo/org/python/jython-standalone/2.7-b1/jython-standalone-2.7-b1.jar \
packages/framework-analyzer/maven-repo/com/google/guava/guava/15.0/guava-15.0.jar

$(foreach file,$(JASPER_FILES),$(eval $(call setup_file_copy_2_dirs,$$(dir $(file)),jars/jasper/,$$(notdir $(file)))))

endif
# ---- end NO_JAVA ----


endif
# EOF
