# 

# Add a target for the named cov-analyze library
#
# Arguments:
# NAME: name of the library. The target will be
# $(RELEASE_ROOT_DIR)/config/$(NAME).xmldb
#
# There's an automatic regular dependency on cov-analyze, and
# order-only dependencies on cov-emit, cov-translate, cov-make-library, gcc-config, license.dat, config
#
# SOURCE_DIR: Directory where sources will be found
#
# SOURCE_INSTALL_DIR: Directory where model sources that we ship should go
# relative to RELEASE_LIB_DIR. Setting this to nothing means nothing
# gets copied though
#
# EXTRA_FLAGS: Additional flags to cov-make-library (reset to empty)
#
# SOURCES: C/C++ sources for the library
# relative to SOURCE_DIR. May contain wildcards.
#
# SOURCES_JAVA: Java sources for the library
# relative to SOURCE_DIR
#
# SOURCES_JAVA_GENERATED: Generated Java sources for the library
# relative to GENSRC_DIR/model-sources/java
#
# EXTRA_FLAGS_JAVA: Additional flags to cov-emit-java invocations
#
# SOURCES_CSHARP: C# sources for the library
# relative to SOURCE_DIR
#
# EXTRA_FLAGS_CSHARP: Additional flags to cov-emit-cs invocations
#
# EXTRA_DEPS: List of extra dependencies, e.g. files #included by SOURCES
#
# Output: LIB_XMLDB = the target

#TODO: Add back in fail-on-recoverable-error to the LIB_IDIR_COMMAND later

include $(RULES)/gcc-config.mk

# We'll store the files in a temporary location, then merge them all
# together.
LIB_XMLDB:=$(OBJ_DIR)/make-library/$(NAME).xmldb

# Intermediate directory to store all emitted source files.
LIB_IDIR:=$(OBJ_DIR)/make-library/$(NAME)-idir

# Track dependency on source files which are #included from primary source
# files.
ifneq ($(EXTRA_DEPS),)
  define add_dep
    DEP:=$$(SOURCE_DIR)/$(value 1)
    $$(LIB_XMLDB): $$(DEP)
  endef
  $(foreach dep, $(EXTRA_DEPS), $(eval $(call add_dep,$(dep))))
endif

# While cov-make-library can compile many files in a single
# invocation, Windows imposes a limit (8191 chars) on the command line
# length.  Get around this by putting file names in a file.
SOURCE_LIST_FILE:=$(OBJ_DIR)/make-library/$(NAME)-sources.txt

# ============== C# ===================

ifneq ($(SOURCES_CSHARP),)

ifneq ($(filter linux64,$(mc_platform)),)
LIBSTDCPP_SO:=$(CSHARP_BIN_DIR)/mono/lib/libstdc++.so
else
LIBSTDCPP_SO:=
endif

$(LIB_XMLDB): $(COV_EMIT_CSHARP_DEPS) $(CSHARP_PRIMITIVES) $(LIBSTDCPP_SO)

endif # $(SOURCES_CSHARP) not empty

# ============== Java ===================

ifneq ($(SOURCES_JAVA),)

$(LIB_XMLDB): $(COV_EMIT_JAVA) $(JAVA_RT_JAR_IDIR_TGZ) | $(JAVA_PRIMITIVES) $(JAVA_ANNOTATIONS)

endif # $(SOURCES_JAVA) not empty

# ============== C/C++ ===================

ifneq ($(SOURCES),)

$(LIB_XMLDB): $(COV_EMIT)

endif # $(SOURCES) not empty

# All sources (relative to SOURCE_DIR, wildcards unexpanded)
ALL_SOURCES_RELATIVE:=$(SOURCES_CSHARP) $(SOURCES_JAVA) $(SOURCES)

# All generated sources, relative to $(GENSRC_DIR)/model-sources/
ALL_GENERATED_SOURCES_RELATIVE:=$(SOURCES_JAVA_GENERATED)

# All sources (relative to root, wildcards expanded)
ALL_SOURCES_EXPANDED:=$(foreach source, $(ALL_SOURCES_RELATIVE), $(wildcard $(SOURCE_DIR)/$(source)))

# All generated sources (relative to root, wildcards expanded)
ALL_GENERATED_SOURCES_EXPANDED:=$(foreach source, $(ALL_GENERATED_SOURCES_RELATIVE), $(wildcard $(GENSRC_DIR)/model-sources/$(source)))

# Only copy sources to library dir if this var is set
ifneq ($(SOURCE_INSTALL_DIR),)

# Install sources. These are files that should exist at the end.
INSTALL_SOURCES:=$(foreach source,$(ALL_SOURCES_EXPANDED),$(subst $(SOURCE_DIR),$(RELEASE_LIB_DIR)/$(SOURCE_INSTALL_DIR),$(source)))
# Generated model sources already have a "good" directory structure
INSTALL_SOURCES+=$(foreach source,$(ALL_GENERATED_SOURCES_EXPANDED),$(subst $(GENSRC_DIR)/model-sources/, $(RELEASE_LIB_DIR)/,$(source)))


# This is similar to setup_file_copy_2_dirs, but doesn't rely on that SYMLINK_FILES
# or get included everywhere for whatever reason
define install_model_source
MSOURCE:=$(value 1)$(value 3)
MTARGET:=$$(RELEASE_LIB_DIR)/$(value 2)$(value 3)
MTARGET_DIR:=$$(patsubst %/,%,$$(dir $$(MTARGET)))

ifeq ($$($$(MTARGET_DIR)),)
$$(MTARGET_DIR):=1
$$(MTARGET_DIR):
	mkdir -p "$$@"
endif

$$(MTARGET): $$(MSOURCE) | $$(MTARGET_DIR)
	$(COVCP) -f "$$<" "$$@"

endef # install_model_source

#Targets for each of the INSTALL_SOURCES
$(foreach source, $(ALL_SOURCES_EXPANDED), $(eval $(call install_model_source,$(SOURCE_DIR)/,$(SOURCE_INSTALL_DIR)/,$(subst $(SOURCE_DIR)/,,$(source)))))
$(foreach source, $(ALL_GENERATED_SOURCES_EXPANDED), $(eval $(call install_model_source,$(GENSRC_DIR)/model-sources/,,$(subst $(GENSRC_DIR)/model-sources/,,$(source)))))

endif # ifneq ($(SOURCE_INSTALL_DIR),)

$(SOURCE_LIST_FILE): SOURCE_DIR:=$(SOURCE_DIR)
$(SOURCE_LIST_FILE): ALL_SOURCES_RELATIVE:=$(ALL_SOURCES_RELATIVE)
$(SOURCE_LIST_FILE): ALL_GENERATED_SOURCES_EXPANDED:=$(ALL_GENERATED_SOURCES_EXPANDED)

# The order-only dependency assumes that the generated sources depend on
# the normal handwritten sources, which is true for now
$(SOURCE_LIST_FILE): $(ALL_SOURCES_EXPANDED) | $(ALL_GENERATED_SOURCES_EXPANDED)
	mkdir -p $(dir $@)
	@ # Note that "ALL_SOURCES_RELATIVE" may include wildcards.
	@ # Let the shell expand them; this allows a potentially smaller
	@ # command
	@ # This relies on "make" and the shell doing the same expansions,
	@ # which seems fairly safe for what we're doing.
	for wildcard in $(ALL_SOURCES_RELATIVE); do \
		for expanded in $(SOURCE_DIR)/$$wildcard; do \
			echo $$expanded; \
		done; \
	done > "$@"
	for file in $(ALL_GENERATED_SOURCES_EXPANDED) ; do \
		echo $$file; \
	done >> "$@"

$(LIB_XMLDB): $(SOURCE_LIST_FILE)

$(LIB_XMLDB): LIB_IDIR:=$(LIB_IDIR)
$(LIB_XMLDB): SOURCE_LIST_FILE:=$(SOURCE_LIST_FILE)
$(LIB_XMLDB): EXTRA_FLAGS:=$(EXTRA_FLAGS) --sanity-checks
$(LIB_XMLDB): $(LIB_IDIR_TARGET) $(COV_ANALYZE) \
| $(COV_MAKE_LIBRARY) $(COV_COLLECT_MODELS) \
$(DISTRIBUTOR_BINS) symlinks
	@ # cov-make-library always appends, so remove first
	rm -f "$@"
	$(COV_MAKE_LIBRARY) \
		--default --save-dir "$(LIB_IDIR)" \
		-of "$@" $(EXTRA_FLAGS) \
		@UTF-8@$(SOURCE_LIST_FILE)


ALL_TARGETS+=$(LIB_XMLDB)
LIBRARIES+=$(LIB_XMLDB)

all:$(LIB_XMLDB) $(INSTALL_SOURCES)

EXTRA_FLAGS:=
EXTRA_FLAGS_JAVA:=
EXTRA_FLAGS_CSHARP:=
SOURCES:=
SOURCES_JAVA:=
SOURCES_JAVA_GENERATED:=
SOURCES_CSHARP:=
EXTRA_DEPS:=
INSTALL_SOURCES:=
