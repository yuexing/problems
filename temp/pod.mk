# pod.mk
# Run Prevent on Discover

# where stuff is
include new-rules/paths.mk

# need to know how Prevent invokes the compiler so we can configure it
include new-rules/all-rules.mk

ifneq (,$(POD_RELEASE))
POD_RELEASE:=$(abspath $(POD_RELEASE))
else
POD_RELEASE:=$(abspath $(RELEASE_ROOT_DIR))
endif

POD_BIN_DIR:=$(POD_RELEASE)/bin

# root of the prevent-on-discover stuff
ifneq (,$(POD_DIR))
POD:=$(POD_DIR)
else
POD:=discover/pod
endif

# where the compiler config will go
CONFIG := $(POD)/config/discover_config.xml

# where the DB will live
DATA := $(POD)/data

prevent-on-discover:
	$(MAKE) -f pod.mk pod-build
	$(MAKE) -f pod.mk pod-analyze
	$(MAKE) -f pod.mk pod-commit

$(CONFIG):
	mkdir -p $(dir $@)
	$(POD_BIN_DIR)/cov-configure -c $(CONFIG) --compiler $(CC) --comptype gcc
ifneq ($(HAS_CCACHE),0)
	$(POD_BIN_DIR)/cov-configure -c $(CONFIG) --compiler $(CCACHE) --comptype prefix
endif

pod-build: $(CONFIG)
	mkdir -p $(POD)
	touch $(POD)/.bk_skip
	(cd discover && $(POD_BIN_DIR)/cov-build -c ../$(CONFIG) --dir $(POD)/dir $(MAKE) OBJ_DIR=$(POD)/objs NO_PACKAGE_CHECK=1)

pod-analyze:
	$(POD_BIN_DIR)/cov-analyze --dir $(POD)/dir --enable-parse-warnings

$(DATA):
	$(POD_BIN_DIR)/cov-install-gui -d $(DATA) --password admin --product Discover

pod-commit: $(DATA)
	$(POD_BIN_DIR)/cov-commit-defects -d $(DATA) --dir $(POD)/dir --user admin

pod-start-gui: $(DATA)
	$(POD_BIN_DIR)/cov-start-gui -d $(DATA) --timeout 300

pod-stop-gui: $(DATA)
	$(POD_BIN_DIR)/cov-stop-gui -d $(DATA)

# EOF
