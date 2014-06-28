# 

# Must include test.mk beforehand
include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))test.mk

include $(RULES)/$(mc_platform)-flags.mk

COV_EMIT:=$(COV_BIN_DIR)/cov-emit
COV_BUILD:=$(COV_BIN_DIR)/cov-build
COV_GENTEST:=$(TEST_BIN_DIR)/cov-gentest


.PHONY: compile-source-c compile-source-cxx compile-source-gcc

compile-source-c:
	$(COV_EMIT) --no_error_recovery --type_sizes P4 --dir $(IDIR) --c -w $(EMITOPTS) $(TESTFILE)

compile-source-cxx:
	$(COV_EMIT) --no_error_recovery --type_sizes P4 --dir $(IDIR) --c++ -w $(EMITOPTS) $(TESTFILE)


.PHONY: generate-test generate-test-cxx

generate-test: compile-source-c gentest-line gentest-branch
generate-test-cxx: compile-source-cxx gentest-line gentest-branch


.PHONY: gentest-line gentest-branch

# Skip tests on AIX since this platform isn't supported
ifneq ($(mc_platform),aix)

gentest-line:
	$(COV_GENTEST) --cov_goal line --dir $(IDIR) $(TESTOPTS) 2>&1 | tee $(TESTSUITE_DIR)/line.log
	diff -u -w golden.line $(TESTSUITE_DIR)/$(TESTRESULTS)

gentest-branch:
	$(COV_GENTEST) --cov_goal branch --dir $(IDIR) $(TESTOPTS) 2>&1 | tee $(TESTSUITE_DIR)/branch.log
	diff -u -w golden.branch $(TESTSUITE_DIR)/$(TESTRESULTS)

else

gentest-line:
gentest-branch:

endif

# EOF
