# locale.mk
# rules to create the shipping locale-related files

ifeq (,$(LOCALE_INCLUDED))
LOCALE_INCLUDED:=1

include new-rules/all-rules.mk

# directory to put the compiled locale files into
RELEASE_LOCALE_DIR := $(RELEASE_ROOT_DIR)/locale

# directory to get the locale source files from
SOURCE_LOCALE_DIR := locale

# rule to make an .mo from a .po
$(RELEASE_LOCALE_DIR)/%/LC_MESSAGES/coverity.mo: $(SOURCE_LOCALE_DIR)/po/%.po
	mkdir -p $(dir $@)
	msgfmt -o $@ $^

# set of files that need to be created and put into a release
LOCALE_FILES :=

# for now, we only pull these in on linux32
ifeq ($(mc_platform),linux)
LOCALE_FILES += $(RELEASE_LOCALE_DIR)/fr/LC_MESSAGES/coverity.mo
endif

.PHONY: locale_files
locale_files: $(LOCALE_FILES)

all: locale_files

endif
# EOF
