# Check presence of packages

ifeq (,$(wildcard $(RULES)/../$(PLATFORM_PACKAGES_DIR)))
$(error "You need to check out $(PLATFORM_PACKAGES_DIR); see the wiki page 'Checking Out The System'")
endif
ifeq (,$(wildcard $(RULES)/../$(PLATFORM_PACKAGES_DIR)/bin))
$(error "You need to build $(PLATFORM_PACKAGES_DIR); go into that directory and type 'make'")
endif
ifeq (,$(wildcard $(top_srcdir)/packages))
$(error "You need to check out the packages repository; see the wiki page 'Using Bit keeper'")
endif

