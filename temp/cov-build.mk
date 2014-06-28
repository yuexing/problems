include new-rules/all-rules.mk
include new-rules/gcc-config.mk

include build/progs.mk
include build/compilers/fake-compilers/fake-compilers.mk

ifneq ($(mc_platform),aix)

include build/systems/ecloud/ecloud.mk

$(RELEASE_BIN_DIR)/cov-internal-thunk.sh: build/cov-build/cov-internal-thunk.sh
	cp -f $^ $@

$(RELEASE_BIN_DIR)/cov-internal-trace.sh: build/cov-translate/cov-internal-trace.sh
	cp -f $^ $@

endif

$(RELEASE_ROOT_DIR)/config/cit/switch-attributes.hpp: build/compilers/switch-attributes.hpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/switch-attributes.cpp: build/compilers/switch-attributes.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/translate_options.hpp: build/compilers/translate_options.hpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/translate_options.cpp: build/compilers/translate_options.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/intern_to_extern_phase.cpp: build/compilers/cit/intern_to_extern_phase.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/intern_to_extern_response_file_split.cpp: build/compilers/cit/intern_to_extern_response_file_split.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/CompilerOptions.hpp: build/compilers/CompilerOptions.hpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

$(RELEASE_ROOT_DIR)/config/cit/CompilerOptions.cpp: build/compilers/CompilerOptions.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@) ; fi
	cp -f $^ $@

COV_BUILD_TARGETS=\
  $(RELEASE_BIN_DIR)/cov-internal-thunk.sh \
  $(RELEASE_BIN_DIR)/cov-internal-trace.sh \

CIT_TARGETS=\
  $(RELEASE_ROOT_DIR)/config/cit/switch-attributes.hpp \
  $(RELEASE_ROOT_DIR)/config/cit/switch-attributes.cpp \
  $(RELEASE_ROOT_DIR)/config/cit/translate_options.hpp \
  $(RELEASE_ROOT_DIR)/config/cit/translate_options.cpp \
  $(RELEASE_ROOT_DIR)/config/cit/CompilerOptions.hpp \
  $(RELEASE_ROOT_DIR)/config/cit/CompilerOptions.cpp \
  $(RELEASE_ROOT_DIR)/config/cit/intern_to_extern_phase.cpp \
  $(RELEASE_ROOT_DIR)/config/cit/intern_to_extern_response_file_split.cpp

COV_BUILD_TARGETS+=$(CIT_TARGETS)

ifneq ($(mc_platform),aix)

ALL_TARGETS+=$(COV_BUILD_TARGETS)

all: $(GCC_CONFIG) $(COV_BUILD_TARGETS)

else

ALL_TARGETS+=$(CIT_TARGETS)
ALL_TARGETS+=$(SN_TARGETS)

all: $(GCC_CONFIG) $(CIT_TARGETS) $(SN_TARGETS)

endif
