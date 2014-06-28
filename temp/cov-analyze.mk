include new-rules/all-rules.mk

SOURCE_DIR:=analysis/cov-analyze

include $(SOURCE_DIR)/prog.mk $(CREATE_PROG)

CHECKER_MODULES:=generic security concurrency

.PHONY: library

LIBRARIES:=

# if COVERITY_OUTPUT is set (running in cov-build), don't build libraries
# this takes forever (cov-make-library uses whole prevent emit dir)
# Same if POP_BUILD is set (on Windows, environment of build is not
# changed by cov-build, so we add a special variable in the pop build)
POP_BUILD = $(if $(COVERITY_DISENGAGE_EXES),,$(COVERITY_OUTPUT))
ifeq (,$(POP_BUILD))
$(foreach module,$(CHECKER_MODULES),\
$(eval SOURCE_DIR:=analysis/checkers/$(module)/library) \
$(eval include $(SOURCE_DIR)/library.mk $(CREATE_ANALYSIS_LIB)) \
)

ifeq ($(NO_CSHARP),)

include cs-primitives.mk

SOURCE_DIR:=analysis/checkers/models/cs
include $(SOURCE_DIR)/library.mk $(CREATE_ANALYSIS_LIB)

endif # NO_CSHARP

ifeq ($(NO_JAVA),)

include java-primitives.mk

SOURCE_DIR:=analysis/checkers/models/java
SOURCE_INSTALL_DIR:=java
include $(SOURCE_DIR)/library.mk $(CREATE_ANALYSIS_LIB)

include model-db-vars.mk

.PHONY: check-model-db-schema-version

endif # NO_JAVA

check-model-db-schema-version:
	@if [ "$(MODEL_DB_EXPECTED_VERSION)" -ne \
		"$(MODEL_DB_REAL_VERSION)" ]; then \
		echo "Model DB schema version changed"; \
		echo "You may need to regenerate models, or set MODEL_DB_EXPECTED_VERSION in model-db-vars.mk to $(MODEL_DB_REAL_VERSION)"; \
		false; \
	fi


LIB_MODELS_DB_ARG:=
# Also not as dependencies
BUILTIN_MODEL_DB_PREREQS:=

# Should we use the built-in JRE models DB? (instead of generating it)
# If so, set this to 1.
USE_BUILTIN_JRE:=1

# Should we use the built-in Andorid models DB? (instead of generating it)
USE_BUILTIN_ANDROID:=1

# Should we use the built-in C# models DB? (instead of generating it)
USE_BUILTIN_CS_SYSTEM:=1

ifneq ($(GENERATE_LIB_MODELS),)

# Use built-in JRE but only if generating android models only.
USE_BUILTIN_JRE:=$(GENERATE_ANDROID_ONLY)
USE_BUILTIN_ANDROID:=
USE_BUILTIN_CS_SYSTEM:=

else

USE_BUILTIN_JRE:=1
USE_BUILTIN_ANDROID:=1
USE_BUILTIN_CS_SYSTEM:=1

endif # GENERATE_LIB_MODELS

SQLITE_SHELL:=$(TEST_BIN_DIR)/sqlite$(DOT_EXE)

ifeq ($(NO_JAVA),)

ifeq ($(USE_BUILTIN_ANDROID),1)

ANDROID_MODELS_DB_UPGRADED:=$(OBJ_DIR)/make-library/android-models.db

# Depend on model-db-vars.mk so that we upgrade when the
# MODEL_DB_EXPECTED_VERSION changes.
$(ANDROID_MODELS_DB_UPGRADED): $(ANDROID_MODELS_DB) model-db-vars.mk | \
	$(SQLITE_SHELL) check-model-db-schema-version
	mkdir -p `dirname "$@"`
	cp "$<" "$@"
	$(SQLITE_SHELL) "$@" "update version set version=$(MODEL_DB_EXPECTED_VERSION)"

LIB_MODELS_DB_ARG+= -if $(ANDROID_MODELS_DB_UPGRADED)
BUILTIN_MODEL_DB_PREREQS+= $(ANDROID_MODELS_DB_UPGRADED)

endif # USE_BUILTIN_ANDROID

ifeq ($(USE_BUILTIN_JRE),1)

JRE_MODELS_DB_UPGRADED:=$(OBJ_DIR)/make-library/jre-models.db

$(JRE_MODELS_DB_UPGRADED): $(JRE_MODELS_DB) model-db-vars.mk | \
	$(SQLITE_SHELL) check-model-db-schema-version
	mkdir -p `dirname "$@"`
	cp "$<" "$@"
	$(SQLITE_SHELL) "$@" "update version set version=$(MODEL_DB_EXPECTED_VERSION)"

LIB_MODELS_DB_ARG+= -if $(JRE_MODELS_DB_UPGRADED)
BUILTIN_MODEL_DB_PREREQS+= $(JRE_MODELS_DB_UPGRADED)

endif # USE_BUILTIN_JRE

endif # NO_JAVA

ifeq ($(NO_CSHARP),)

ifeq ($(USE_BUILTIN_CS_SYSTEM),1)

CS_SYSTEM_MODELS_DB_UPGRADED:=$(OBJ_DIR)/make-library/cs-system-models.db

$(CS_SYSTEM_MODELS_DB_UPGRADED): $(CS_SYSTEM_MODELS_DB) model-db-vars.mk | \
	$(SQLITE_SHELL) check-model-db-schema-version
	mkdir -p `dirname "$@"`
	cp "$<" "$@"
	$(SQLITE_SHELL) "$@" "update version set version=$(MODEL_DB_EXPECTED_VERSION)"

LIB_MODELS_DB_ARG+= -if $(CS_SYSTEM_MODELS_DB_UPGRADED)
BUILTIN_MODEL_DB_PREREQS+= $(CS_SYSTEM_MODELS_DB_UPGRADED)

endif # USE_BUILTIN_CS_SYSTEM

endif # NO_CSHARP

BUILTIN_MODEL_DB:=$(RELEASE_ROOT_DIR)/config/builtin-models.db

$(BUILTIN_MODEL_DB): LIBS_ARG:=$(foreach i,$(LIBRARIES),-if $i)

$(BUILTIN_MODEL_DB): $(LIBRARIES) $(BUILTIN_MODEL_DB_PREREQS)
	rm -f $@
	$(COV_COLLECT_MODELS) -of $@ $(LIBS_ARG) $(LIB_MODELS_DB_ARG)

ALL_TARGETS+=$(BUILTIN_MODEL_DB)

library: $(BUILTIN_MODEL_DB)

all: library

# dchai: Why is RELAX_GETLOCK disabled?
# chgros: Originally, this was because of a crash (since fixed, bug 50329)
# However, "getlock" models are not useful in our built-in models,
# because they will reference locks that are typically not visible
# to users anyway, so I think it's fine to keep it disabled.
LIB_DB_ANALYSIS_OPTIONS:=\
		--make-library-from-impl \
		--allow-library-duplicates \
		--fail-stop \
		--enable-derivers-for-modules all \
		-j auto \
		-n RELAX_GETLOCK \
		-co IFACE_TRACKER:force_no_this_escape_file:analysis/checkers/models/java/no-escape-classes

ifneq ($(USE_BUILTIN_JRE),1)

JRE_MODELS_DIR:=$(OBJ_DIR)/make-library/jre-models
JRE_MODELS_SOURCES_TARBALL:=packages/jre-models.tgz

# Separate compilation from analysis, because only analysis depends on
# other models.
JRE_MODELS_IDIR_STAMP:=$(JRE_MODELS_DIR)/idir-timestamp

$(JRE_MODELS_IDIR_STAMP): $(JRE_MODELS_SOURCES_TARBALL) $(COV_EMIT_JAVA)
	rm -rf $(JRE_MODELS_DIR)
	@ # Extract the sources and jars we're going to analyze
	tar -zxf $(JRE_MODELS_SOURCES_TARBALL) -C $(OBJ_DIR)/make-library
	@ # Find sources for cov-emit-java argument
	find $(JRE_MODELS_DIR) -name '*.java' > $(JRE_MODELS_DIR)/srcs
	@ # Compile all the sources
	$(COV_EMIT_JAVA) -n --dir $(JRE_MODELS_DIR)/dir \
	        --source=1.7 \
	        --javac-version=1.7 \
		--no-restart \
		--skip-emit-bootclasspath \
		--bootclasspath "$(JRE_MODELS_DIR)/lib/rt.jar" \
		--bootclasspath "$(JRE_MODELS_DIR)/lib/jsse.jar" \
		--bootclasspath "$(JRE_MODELS_DIR)/lib/jce.jar" \
	@UTF-8@$(JRE_MODELS_DIR)/srcs
	touch "$@"

# Built-in model DB (+ idir) is sufficient as a prerequisite because, when
# using this target, we always rebuild that DB (so e.g. no need to
# depend on cov-analyze)
# This depends on the C# models only to avoid running "-j auto" analysis
# more than once at one time.
$(JRE_MODELS_DB): $(JRE_MODELS_IDIR_STAMP) $(CS_SYSTEM_MODELS_DB) $(BUILTIN_MODEL_DB)
	@ # Analyze them, as a library
	$(COV_ANALYZE) --java --dir $(JRE_MODELS_DIR)/dir \
		$(LIB_DB_ANALYSIS_OPTIONS)
	rm -f "$@"
	@ # Collect all the models
	$(COV_COLLECT_MODELS) --java --dir $(JRE_MODELS_DIR)/dir -of "$@"
	@ # Now include these models into the built-in model DB, too,
	@ # before we move on to Android.
	$(COV_COLLECT_MODELS) -of $(BUILTIN_MODEL_DB) -if "$@"

endif # USE_BUILTIN_JRE

ifneq ($(USE_BUILTIN_ANDROID),1)

ANDROID_MODELS_DIR:=$(OBJ_DIR)/make-library/android-models
ANDROID_MODELS_SOURCES_TARBALL:=packages/android-models.tgz

ANDROID_MODELS_IDIR_STAMP:=$(ANDROID_MODELS_DIR)/idir-timestamp

$(ANDROID_MODELS_IDIR_STAMP): $(ANDROID_MODELS_SOURCES_TARBALL) $(COV_EMIT_JAVA)
	rm -rf $(ANDROID_MODELS_DIR)
	@ # Extract the sources and jars we're going to analyze
	tar -zxf $(ANDROID_MODELS_SOURCES_TARBALL) -C $(OBJ_DIR)/make-library
	@ # Compile all the sources
	$(MAKE) -C $(ANDROID_MODELS_DIR) \
		PREVENT_DIR=$(top_srcdir)/$(RELEASE_ROOT_DIR) \
		PACKAGES=$(call SHELL_TO_NATIVE,$(top_srcdir)/packages)
	touch "$@"

$(ANDROID_MODELS_DB): $(ANDROID_MODELS_IDIR_STAMP) $(JRE_MODELS_DB) $(BUILTIN_MODEL_DB)
	@ # Analyze Android sources, as a library
	$(COV_ANALYZE) --java --dir $(ANDROID_MODELS_DIR)/dir \
		$(LIB_DB_ANALYSIS_OPTIONS)
	rm -f "$@"
	@ # Collect all the models
	$(COV_COLLECT_MODELS) --java --dir $(ANDROID_MODELS_DIR)/dir -of "$@"
	@ # Now include these models into the built-in model DB, too,
	@ # so that it's up-to-date.
	$(COV_COLLECT_MODELS) -of $(BUILTIN_MODEL_DB) -if "$@"

endif # USE_BUILTIN_ANDROID

ifneq ($(USE_BUILTIN_CS_SYSTEM),1)

CS_SYSTEM_MODELS_DIR:=$(OBJ_DIR)/make-library/cs-system-models

CS_SYSTEM_MODELS_IDIR_STAMP:=$(CS_SYSTEM_MODELS_DIR)/idir-timestamp

$(CS_SYSTEM_MODELS_IDIR_STAMP): $(COV_EMIT_CSHARP)
	rm -rf $(CS_SYSTEM_MODELS_DIR)
	mkdir -p $(CS_SYSTEM_MODELS_DIR)
	@ # Create the dummy source file
	echo "namespace ignoreme { class ignoreme {} };" > $(CS_SYSTEM_MODELS_DIR)/Empty.cs
	@ # Compile the system referenced assemblies
	@ # We list the assemblies we want to ship directly, rather than relying on
	@ # transitive reference gathering. This list should match the
	@ # "shippedNames" list in csfe/libs/EmitDotNet/EmitDotNet.cs, which
	@ # is used to suppress emitting method bodies for these assemblies.
	$(COV_EMIT_CSHARP) --dir $(CS_SYSTEM_MODELS_DIR)/dir \
		--noconfig --nostdlib --no-out \
		--make-library \
		--disable-indirect-references \
		--reference "Accessibility.dll" \
		--reference "Microsoft.Build.Framework.dll" \
		--reference "Microsoft.Build.Tasks.v4.0.dll" \
		--reference "Microsoft.Build.Utilities.v4.0.dll" \
		--reference "Microsoft.CSharp.dll" \
		--reference "mscorlib.dll" \
		--reference "SMDiagnostics.dll" \
		--reference "System.dll" \
		--reference "System.ComponentModel.DataAnnotations.dll" \
		--reference "System.Configuration.dll" \
		--reference "System.Configuration.Install.dll" \
		--reference "System.Core.dll" \
		--reference "System.Data.dll" \
		--reference "System.Data.OracleClient.dll" \
		--reference "System.Data.SqlXml.dll" \
		--reference "System.Deployment.dll" \
		--reference "System.Design.dll" \
		--reference "System.DirectoryServices.dll" \
		--reference "System.DirectoryServices.Protocols.dll" \
		--reference "System.Drawing.dll" \
		--reference "System.Drawing.Design.dll" \
		--reference "System.Dynamic.dll" \
		--reference "System.EnterpriseServices.dll" \
		--reference "System.Net.dll" \
		--reference "System.Numerics.dll" \
		--reference "System.Runtime.Caching.dll" \
		--reference "System.Runtime.Remoting.dll" \
		--reference "System.Runtime.Serialization.dll" \
		--reference "System.Runtime.Serialization.Formatters.Soap.dll" \
		--reference "System.Security.dll" \
		--reference "System.ServiceModel.Internals.dll" \
		--reference "System.ServiceProcess.dll" \
		--reference "System.Transactions.dll" \
		--reference "System.Web.dll" \
		--reference "System.Web.ApplicationServices.dll" \
		--reference "System.Web.RegularExpressions.dll" \
		--reference "System.Web.Services.dll" \
		--reference "System.Windows.Forms.dll" \
		--reference "System.Xaml.dll" \
		--reference "System.Xml.dll" \
		$(CS_SYSTEM_MODELS_DIR)/Empty.cs
	touch "$@"

# Built-in model DB (+ idir) is sufficient as a prerequisite because, when
# using this target, we always rebuild that DB (so e.g. no need to
# depend on cov-analyze)
$(CS_SYSTEM_MODELS_DB): $(CS_SYSTEM_MODELS_IDIR_STAMP) $(BUILTIN_MODEL_DB)
	@ # Analyze them, as a library
	$(COV_ANALYZE) --cs --dir $(CS_SYSTEM_MODELS_DIR)/dir \
		$(LIB_DB_ANALYSIS_OPTIONS) --disable-rta \
		-co "RELAX_ALLOC:mark_disposable_ctors:true" \
		-co "RELAX_TA_ALLOC:mark_disposable_ctors:true"
	rm -f "$@"
	@ # Collect all the models
	$(COV_COLLECT_MODELS) --cs --dir $(CS_SYSTEM_MODELS_DIR)/dir -of "$@"
	@ # Now include these models into the built-in model DB, too.
	$(COV_COLLECT_MODELS) -of $(BUILTIN_MODEL_DB) -if "$@"

endif # USE_BUILTIN_CS_SYSTEM

# Generate a target , copy-$(1)-to-cvs, that will copy the DB in the variable
# names $(2), based on its md5sum, to "cvs", and update model-db-hashes.mk
# with the proper hashes.
define copy-model-db-to-cvs

update-$(1)-db-hash: $($(2))
	HASH=`md5sum $$^ | sed 's/^\([0-9a-f]*\).*$$$$/\1/'`; \
	sed "s/\($(2)_HASH:=\).*/\1$$$$HASH/" model-db-hashes.mk > \
		model-db-hashes2.mk; \
	mv model-db-hashes2.mk model-db-hashes.mk

copy-$(1)-db-to-cvs:
	HASH=`md5sum $($(2)) | sed 's/^\([0-9a-f]*\).*$$$$/\1/'`; \
	FILE_NAME=`basename $($(2))`-$$$$HASH; \
	if ! ssh $(PACKAGE_USER)@cvs.sf.coverity.com ls /home/coverity/java-models/ | grep -q -F "$$$$FILE_NAME"; then \
		scp $($(2)) $(PACKAGE_USER)@cvs.sf.coverity.com:/home/coverity/java-models/$$$$FILE_NAME; \
	else \
		echo "$$$$FILE_NAME is up-to-date"; \
	fi

endef

$(eval $(call copy-model-db-to-cvs,android,ANDROID_MODELS_DB))
$(eval $(call copy-model-db-to-cvs,jre,JRE_MODELS_DB))
$(eval $(call copy-model-db-to-cvs,cs-system,CS_SYSTEM_MODELS_DB))

android-models: testsuite-shipping-models-linux64-check update-android-db-hash
jre-models: testsuite-shipping-models-linux64-check update-jre-db-hash
cs-system-models: testsuite-shipping-models-linux64-check update-cs-system-db-hash

# BZ 38384
# Maintain an MD5 hash of custom (i.e. handwritten) models.  If the hash
# is inconsistent with updated models, do one of two things:
# - build machines: fail.  Remediation must be done on a developer machine.
# - other: trigger a rebuild of JRE/Android/C# models, and update hash.
# - We look at both Java and C# handwritten models for computing the hash.
custom-db-hashes.mk: $(OBJ_DIR)/make-library/java-models.xmldb $(OBJ_DIR)/make-library/cs-models.xmldb
	@# Notes:
	@# 1) I'm manually grepping through custom-db-hashes.mk because
	@# including it from a Makefile leads to annoying dependency relations.
	@# 2) cov-collect-models will fail if it sees a 0-byte file, so we need
	@# to remove temporary files first.
	@TEMP_JSON=$(call SHELL_TO_NATIVE,$(shell mktemp /tmp/tempjson-XXXXX)); \
	rm $$TEMP_JSON; \
	STORED_HASH=`grep CUSTOM_MODELS_DB_HASH $@ | sed 's/.*:=\([0-9a-f]*\)/\1/'`; \
	$(COV_COLLECT_MODELS) --json -of $$TEMP_JSON -if $< -if $(word 2,$^); \
	HASH=`md5sum $$TEMP_JSON | sed 's/^\([0-9a-f]*\).*$$/\1/'`; \
	rm $$TEMP_JSON; \
	if [ "x$$HASH" != "x$$STORED_HASH" ]; then \
	    echo Custom model DB hashes differ.; \
	    echo Stored hash: $$STORED_HASH; \
	    echo New hash: $$HASH; \
	    if [ "$${HOSTNAME#b-}" != "$$HOSTNAME" -a \
			-z "$(ALLOW_MODEL_REGEN_ON_BUILD_MACHINE)" ]; then \
	        echo This is a build machine.  Giving up.; \
		exit 1; \
	    elif [ -n "$(NO_AUTO_MODEL_REGEN)" ]; then \
	        echo Refusing to regenerate models due to NO_AUTO_MODEL_REGEN.; \
		exit 1; \
	    else \
	        echo Regenerating JRE/Android/C# models...; \
		$(MAKE) regenerate-models || exit 1; \
		sed "s/\(CUSTOM_MODELS_DB_HASH:=\).*/\1$$HASH/" custom-db-hashes.mk > \
			custom-db-hashes2.mk; \
		mv custom-db-hashes2.mk custom-db-hashes.mk; \
	    fi; \
	else \
	    echo Custom model DB hashes up-to-date.; \
	    touch $@; \
	fi

ifeq ($(ENABLE_BUILDING_LIB_MODELS),1)
 all: check-package-names-for-java-models custom-db-hashes.mk
endif

# Compare the path of our handwritten Java models to their package
# name in an attempt to avoid issues like Bug 55170.
.PHONY: check-package-names-for-java-models
check-package-names-for-java-models:
	perl -w analysis/checkers/models/java/check-models.pl

lib-models: testsuite-shipping-models-linux64-check jre-models android-models cs-system-models
	echo "# NO""CHECKIN -- need to do make copy-models" >> model-db-hashes.mk
	echo "You will not be able to checkin model-db-hashes.mk until you do make copy-models."

copy-models: copy-android-db-to-cvs copy-jre-db-to-cvs copy-cs-system-db-to-cvs
	egrep -v "NO""CHECKIN" model-db-hashes.mk > model-db-hashes-clean.mk;
	mv model-db-hashes-clean.mk model-db-hashes.mk


else

library:
	@echo "Library building disabled"

endif # POP_BUILD
