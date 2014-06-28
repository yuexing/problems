# pop.mk
# Rules to set up Prevent-on-prevent
# Relies on "POP_RELEASE", representing a prevent installation (absolute or
# relative to repo root) being set in personal.mk at toplevel.

ifneq ($(POP_RELEASE),)
HAS_POP:=1

# I want to reassign POP_RELEASE but if it's specified on the command
# line it doesn't work apparently.
POP_RELEASE_NO_OVERRIDE:=$(POP_RELEASE)

# Allow specifying a repository instead of an install
ifneq ($(wildcard $(POP_RELEASE_NO_OVERRIDE)/objs/$(mc_platform)/root/bin/cov-analyze$(DOT_EXE)),)
POP_RELEASE_NO_OVERRIDE:=$(POP_RELEASE_NO_OVERRIDE)/objs/$(mc_platform)/root
endif
# Root of the PoP data (config, idir)
# Relative to repo root
POP_ROOT?=pop
POP_CONFIG?=$(POP_ROOT)/config/coverity_config.xml
POP_DIR?=$(POP_ROOT)/dir
POP_BIN_DIR:=$(POP_RELEASE_NO_OVERRIDE)/bin
POP_OPTIONS?=--enable-virtual --all

ifeq ($(wildcard $(POP_BIN_DIR)/cov-analyze$(DOT_EXE)),)
$(error "Cannot find cov-analyze in $(POP_BIN_DIR)")
endif

POP_RECORDED_TUS:=$(POP_ROOT)/recorded-tus

# If POP_AUTO_TRANSLATE, then every compilation will be recorded by
# default.
ifneq ($(POP_AUTO_TRANSLATE),)

# Normal operation is to run the normal compilation with record-only.
# With POP_NO_COMPILE, it will do a cov-emit
ifneq ($(POP_NO_COMPILE),)
POP_RUN_COMPILE:=
else
POP_RUN_COMPILE:=\
--run-compile \
--record-only
endif

POP_TRANSLATE:=$(POP_BIN_DIR)/cov-translate \
-c $(POP_CONFIG) \
$(POP_RUN_COMPILE) \
--dir $(POP_DIR)

endif

ifneq ($(THREADS),)
POP_PARALLEL:=-j $(THREADS)
else
POP_PARALLEL:=-j auto
endif

.PHONY: \
pop \
pop-config \
pop-replay \
pop-record-desktop-build \
pop-analyze \
pop-analyze-replayed \
pop-analyze-format \
pop-format \
pop-clean \

pop-config: $(POP_CONFIG)

$(POP_CONFIG):
	$(POP_BIN_DIR)/cov-configure -c $@ -co gcc --template
	$(POP_BIN_DIR)/cov-configure -c $@ -co $(PLATFORM_PACKAGES_DIR)/bin/ccache


pop-record-desktop-build: pop-config
	$(POP_BIN_DIR)/cov-build -c $(POP_CONFIG) --dir $(POP_DIR) \
		--desktop $(MAKE) POP_BUILD=1

pop-replay:
	if $(POP_BIN_DIR)/cov-manage-emit -tp 'recorded()' --dir $(POP_DIR) \
	          link-file $(POP_RECORDED_TUS); then \
	          $(POP_BIN_DIR)/cov-manage-emit -tp 'link_file("$(POP_RECORDED_TUS)")' --dir $(POP_DIR) \
	          recompile $(POP_PARALLEL); \
	fi

# POP_OPTIONS can be set on the command line, which overrides any
# reassignment.
# So use a different variable name.
POP_REAL_OPTIONS:=--dir $(POP_DIR) $(POP_OPTIONS) $(POP_PARALLEL)

# A target that only analyzes TUs that were just replayed.
pop-analyze-replayed:
	$(POP_BIN_DIR)/cov-analyze -tp 'link_file("$(POP_RECORDED_TUS)")' $(POP_REAL_OPTIONS)

pop-clean:
	@ # Use cov-manage-emit to delete only the emit so that we don't
	@ # clear the incremental DB.
	test '!' -d $(POP_DIR) || $(POP_BIN_DIR)/cov-manage-emit \
		--dir $(POP_DIR) \
		clear

pop-analyze:
	$(POP_BIN_DIR)/cov-analyze $(POP_REAL_OPTIONS)

pop-analyze-format:
	$(POP_BIN_DIR)/cov-analyze $(POP_REAL_OPTIONS) --emacs-errors

pop-format: 
	$(POP_BIN_DIR)/cov-format-errors --dir $(POP_DIR) --emacs-style \
	--exclude-files '/($(PLATFORM_PACKAGES_DIR)|packages)/|(edg/src/[^/]*$$)'

pop: pop-replay pop-analyze pop-format

ifneq ($(POP_HOST),)
ifneq ($(POP_STREAM),)
ifneq ($(POP_AUTH_FILE),)
ifneq ($(SOURCES),)
HAS_DESKTOP_PARAMS:=1
endif
endif
endif
endif

ifeq ($(HAS_DESKTOP_PARAMS),1)

pop-desktop:
	$(POP_RELEASE)/bin/cov-run-desktop --dir $(POP_DIR) --host $(POP_HOST) \
		--stream $(POP_STREAM) --auth-key-file $(POP_AUTH_FILE) \
		--scm git \
		--reference-snapshot scm \
		$(SOURCES)

else

pop-desktop:
	@echo "pop-desktop requires POP_HOST, POP_STREAM, POP_AUTH_FILE (e.g. in personal.mk) as well as SOURCES"
	@false

endif

else
HAS_POP:=
POP_TRANSLATE:=
POP_CONFIG:=
endif
