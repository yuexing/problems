# This file should be included at the end of a unit-test's prog.mk.
#
# It will prepare for a call to $(CREATE_PROG), computing the
# variables NAME, PROG, SOURCE_DIR, and BIN_DIR, and adding CppUnit
# dependencies.
#
# 

CALLER_MAKEFILE=$(word $(words $(MAKEFILE_LIST)),pad_list_front $(MAKEFILE_LIST))

SOURCE_DIR:=$(patsubst %/,%,$(dir $(CALLER_MAKEFILE)))

word_fromend = $(word $(words $(wordlist $(1), $(words $(2)),$(2))),$(2))

SOURCE_COMPONENTS:=$(subst /, ,$(SOURCE_DIR))
ifneq (unit-test,$(call word_fromend,1,$(SOURCE_COMPONENTS)))
$(error Makefile calculates test executable name assuming includer $(CALLER_MAKEFILE) is in a unit-test subdirectory)
endif

# this logic needs to match that in cppunit_targets.mk:
NAME:=$(call word_fromend,3,$(SOURCE_COMPONENTS))-$(call word_fromend,2,$(SOURCE_COMPONENTS))-xunit

BIN_DIR:=$(TEST_BIN_DIR)

DEPENDENCIES+=\
packages/cppunit \
libs/cppunit_main \

EXTRA_CXXFLAGS+=\
-Ipackages/cppunit/include \

PROG:=$(SOURCE_DIR)/prog.mk
