include new-rules/all-rules.mk

ifeq ($(NO_CSHARP),)

include csta/progs.mk

endif
