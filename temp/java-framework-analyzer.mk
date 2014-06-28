include new-rules/all-rules.mk

ifeq ($(NO_JAVA),)

FD_MVN_REPO_SRC:=$(top_srcdir)/$(PACKAGES_DIR)/framework-analyzer/maven-repo
FD_MVN_SETTINGS:=$(top_srcdir)/$(PACKAGES_DIR)/framework-analyzer/maven-repo/settings.xml
FD_ANDROID_LIBS:=$(top_srcdir)/$(PACKAGES_DIR)/framework-analyzer/android-libs


# Build and install dir.
FD_BUILD_DIR:=$(top_srcdir)/$(OBJ_DIR)/objs/java-framework-analyzer-jar
FD_OUT_DIR:=$(top_srcdir)/$(OBJ_DIR)/root/jars


FD_MVN_REPO:=$(FD_BUILD_DIR)/maven-repo
TEST_OUTPUT:=$(FD_BUILD_DIR)/test_output


# The workflow is: cygwin -> cmd -> bat -> java and that doesn't play
# well with Windows and path. The workaround is to first convert the
# path into a windows path (for cmd/git coJava), then double escape the \ .
ifeq ($(IS_WINDOWS),1)
	FD_MVN_REPO_SRC:=$(subst \,\\,$(shell cygpath -w $(FD_MVN_REPO_SRC)))
	FD_MVN_REPO:=$(subst \,\\,$(shell cygpath -w $(FD_MVN_REPO)))
	FD_MVN_SETTINGS:=$(subst \,\\,$(shell cygpath -w $(FD_MVN_SETTINGS)))
	FD_ANDROID_LIBS:=$(subst \,\\,$(shell cygpath -w $(FD_ANDROID_LIBS)))
	FD_BUILD_DIR:=$(subst \,\\,$(shell cygpath -w $(FD_BUILD_DIR)))
	FD_OUT_DIR:=$(subst \,\\,$(shell cygpath -w $(FD_OUT_DIR)))
	TEST_OUTPUT:=$(subst \,\\,$(shell cygpath -w $(TEST_OUTPUT)))
endif

# Force offline mode, and set the local repository
LOCAL_MVN_OPTS:=-o -q \
				-Dmaven.repo.local=$(FD_MVN_REPO) \
				-Dlocal_repo=$(FD_MVN_REPO) \
				-Dandroid_libs_dir=$(FD_ANDROID_LIBS) \
				-Dprevent_fd_build_dir=$(FD_BUILD_DIR) \
				-Pprevent_build_directory

JAVA_FD_POM:=java-framework-analyzer/framework-analyzer/pom.xml

FD_MVN_REPO_EXISTS:=$(shell if test -e $(FD_MVN_REPO) ; then echo yes ; else echo no ; fi)

# Copy the maven repo in the objs/java-framework-analyzer directory
# so that we don't pollute the packages dir w/ modified files
ifeq ($(FD_MVN_REPO_EXISTS),no)
prepare-local-maven-repo:
	@mkdir -p $(FD_BUILD_DIR)
	@mkdir -p $(FD_OUT_DIR)
	@cp -rf $(FD_MVN_REPO_SRC) $(FD_MVN_REPO)
else
prepare-local-maven-repo:
	echo "Maven repo exists in $(FD_MVN_REPO)"

endif

java-framework-analyzer-build: prepare-local-maven-repo
	$(MVN) -s $(FD_MVN_SETTINGS) $(LOCAL_MVN_OPTS) -f $(JAVA_FD_POM) -Dmaven.test.skip=true package
	$(JAVA) -Xms256m -Dapple.awt.UIElement=true -Djava.awt.headless=true -jar $(FD_BUILD_DIR)/framework-analyzer-analysis.jar -d $(FD_BUILD_DIR)/tmp_list_plugins -lp $(FD_BUILD_DIR)/framework_analyzer_plugin_list.txt
	$(JAR) uf $(FD_BUILD_DIR)/framework-analyzer-analysis.jar -C $(FD_BUILD_DIR) framework_analyzer_plugin_list.txt
	@rm -rf $(FD_BUILD_DIR)/tmp_list_plugins $(FD_BUILD_DIR)/framework_analyzer_plugin_list.txt
	@cp -f $(FD_BUILD_DIR)/framework-analyzer-analysis.jar $(FD_OUT_DIR)


all: java-framework-analyzer-build


clean:: prepare-local-maven-repo
	$(MVN) -s $(FD_MVN_SETTINGS) $(LOCAL_MVN_OPTS) -f $(JAVA_FD_POM) clean


run-tests:: prepare-local-maven-repo
	@rm -f $(TEST_OUTPUT)
	$(MVN) -s $(FD_MVN_SETTINGS) $(LOCAL_MVN_OPTS) -s $(FD_MVN_SETTINGS) -f $(JAVA_FD_POM) test &> $(TEST_OUTPUT)
	# ghetto tests to check Maven tests worked properly
	test `grep '<<< FAILURE!' $(TEST_OUTPUT) | wc -l` -eq 0
	test `grep 'BUILD FAILURE' $(TEST_OUTPUT) | wc -l` -eq 0

else  # NO_JAVA = 1
run-tests::

endif
