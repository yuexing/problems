# Current dir should be repo root
# Include symlinks.mk to copy files
include symlinks.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

# 1) Rules for extracting FindBugs itself into release dir

$(RELEASE_ROOT_DIR)/findbugs/lib/findbugs.jar: packages/findbugs/lib/findbugs.jar
	@# First checking for files with problematic licenses
	cd packages/findbugs && ../../findbugs-integration/scripts/oslicvio.pl
	@# Ok, now clean up old files, copy, and atomically move into place
	rm -rf $(RELEASE_ROOT_DIR)/findbugs{,-tmp}
	cp -pR packages/findbugs $(RELEASE_ROOT_DIR)/findbugs-tmp
	mv $(RELEASE_ROOT_DIR)/findbugs-tmp $(RELEASE_ROOT_DIR)/findbugs

check-and-copy-findbugs: $(RELEASE_ROOT_DIR)/findbugs/lib/findbugs.jar


# 2) Rules for creating necessary directories

FINDBUGS_EXT_DIR:=$(RELEASE_ROOT_DIR)/findbugs-ext

# Already defined in symlinks.mk
#$(FINDBUGS_EXT_DIR):
#	mkdir -p $@

# 3) Rules for building the FindBugs wrapper, jaring into release dir

FINDBUGS_EXT_ANT:=$(ANT) -Dfindbugs.ext=../../$(FINDBUGS_EXT_DIR) -Dobject.directory=../../$(OBJ_DIR)/objs -Dpackages.directory=../../packages

build-findbugs-wrapper: $(FINDBUGS_EXT_DIR)
	$(FINDBUGS_EXT_ANT) -f findbugs-integration/wrapper/build.xml


# 4) Rules for building the CovLStr plugin for FindBugs

FB_COVLSTR_CONVERT_OBJS:=$(OBJ_DIR)/objs/fb-covlstr-convert
FB_COVLSTR_CONVERT_SRCS:=$(wildcard findbugs-integration/i18n/fb-covlstr-convert/*.java)

FB_COVLSTR_PLUGIN_OBJS:=$(OBJ_DIR)/objs/fb-covlstr-plugin

$(FINDBUGS_EXT_DIR)/covlstr-plugin.jar: $(FINDBUGS_EXT_DIR) $(FB_COVLSTR_CONVERT_SRCS) $(wildcard findbugs-integration/coverity-info/output/*_precovlstr.*)
	@# Clean up first
	rm -rf $(FB_COVLSTR_CONVERT_OBJS) $(FB_COVLSTR_PLUGIN_OBJS) $(FB_COVLSTR_PLUGIN_OBJS).jar
	@# Build the converter
	mkdir -p $(FB_COVLSTR_CONVERT_OBJS)
	$(JAVAC) -d $(FB_COVLSTR_CONVERT_OBJS) $(FB_COVLSTR_CONVERT_SRCS)
	@# Use converter to generate plugin data files
	mkdir -p $(FB_COVLSTR_PLUGIN_OBJS)/edu/umd/cs/findbugs
	$(JAVA) -classpath $(FB_COVLSTR_CONVERT_OBJS) com.coverity.findbugs.util.Convert -m xml -i findbugs-integration/coverity-info/output/messages_precovlstr.xml -o $(FB_COVLSTR_PLUGIN_OBJS)/messages_covlstr.xml
	$(JAVA) -classpath $(FB_COVLSTR_CONVERT_OBJS) com.coverity.findbugs.util.Convert -m props -i findbugs-integration/coverity-info/output/FindBugsAnnotationDescriptions_precovlstr.properties -o $(FB_COVLSTR_PLUGIN_OBJS)/edu/umd/cs/findbugs/FindBugsAnnotationDescriptions_covlstr.properties
	@# Jar it up
	cd $(FB_COVLSTR_PLUGIN_OBJS); $(JAR) cf ../fb-covlstr-plugin.jar *
	cp $(FB_COVLSTR_PLUGIN_OBJS).jar $@

build-findbugs-covlstr-plugin: $(FINDBUGS_EXT_DIR)/covlstr-plugin.jar

# 5) Checker references
.PHONY: copy-findbugs-checker-ref

$(RELEASE_ROOT_DIR)/doc/en/findbugs/fb_checker_ref.html: findbugs-integration/coverity-info/output/checkers_fbref.html
	mkdir -p $(RELEASE_ROOT_DIR)/doc/en/findbugs/
	cat $< > $@
$(RELEASE_ROOT_DIR)/doc/ja/findbugs/fb_checker_ref.html: findbugs-integration/coverity-info/output/checkers_fbref_ja.html
	mkdir -p $(RELEASE_ROOT_DIR)/doc/ja/findbugs/
	cat $< > $@

copy-findbugs-checker-ref: \
  $(RELEASE_ROOT_DIR)/doc/en/findbugs/fb_checker_ref.html \
  $(RELEASE_ROOT_DIR)/doc/ja/findbugs/fb_checker_ref.html

# Add all of these to "all"

all: check-and-copy-findbugs build-findbugs-wrapper build-findbugs-covlstr-plugin copy-findbugs-checker-ref

clean::
	rm -rf $(FB_COVLSTR_PLUGIN_OBJS) $(FB_COVLSTR_CONVERT_OBJS) $(RELEASE_ROOT_DIR)/findbugs*
	$(FINDBUGS_EXT_ANT) -f findbugs-integration/wrapper/build.xml clean

# To be run for i18n on each upgrade
.PHONY: generate-findbugs-po
generate-findbugs-po:
	$(TEST_BIN_DIR)/findbugs-helper -d findbugs-integration generate-po $(FB_COVLSTR_PLUGIN_OBJS)/messages_covlstr.xml $(FB_COVLSTR_PLUGIN_OBJS)/edu/umd/cs/findbugs/FindBugsAnnotationDescriptions_covlstr.properties

endif # NO_JAVA
