# 

# makefiles.mk
# Variables referencing various includes that generate targets
# ("function-like includes")
#
# The idea is that each of these variables can be treated like a
# function, and invoked by setting certain variables and then
# including the variable's expansion.  See each of the individual
# makefiles for information on how to invoke.

ifeq (1,$(ALL_RULES_INCLUDES))

CREATE_LIB:=$(RULES)/create-lib.mk
CREATE_SHLIB:=$(RULES)/create-shlib.mk
CREATE_PROG:=$(RULES)/create-prog.mk
CREATE_DLL:=$(RULES)/create-dll.mk
BUILD_SOURCES:=$(RULES)/build.mk
ADD_LIBS:=$(RULES)/add-libs.mk
ADD_LIB:=$(RULES)/add-lib.mk
CREATE_ANALYSIS_LIB:=$(RULES)/create-analysis-lib.mk

else

$(error You should include all-rules.mk)

endif
