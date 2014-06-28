# 
# clang.mk
# Include targets for Clang specific components

include new-rules/all-rules.mk

# Clang specific modules
SOURCE_DIR:=clang/cov-internal-emit-clang
include $(SOURCE_DIR)/prog.mk $(CREATE_PROG)

ifneq (,$(BUILD_FE_CLANG))
$(RELEASE_ROOT_DIR)/bin/cov-internal-clang$(EXE_POSTFIX): $(mc_platform)-packages/bin/clang$(EXE_POSTFIX)
	cp -f $^ $@

ALL_TARGETS+=$(RELEASE_ROOT_DIR)/bin/cov-internal-clang$(EXE_POSTFIX)

all: $(RELEASE_ROOT_DIR)/bin/cov-internal-clang$(EXE_POSTFIX)
endif
