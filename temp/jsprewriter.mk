include new-rules/all-rules.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

BASE_PATH:=$(top_srcdir)/packages/tomcat7-source-deps
PREVENT_TOP:=$(top_srcdir)

TOMCAT_VERSION:=7.0.53
TOMCAT_SRC_ARCHIVE:=$(top_srcdir)/packages/apache-tomcat-$(TOMCAT_VERSION)-src.tgz
TOMCAT_SRC_PATCH:=$(top_srcdir)/utilities/jsprewriter/tomcat-src.patch
TOMCAT_BUILD_DIR:=$(OBJ_DIR)/jasper/apache-tomcat-$(TOMCAT_VERSION)-src

ANT_FILE:=$(TOMCAT_BUILD_DIR)/build.xml
JAVAC_SOURCE_ARG=source=\"\$${compile.source}\"
JAVAC_BOOTCLASSPATH_ARG=bootclasspath=\"\$${compile.bootclasspath}\"

ifeq ($(IS_WINDOWS),1)
	WIN_JRE_RT_JAR:=$(subst \,\\,$(shell cygpath -w $(top_srcdir)/packages/jre/1.6/rt.jar))
	WIN_TOOLS_JAR:=$(subst \,\\,$(shell cygpath -w $(top_srcdir)/packages/jre/1.6/tools.jar))
	WIN_JDK_RT_JAR:=$(subst \,\\,$(shell cygpath -w $(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk/jre/lib/rt.jar))
	BOOT_CLASSPATH:=
	BOOT_CLASSPATH:="$(WIN_JRE_RT_JAR):$(WIN_TOOLS_JAR):$(WIN_JDK_RT_JAR)"
	PREVENT_TOP:=$(subst \,\\,$(shell cygpath -w $(top_srcdir)))
	BASE_PATH:=$(subst \,\\,$(shell cygpath -w $(top_srcdir)/packages/tomcat7-source-deps))
else
	BOOT_CLASSPATH:=$(top_srcdir)/packages/jre/1.6/rt.jar:$(top_srcdir)/packages/jre/1.6/tools.jar:$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/jdk/jre/lib/rt.jar
endif

include utilities/jsprewriter/deps.mk

# Q: Why are we going to this trouble, instead of just checking in our
# modified Tomcat?
# A: We have to frequently update the Tomcat version (to keep pace with
# the latest JSP and EL versions).  This is an attempt to keep the
# divergence from the distribution to a minimum and to make it easy
# to perform an upgrade.

$(RELEASE_JARS_DIR)/jasper/jasper.jar: $(addprefix utilities/jsprewriter/, $(JSPREWRITER_DEPS))
	# Unzip the pristine Tomcat source
	rm -rf $(OBJ_DIR)/jasper
	mkdir -p $(OBJ_DIR)/jasper
	cd $(OBJ_DIR)/jasper; tar xzf $(TOMCAT_SRC_ARCHIVE)

	# Apply Coverity patch to Tomcat source and copy extra Coverity files
	cd $(TOMCAT_BUILD_DIR); git apply --ignore-whitespace $(TOMCAT_SRC_PATCH)
	cd $(top_srcdir)/utilities/jsprewriter; zip -q -r ../../$(TOMCAT_BUILD_DIR)/temp.zip .
	cd $(TOMCAT_BUILD_DIR); unzip -q temp.zip

	# Fix Ant build.xml files to allow setting the bootclasspath
	# For every source= attribute to <javac>, add a bootclasspath= attribute
	find $(TOMCAT_BUILD_DIR) -name build.xml -exec \
		perl -p -i -e 's/$(JAVAC_SOURCE_ARG)/$(JAVAC_SOURCE_ARG) $(JAVAC_BOOTCLASSPATH_ARG)/g' {} \;

	# Build Jasper
	$(ANT) -f $(ANT_FILE) \
		-Dcompile.source=1.6 \
		-Dcompile.target=1.6 \
		-Dcompile.bootclasspath=$(BOOT_CLASSPATH) \
		-Dbase.path=$(BASE_PATH) \
		-Dprevent-top=$(PREVENT_TOP) \
		-Dcoverity.jasperLoc=$(RELEASE_JARS_DIR)/jasper/

	# Deploy Jasper
	mkdir -p $(RELEASE_JARS_DIR)/jasper/
	cp -f $(TOMCAT_BUILD_DIR)/output/build/lib/*.jar $(RELEASE_JARS_DIR)/jasper/
	cp -f $(TOMCAT_BUILD_DIR)/output/build/bin/*.jar $(RELEASE_JARS_DIR)/jasper/


# Temporary rule to clean out any old files from incremental/touchstone builds
# that still have a copy of an older ECJ JAR.
# XXX: delete me after... say... June 2014
cleanup-old-ecj:
	rm -f $(RELEASE_JARS_DIR)/jasper/ecj-3.7.2.jar

all: cleanup-old-ecj $(RELEASE_JARS_DIR)/jasper/jasper.jar

clean::
	$(ANT) -f $(ANT_FILE) clean
	@rm -f $(RELEASE_JARS_DIR)/jasper/jasper.jar

run-tests::

endif # NO_JAVA
