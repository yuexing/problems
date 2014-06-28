# Set "TESTSUITE_DIR_OPT" based on "TESTSUITEDIR" or "TESTSUITE_DIR"

# Allow "TESTSUITEDIR" as an alias for "TESTSUITE_DIR"
# This is mainly for historical reason, and because, when running multiple tests, the TESTSUITE_DIR in actual test makefiles will be different.
test_help::
	@echo "  TESTSUITEDIR"
	@echo "      By default, tests are run in temporary directories"
	@echo "      Set this variable to indicate a persistent directory to be used instead."

ifneq (,$(TESTSUITEDIR))
TESTSUITE_DIR:=$(TESTSUITEDIR)
endif
ifeq (,$(TESTSUITE_DIR))
TESTSUITE_DIR_OPT:=
else
$(warning Setting testsuite dir to $(TESTSUITE_DIR))
TESTSUITE_DIR_OPT:=--test-dir $(call SHELL_TO_NATIVE,$(TESTSUITE_DIR))
endif
# Make sure TESTSUITE_DIR is not exported and doesn't override waht run-testsuite will set.
unexport TESTSUITE_DIR
