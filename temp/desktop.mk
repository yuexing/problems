# desktop.mk
# Commands for running fast desktop analysis prototype tools.

# Default settings.

# Summary server details.
SSHOST := d-linux64-03.sf.coverity.com
SSPORT := 9202

# This is not ideal, but it will suffice for now to avoid people
# having to put their password in a file.  This account can get
# function summaries and run the commit preview report.
SSUSER := shared_desktop_acct
SSPASS := shared

# Stream that has the summaries.  Last one is simply-expanded ("=")
# so personal.mk can change the platform or branch in order to
# adjust the stream.  The branch name has to be manually updated
# when creating a new branch, but should be done only once there
# is a central analysis with function summaries; until then, a
# new branch should just keep using whatever value it inherited
# from its parent branch.
DESKTOP_PLATFORM := $(shell ./get_platform.sh)
DESKTOP_BRANCH := gilroy
DESKTOP_STREAM = prevent-$(DESKTOP_BRANCH)-$(DESKTOP_PLATFORM)

# Intermediate directory.
DESKTOP_DIR := dir

# Arguments to specify -c coverity_config.xml.  This is normally
# not needed, but it is if you are using a development build.
DESKTOP_CONFIG_ARGS :=

# Shell to start under cov-build.  I do not just use $(SHELL)
# because 'make' always sets that to /bin/sh.
DESKTOP_SHELL := bash

# Analysis options.
DESKTOP_AOPTS :=
DESKTOP_AOPTS += --all
DESKTOP_AOPTS += --enable-virtual --enable-parse-warnings
DESKTOP_AOPTS += --enable PARSE_ERROR
DESKTOP_AOPTS += --disable ASSIGN_NOT_RETURNING_STAR_THIS
DESKTOP_AOPTS += --disable COPY_WITHOUT_ASSIGN
DESKTOP_AOPTS += --disable MISSING_COPY_OR_ASSIGN
DESKTOP_AOPTS += --disable SELF_ASSIGN
DESKTOP_AOPTS += --disable SECURE_CODING
DESKTOP_AOPTS += --disable SLEEP
DESKTOP_AOPTS += --disable STACK_USE
DESKTOP_AOPTS += --disable UNUSED_VALUE
DESKTOP_AOPTS += -j auto

# cov-run-desktop options.  No-spin is mainly so the output does
# not spew all over the emacs compilation output buffer.
DESKTOP_OPTS :=
DESKTOP_OPTS += --ticker-mode=no-spin

# Pull in user-specific settings.
-include $(HOME)/prevent-personal.mk
# Pull in user-set baseline, if it exists. It should override the global
# personal settings, but not the per-repo ones.
-include $(DESKTOP_DIR)/baseline.mk
-include personal.mk

# Check for required settings.
ifeq ($(DESKTOP_CLIENT_TOOLS),)
  $(error Must set DESKTOP_CLIENT_TOOLS to the root of a Prevent install.)
endif

# If you don't plan on using the baseline-now mechanism, you may also want to
# put in personal.mk something like this:
#
#   DESKTOP_MODIFICATION_DATE_THRESHOLD := --modification-date-threshold "2013-09-25 11:33"
#
# but that is not required.  The default date is the date the
# intermediate directory was created.

# Default target.
.PHONY: default
default: run-desktop

# This must be run once after the analysis tools are installed.
.PHONY: config
config: $(DESKTOP_CLIENT_TOOLS)/config/coverity_config.xml

$(DESKTOP_CLIENT_TOOLS)/config/coverity_config.xml:
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-configure $(DESKTOP_CONFIG_ARGS) --gcc
	@#
	ccache=$(DESKTOP_PLATFORM)-packages/bin/ccache; \
	if [ -x "$$ccache" ]; then \
	  $(DESKTOP_CLIENT_TOOLS)/bin/cov-configure $(DESKTOP_CONFIG_ARGS) \
	    --compiler "$$ccache" --comptype prefix; \
	else \
	  echo "Not configuring $$ccache because it does not exist."; \
	fi

.PHONY: create-desktop-dir
create-desktop-dir:
	make -p $(DESKTOP_DIR)

# Start shell under cov-build.
#
# The COVERITY_DISENGAGE_EXES will avoid the outer cov-build interfering
# with the build and tests.  Unfortunately, it only works on Unix.  We
# still don't have a solution for Windows.
.PHONY: start-shell
start-shell:
	COVERITY_DISENGAGE_EXES='run-testsuite;cov-make-library;cov-build' \
	  $(DESKTOP_CLIENT_TOOLS)/bin/cov-build $(DESKTOP_CONFIG_ARGS) \
	    --dir $(DESKTOP_DIR) \
	    --record-with-source $(DESKTOP_SHELL)

# Run capture manually
#
.PHONY: capture
capture: config
	COVERITY_DISENGAGE_EXES='run-testsuite;cov-make-library;cov-build' \
	  $(DESKTOP_CLIENT_TOOLS)/bin/cov-build $(DESKTOP_CONFIG_ARGS) \
	    --dir $(DESKTOP_DIR) \
	    $(COMMAND)

# Just download summaries, for testing purposes.
#
# With default settings of 1 retry and 5 minute timeout, over the
# VPN, it spends 5 minutes downloading about 60 MB, then times out,
# then does it again, before aborting.  The settings below turn off
# the retry and use a timeout of 10 hours.  See bug 53688.
.PHONY: download-fs
download-fs:
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-manage-history --dir $(DESKTOP_DIR) \
	  download-analysis-summaries \
	  --host $(SSHOST) --port $(SSPORT) \
	  --user $(SSUSER) --password $(SSPASS) \
	  --stream $(DESKTOP_STREAM) \
	  --max-retries=0 --response-timeout=36000

# Check that the current idir is compatible with the current analysis
# tools.  This is useful after an upgrade; if it is compatible, then
# you can keep using your long-running cov-builded editor.
.PHONY: check-compatible
check-compatible:
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-manage-emit --dir $(DESKTOP_DIR) \
	  check-compatible

# Analyze!
.PHONY: run-desktop
run-desktop:
	COVERITY_COMMIT_WITH_DOMAIN=1 \
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-run-desktop --dir $(DESKTOP_DIR) \
	  --host $(SSHOST) --port $(SSPORT) \
	  --user $(SSUSER) --password $(SSPASS) \
	  --stream $(DESKTOP_STREAM) \
	  $(DESKTOP_MODIFICATION_DATE_THRESHOLD) \
	  $(DESKTOP_AOPTS) \
	  $(DESKTOP_OPTS) \
	  --json-output $(DESKTOP_DIR)/desktop-defects.json

# Throw away all the TUs.
#
# Hmmm, this does not work if cov-build is still running because
# it removes the CovBuildInvocation, but cov-build still thinks
# the invocation record exists and can be updated.
.PHONY: clear-emit
clear-emit:
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-manage-emit --dir $(DESKTOP_DIR) clear

# List current emit contents.
.PHONY: list
list:
	$(DESKTOP_CLIENT_TOOLS)/bin/cov-manage-emit --dir $(DESKTOP_DIR) list

# Show values of various variables.
.PHONY: show-variables
show-variables:
	@echo "DESKTOP_STREAM: $(DESKTOP_STREAM)"
	@echo "DESKTOP_CLIENT_TOOLS: $(DESKTOP_CLIENT_TOOLS)"

.PHONY: baseline-now
baseline-now:
	@if [ -d $(DESKTOP_DIR) ]; then \
	echo "DESKTOP_MODIFICATION_DATE_THRESHOLD:=--modification-date-threshold \"`date '+%F %R'`\"" > $(DESKTOP_DIR)/baseline.mk; \
	echo "Baseline time set to `date '+%F %R'`"; \
	else \
	echo "Desktop analysis intermediate directory does not exist yet. When the intermediate directory is created, it will implicitly have the baseline time of its creation."; \
	fi


# EOF
