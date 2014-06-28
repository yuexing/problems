# This file should be included as the only line from a unit-test
# subdirectory's Makefile.
#
# It will define the targets "prog" and "testsuite" and define the
# default target as "prog".  prog effectively runs make-one-prog with
# the prog.mk makefile also in the unit-test subdirectory.
#
# 

THIS_MAKEFILE=$(lastword $(MAKEFILE_LIST))

RULES_DIR:=$(patsubst %/,%,$(dir $(THIS_MAKEFILE)))

ROOT_DIR:=$(patsubst %/,%,$(dir $(RULES_DIR)))

# Presumably just 'Makefile'!
CALLER_MAKEFILE:=$(word $(words $(MAKEFILE_LIST)),pad_list_from_front $(MAKEFILE_LIST))

# list of directory components/makefile names:
CALLER_COMPONENTS=$(subst /, ,$(dir $(abspath $(CALLER_MAKEFILE))))

word_fromend = $(word $(words $(wordlist $(1), $(words $(2)), $(2))),$(2))

ifneq (unit-test, $(call word_fromend,1,$(CALLER_COMPONENTS)))
$(error This makefile calculates PROG and NAME based on the directory of its includer.  Make sure it is included from the Makefile in unit-test.)
endif

# this logic needs to match that in cppunit_create_prog.mk:
NAME:=$(call word_fromend,3,$(CALLER_COMPONENTS))-$(call word_fromend,2,$(CALLER_COMPONENTS))-xunit

PROG:=$(subst $(EMPTY) $(EMPTY),/,$(wordlist $(words $(subst /, ,startafterroot $(abspath $(ROOT_DIR)))),30000,$(CALLER_COMPONENTS)))/prog.mk


# get TEST_BIN_DIR:
include $(RULES_DIR)/paths.mk

.PHONY: prog
default: prog

prog:
	$(MAKE) -C $(ROOT_DIR) -f new-rules/make-one-prog.mk \
	PROG=$(PROG)

clean:
	$(MAKE) -C $(ROOT_DIR) -f new-rules/make-one-prog.mk \
	PROG=$(PROG) \
	clean

testsuite:
	$(ROOT_DIR)/$(TEST_BIN_DIR)/$(NAME) run
