include new-rules/all-rules.mk

VERSION:=$(shell cat version)

ifeq (1,$(IS_WINDOWS))

ARCHIVE_EXT:=zip
ARCHIVE_CMD:=zip -r

else

# COPYFILE_DISABLE for MacOSX, see bug 64446 and
# http://superuser.com/questions/61185/why-do-i-get-files-like-foo-in-my-tarball-on-os-x/
# Should be innocuous on other platforms.
#
# -h = deref symlinks

ARCHIVE_EXT:=tar.gz
ARCHIVE_CMD:=COPYFILE_DISABLE=1 tar -zhcf

endif

ifneq ($(USE_HOST_COMPILER),0)
 ifneq ($(filter macosx,$(mc_platform)),)
  NM_CMD=nm -n
 else
  NM_CMD=nm -n -C
 endif
else
 ifneq ($(filter aix,$(mc_platform)),)
  NM_CMD=/usr/bin/nm -p -v -x -C
 else
  NM_CMD=$(PLATFORM_PACKAGES_DIR)/bin/nm -n -C
 endif
endif

ifneq ($(USE_HOST_COMPILER),0)
  STRIP_BIN_CMD=/usr/bin/strip
  STRIP_SHLIB_CMD=/usr/bin/strip -x
else ifeq ($(mc_platform),aix)
  STRIP_BIN_CMD=/usr/bin/strip
  STRIP_SHLIB_CMD=/usr/bin/strip -x
else
  STRIP_BIN_CMD=$(PLATFORM_PACKAGES_DIR)/bin/strip
  STRIP_SHLIB_CMD=$(PLATFORM_PACKAGES_DIR)/bin/strip -x
endif

ifeq ($(mc_platform),mingw)
  RELEASE_PLATFORM:=win32
else
  ifeq ($(mc_platform),mingw64)
  RELEASE_PLATFORM:=win64
  else
  RELEASE_PLATFORM:=$(mc_platform)
  endif
endif

# The name of the directory that the final archive will unpack itself
# into.
RELEASE_NAME:=prevent-$(RELEASE_PLATFORM)-$(VERSION)

# The final archive file itself.
RELEASE_FILE:=$(OBJ_DIR)/$(RELEASE_NAME).$(ARCHIVE_EXT)

# The QA archive.  It's like the release archive, except it contains
# developer licenses and unstripped binaries.
RELEASE_QA_FILE:=$(OBJ_DIR)/$(RELEASE_NAME)-qa.$(ARCHIVE_EXT)

# The SN archive.  Contains source released only to Sony licensees.
RELEASE_SN_FILE:=$(OBJ_DIR)/sn-$(VERSION).$(ARCHIVE_EXT)
SN_DIR=config/templates/sn

# The SN PS4 archive. Contains source released only to Sony PS4
# licensees.
RELEASE_SN_PS4_FILE:=$(OBJ_DIR)/sn-ps4-$(VERSION).$(ARCHIVE_EXT)
SN_PS4_DIR=config/templates/snps4

# The directory that will be created for assembling the release image
# prior to archiving.
RELEASE_TMP_DIR:=$(OBJ_DIR)/$(RELEASE_NAME)

# The name of the directory that the "LGPL relink distribution"
# will unpack itself into.
LGPL_RELEASE_NAME:=cov-lgpl-$(RELEASE_PLATFORM)-$(VERSION)

# The final archive file for the "LGPL relink distribution"
LGPL_RELEASE_FILE:=$(OBJ_DIR)/$(LGPL_RELEASE_NAME).$(ARCHIVE_EXT)

# The directory to be created for assembling the "LGPL relink distribution"
# prior to archiving.
LGPL_TMP_DIR :=$(OBJ_DIR)/$(LGPL_RELEASE_NAME)

# Subdirectory where jprevent is.
JPREVENT_DIR:=jprevent

# Subdirectory where prevent.net is.
DOTNET_DIR:=prevent.net

# Subdirectory where symbols are saved
SYMBOL_TMP:= symbols
SYMBOL_NAME:= prevent-$(RELEASE_PLATFORM)-$(VERSION)-symbols.$(ARCHIVE_EXT)

.PHONY: release lgpl

lgpl: $(LGPL_RELEASE_FILE)

release: $(LGPL_RELEASE_FILE) $(RELEASE_FILE)

ifeq (1,$(IS_WINDOWS))
BIN_WILDCARD:=*.exe
else
BIN_WILDCARD:=*
endif

CPR_CMD:=cp -r

$(RELEASE_FILE): all
	@# Create the release image directory.
	rm -rf $(RELEASE_TMP_DIR)
	mkdir -p $(RELEASE_TMP_DIR)
	@#
	@# Copy most of the files into it.  Explicitly name the things
	@# that *do* go into the release, rather than looping and
	@# excluding, to make it less likely that something unintentional
	@# gets shipped.
	@#
	@# Note that the developer license files are among the files
	@# copied during this stage.
	@# There may not be a jre if no-java or on macos
	if [ -d $(JRE7_DIR) ]; then \
	  $(CPR_CMD) $(JRE7_DIR) $(RELEASE_TMP_DIR);\
	fi
	if [ -d $(JRE8_DIR) ]; then \
	  $(CPR_CMD) $(JRE8_DIR) $(RELEASE_TMP_DIR);\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/findbugs ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/findbugs $(RELEASE_TMP_DIR);\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/findbugs-ext ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/findbugs-ext $(RELEASE_TMP_DIR);\
	fi
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/bin $(RELEASE_TMP_DIR)
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/certs $(RELEASE_TMP_DIR)
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/config $(RELEASE_TMP_DIR)
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/doc $(RELEASE_TMP_DIR)
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/dtd $(RELEASE_TMP_DIR)
	if [ -d $(RELEASE_ROOT_DIR)/sdk ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/sdk $(RELEASE_TMP_DIR)/.;\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/scripts ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/scripts $(RELEASE_TMP_DIR)/.;\
	fi
	@#mkdir $(RELEASE_TMP_DIR)/test-bin
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/test-bin $(RELEASE_TMP_DIR)/.
	@#cp $(RELEASE_ROOT_DIR)/test-bin/create-version-file $(RELEASE_TMP_DIR)/test-bin/.
	$(STRIP_BIN_CMD) $(RELEASE_TMP_DIR)/test-bin/create-version-file*
	@#cp $(RELEASE_ROOT_DIR)/test-bin/sqlite $(RELEASE_TMP_DIR)/test-bin/.
	$(STRIP_BIN_CMD) $(RELEASE_TMP_DIR)/test-bin/sqlite*
	if [ -e $(RELEASE_ROOT_DIR)/test-bin/unmangler_$(mc_platform).dll ] ; then \
		cp $(RELEASE_ROOT_DIR)/test-bin/unmangler_$(mc_platform).dll $(RELEASE_TMP_DIR)/test-bin/.;\
		$(STRIP_SHLIB_CMD) $(RELEASE_TMP_DIR)/test-bin/unmangler_$(mc_platform).dll;\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/test-data ]; then \
		$(CPR_CMD) $(RELEASE_ROOT_DIR)/test-data $(RELEASE_TMP_DIR)/.;\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/jars ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/jars $(RELEASE_TMP_DIR)/.;\
	fi
	if [ -d $(RELEASE_ROOT_DIR)/library ]; then \
	  $(CPR_CMD) $(RELEASE_ROOT_DIR)/library $(RELEASE_TMP_DIR);\
        fi
	$(CPR_CMD) $(RELEASE_ROOT_DIR)/xsl $(RELEASE_TMP_DIR)
	@#

	@# copy doc files to the release image

	if [ -d $(DOC_REP) ] && [ -d $(DOC_REP)/release_prevent ]; then \
	   $(CPR_CMD) $(DOC_REP)/release_prevent/* $(RELEASE_TMP_DIR)/.;\
	fi

	@# generate zip of checker examples for docs to grab as per bug 60961
	@# only needed on one platform, might as well use linux64.
	if [ "$(mc_platform)" = "linux64" ]; then \
	  cd analysis/checkers/examples/ && $(MAKE) zip; \
	fi

	@#   
	@# Copy in the Java release files. moved to here.
	if [ -d $(JPREVENT_DIR)/library ] ; then \
	  $(CPR_CMD) $(JPREVENT_DIR)/* $(RELEASE_TMP_DIR);\
	fi
	@#   
	@# Copy in the dotnet release files. moved to here.
	if [ -d $(DOTNET_DIR) ] ; then \
	  $(CPR_CMD) $(DOTNET_DIR)/* $(RELEASE_TMP_DIR);\
	fi
	@#   
	@# Create components.xml, VERSION and VERSION.XML.
	$(TEST_BIN_DIR)/create-version-file $(RELEASE_TMP_DIR) $(RELEASE_NAME)
	@# Bug 54264, 54275.  include-fixed causes problems with the scenario-extend
	@# tests on the newer FreeBSD/NetBSD platforms
	if [ "$(mc_platform)" = "freebsd" -o "$(mc_platform)" = "freebsdv6" -o "$(mc_platform)" = "freebsd64" -o "$(mc_platform)" = "netbsd" -o "$(mc_platform)" = "netbsd64" ]; then \
	  rm -rf $(RELEASE_TMP_DIR)/sdk/compiler/lib/gcc/*/*/include-fixed; \
	fi
	@# Tar all that up to make the "QA" image.
	cd $(RELEASE_TMP_DIR)/.. && $(ARCHIVE_CMD) $(call normalize_path,$(RELEASE_QA_FILE)) $(RELEASE_NAME)
	@#
	@# Make a separate archive of the sn support, for separate release to
	@# Sony licensees.
	cd $(RELEASE_TMP_DIR) && $(ARCHIVE_CMD) $(call normalize_path,$(RELEASE_SN_FILE)) $(SN_DIR)
	@# Make a separate archive of the snps4 support, for separate release to
	@# Sony PS4 licensees.
	cd $(RELEASE_TMP_DIR) && $(ARCHIVE_CMD) $(call normalize_path,$(RELEASE_SN_PS4_FILE)) $(SN_PS4_DIR)

	@# Bug 30201.  We're not to release the sn directory
	rm -rf $(RELEASE_TMP_DIR)/$(SN_DIR)

	@# We're not to release the SN PS4 directory
	rm -rf $(RELEASE_TMP_DIR)/$(SN_PS4_DIR)
	@# We're not to release the open watcom support
	rm -rf $(RELEASE_TMP_DIR)/config/templates/openwatcom
	@# We're not to release flacc support
	@# Originally developed by Nik for Ericsson
	rm -rf $(RELEASE_TMP_DIR)/config/templates/flacc
	@# Remove the license files.
	rm -f $(RELEASE_TMP_DIR)/bin/.*sec* $(RELEASE_TMP_DIR)/bin/*.dat
	@# Remove the flexnet license.config file.
	rm -f $(RELEASE_TMP_DIR)/bin/license.config
	@# Remove help for unreleased binaries
	rm -f $(RELEASE_TMP_DIR)/doc/help/_*
	@# Remove the few remaining remnants of DM support
	@# This may be a hack, since in Davis it wasn't necessary to do
	@# this and I don't know why it is now (in Eureka).
	rm -f $(RELEASE_TMP_DIR)/bin/cov-install-gui*
	rm -f $(RELEASE_TMP_DIR)/bin/cov-manage-db*
	rm -f $(RELEASE_TMP_DIR)/bin/cov-query-db*
	@# Remove JPrevent obfuscate info
	rm -rf $(RELEASE_TMP_DIR)/obs
	@#
	@# Remove prevent.net debug info
	rm -f $(RELEASE_TMP_DIR)/bin/*.pdb
	@#
	@# Remove cov-run-eval
	rm -f $(RELEASE_TMP_DIR)/bin/cov-run-eval
	@# Remove cov-internal-dw/dm if this is AIX
	if [ "$(mc_platform)" = "aix" ]; then \
	  rm -f $(RELEASE_TMP_DIR)/bin/cov-internal-dm; \
	  rm -f $(RELEASE_TMP_DIR)/bin/cov-internal-dw; \
  fi
	@#
	@# get the symbol tables before running the strip
	mkdir -p $(OBJ_DIR)/$(SYMBOL_TMP)
	for i in $(RELEASE_TMP_DIR)/bin/$(BIN_WILDCARD); do \
		case $$i in \
			*cov-internal-thunk.sh) ;; \
			*cov-internal-trace.sh) ;; \
			*cov-ec-*.sh) ;; \
			*cov-ec-*.bat) ;; \
			*sample-license.config) ;; \
			*) $(NM_CMD) "$$i" > $$i.map && \
			   gzip -c $$i.map > $$i.map.gz;;\
                esac; \
	done
	rm -f $(RELEASE_TMP_DIR)/bin/*.map
	rm -rf $(RELEASE_TMP_DIR)/test-bin
	rm -rf $(RELEASE_TMP_DIR)/test-data
	mv $(RELEASE_TMP_DIR)/bin/*.map.gz $(OBJ_DIR)/$(SYMBOL_TMP)
	@# Strip the binaries.  Don't strip 2 of the flexnet executables for mingw
	@# because that renders them un-signable by osslsigncode v1.5.3 and later.
	for i in $(RELEASE_TMP_DIR)/bin/$(BIN_WILDCARD); do \
		case $$i in \
			*cov-internal-clang) ;; \
			*cov-internal-thunk.sh) ;; \
			*cov-internal-trace.sh) ;; \
			*cov-ec-*.sh) ;; \
			*cov-ec-*.bat) ;; \
			*cov-internal-fxcop-helper.exe) ;; \
			*cov-internal-tfs.exe) ;; \
			*OpenCover.Console.exe) ;; \
			*sample-license.config) ;; \
			*generate-flexnet-hostid*) ;; \
			*cov-run) ;; \
			*.dll) ;; \
            *lmgrd.exe | *lmutil.exe) ;; \
			*.so | *.dylib) $(STRIP_SHLIB_CMD) $$i || exit;; \
			*) $(STRIP_BIN_CMD) $$i || exit;; \
		esac; \
	done
	@# DR 21377 -- remove all Defect Manager 4.x material from release
	cd $(RELEASE_TMP_DIR) && \
        cd bin && \
        rm -f cov-clean-locks* cov-internal-httpd* mod*.so lib*.dll \
              cov-listen-db* cov-probe-gui* cov-clean-locks* && \
        cd ../config && \
        rm -f default-server.* httpd.conf.template mime.types

	@# Fix permissions
	-find $(RELEASE_TMP_DIR)/bin -type f -print0 | xargs -0 chmod a=rx
	-chmod 644 $(RELEASE_TMP_DIR)/bin/sample-license.config
	-chmod 644 $(RELEASE_TMP_DIR)/config/wrapper_escape.conf
	-chmod 644 $(RELEASE_TMP_DIR)/config/parse_warnings.conf.sample

	@# Update VERSION.XML with md5s of stripped binaries
	$(TEST_BIN_DIR)/create-version-file $(RELEASE_TMP_DIR) $(RELEASE_NAME)
	@#
	@# remove the JPREVENT_VERSION from release
	rm -f $(RELEASE_TMP_DIR)/JPREVENT_VERSION
	@# remove the DOTNET_VERSION from release
	rm -f $(RELEASE_TMP_DIR)/DOTNET_VERSION
	@# Create the final customer-release archive.
	cd $(RELEASE_TMP_DIR)/.. && $(ARCHIVE_CMD) $(call normalize_path,$@) $(RELEASE_NAME)
	@# Save off cov-generate-hostid separately
	-rm -f $(RELEASE_TMP_DIR)/../cov-generate-hostid-*
	-cp $(RELEASE_TMP_DIR)/bin/cov-generate-hostid $(RELEASE_TMP_DIR)/../cov-generate-hostid-$(RELEASE_PLATFORM)-$(VERSION)
	@# Save off VERSION
	cp $(RELEASE_TMP_DIR)/VERSION $(RELEASE_TMP_DIR)/../.
	@#
	@# Clean up.
	rm -rf $(RELEASE_TMP_DIR);


$(LGPL_RELEASE_FILE): all
	@# Create the image directory.
	rm -rf $(LGPL_TMP_DIR)
	mkdir -p $(LGPL_TMP_DIR)
	@#
	@# Copy binaries and create the Makefile
	cat $(LINK_COMMAND_LOG_PREFIX)_* \
	  | perl -Wall utilities/create-lgpl-build.pl $(LGPL_TMP_DIR) "$(CREATE_LIBRARY_ARCHIVE)"
	@#
	@# Add the readme
	cp utilities/create-lgpl-build.readme $(LGPL_TMP_DIR)/README
	@#
	@# Create components.xml, VERSION and VERSION.XML.
	mkdir $(LGPL_TMP_DIR)/version_tmp
	$(TEST_BIN_DIR)/create-version-file $(LGPL_TMP_DIR)/version_tmp $(LGPL_RELEASE_NAME)
	@# But only save VERSION
	mv $(LGPL_TMP_DIR)/version_tmp/VERSION $(LGPL_TMP_DIR)
	rm -rf $(LGPL_TMP_DIR)/version_tmp
	@#
	@# Recreate the internal-version.a file.  Add "LGPL" to the end of all versions.
	sed -e 's/";/ LGPL_DISTRIBUTION";/g' < libs/internal-version/version.cpp \
	    > $(LGPL_TMP_DIR)/version.cpp
	mv $(LGPL_TMP_DIR)/version.cpp libs/internal-version/version.cpp
	cd libs/internal-version && $(MAKE)
	mv $(OBJ_DIR)/libs/libinternal-version.a $(LGPL_TMP_DIR)/lib
	@# Put back the original version.cpp - it is generated by the makefiles
	rm libs/internal-version/version.cpp
	cd libs/internal-version && $(MAKE)
	@#
	@# Create the final archive
	rm -f $@
	cd $(LGPL_TMP_DIR)/.. && $(ARCHIVE_CMD) $(call normalize_path,$@) $(LGPL_RELEASE_NAME)
	@#
	@# Clean up.
	rm -rf $(LGPL_TMP_DIR);

# EOF
