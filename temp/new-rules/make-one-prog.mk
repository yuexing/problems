# 

# An include to build a single program, based on a prog.mk passed to the command line

default: all

include new-rules/all-rules.mk

# You must pass PROG=<path to prog.mk> to build a specific program
# The path must be relative to the root
# e.g.
# $(MAKE) -C ../.. -f new-rules/make-one-prog.mk PROG=build/cov-build/prog.mk
ifneq (,$(PROG))

SOURCE_DIR:=$(patsubst %/,%,$(dir $(PROG)))

include $(PROG)

EXTRA_CXXFLAGS+=$(MAKE_ONE_PROG_EXTRA_CXXFLAGS)
EXTRA_CFLAGS+=$(MAKE_ONE_PROG_EXTRA_CFLAGS)

ifneq (,$(SOURCES_OVERRIDE))
SOURCES:=$(SOURCES_OVERRIDE)
endif

ifeq (,$(NO_LINK))
include $(CREATE_PROG)

clean: extra-clean
	rm -f $(OBJS) $(DEPS) $(PROG_BINARY)

else
include $(BUILD_SOURCES)

clean: extra-clean
	rm -f $(OBJS)

endif

.PHONY: extra-clean

else
$(error Must give a value to the PROG variable)

endif


# -----------------------------------------------
# Experiment: insert desktop analysis at the end.
ifneq ($(RUN_DESKTOP_DURING_MAKE),)

.PHONY: run-desktop
default: run-desktop
run-desktop: all
	$(MAKE) -C $(top_srcdir) -f desktop.mk

endif


# EOF
