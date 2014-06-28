# pop.mk
# Run Prevent on Prevent

# where stuff is
include new-rules/paths.mk

# need to know how Prevent invokes the compiler so we can configure it
include new-rules/all-rules.mk

ifneq (,$(POP_RELEASE))
POP_RELEASE:=$(abspath $(POP_RELEASE))
else
POP_RELEASE:=$(abspath $(RELEASE_ROOT_DIR))
endif

POP_BIN_DIR:=$(POP_RELEASE)/bin
POP_TEST_BIN_DIR:=$(POP_RELEASE)/test-bin
POP_PORT:=5467

POP_USER := admin
POP_PASSWORD := coverity
POP_HOST := localhost
POP_CIM_PORT := 8080
POP_STREAM := pop

# root of the prevent-on-prevent stuff
ifneq (,$(POP_DIR))
POP:=$(POP_DIR)
else
POP:=pop
endif

# Analysis options.
ifeq (,$(JOBS))
  JOBS := 4
endif
AOPTS := --all --enable-virtual --enable-fnptr -j $(JOBS)

# where the compiler config will go
CONFIG := $(POP)/config/coverity_config.xml

# where the DB will live
DATA := $(POP)/data

prevent-on-prevent:
	$(MAKE) -f pop.mk pop-build
	$(MAKE) -f pop.mk pop-analyze
	$(MAKE) -f pop.mk pop-commit-cim

$(CONFIG):
	mkdir -p $(dir $@)
	$(POP_BIN_DIR)/cov-configure -c $(CONFIG) --gcc
ifneq ($(HAS_CCACHE),0)
	$(POP_BIN_DIR)/cov-configure -c $(CONFIG) --compiler $(CCACHE) --comptype prefix
endif

# Note: cov-build on a Prevent build relies on cov-analyze.mk
# suppressing the build of model libraries, since otherwise the emit
# dirs would get conflated.
pop-build: $(CONFIG)
	mkdir -p $(POP)
	touch $(POP)/.bk_skip
	POP_BUILD=1 $(POP_BIN_DIR)/cov-build -c $(CONFIG) --dir $(POP)/dir \
	  $(MAKE) OBJ_DIR=$(POP)/objs HAS_CCACHE=0

pop-analyze:
	$(POP_BIN_DIR)/cov-analyze --dir $(POP)/dir --enable-parse-warnings $(AOPTS)

$(DATA):
	$(POP_BIN_DIR)/cov-install-gui -d $(DATA) --password admin --product Prevent --domain C/C++

pop-commit: $(DATA)
	$(POP_TEST_BIN_DIR)/dm-cov-commit-defects -d $(DATA) --dir $(POP)/dir --user admin

pop-commit-cim:
	$(POP_BIN_DIR)/cov-commit-defects --dir $(POP)/dir \
	  --user $(POP_USER) --password $(POP_PASSWORD) \
	  --host $(POP_HOST) --port $(POP_CIM_PORT) \
	  --stream $(POP_STREAM)

pop-start-gui: $(DATA)
	$(POP_BIN_DIR)/cov-start-gui -d $(DATA) --timeout 300 --port $(POP_PORT)

pop-stop-gui: $(DATA)
	$(POP_BIN_DIR)/cov-stop-gui -d $(DATA)

# EOF
