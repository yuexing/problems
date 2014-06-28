-include personal.mk

# Get mc_platform
include new-rules/config.mk
include new-rules/paths.mk  # RELEASE_DOC_DIR

include package-tags.mk
include model-db-hashes.mk
include model-db-vars.mk # EMIT_VER, ANDROID_MODELS_DB
USE_GIT_PACKAGES_REPO:=1

binary_packages_tag:=$(mc_platform)-packages-$($(mc_platform)_binary_packages_tag)

PVERSION:=-$(shell cat version-codename)
BRANCH:=$(shell cat version-codename | cut -d. -f1,2)

PBRANCH:=-trunk
ifneq (,$(findstring b-push-, $(shell cat scripts/bktag.txt)))
        PBRANCH:=-branch
endif

export CVS_RSH=ssh

#set gone just in case it is set in environment
GONE=BitKeeper/etc/gone

CVS_TAG=-r $(binary_packages_tag)
BK_TAG=-r$(source_packages_tag)
GIT_HASH=$(source_packages_git_hash)

ifeq (1,$(BUILD_FE_SUBSET))
FE_PACKAGES_REPO=bk://offshore.coverity.com:5555:/home/coverity/packages-subset${PVERSION}${PBRANCH}
FE_BIN_PACKAGES_REPO=":pserver:luxoft@offshore.coverity.com:/home/coverity/repository-subset"

BK_REPO=$(FE_PACKAGES_REPO)
CVS_REPO=$(FE_BIN_PACKAGES_REPO)
else
BK_REPO=bk://bk.sf.coverity.com:/home/coverity/packages$(PVERSION)$(PBRANCH)
GIT_REPO=$(PACKAGE_USER)@git.coverity.com:/home/git/packages.git
CVS_REPO=$(PACKAGE_USER)@cvs.sf.coverity.com:/home/coverity/repository
endif
CLANG_REPO=$(PACKAGE_USER)@git.coverity.com:/home/git/clang.git
CLANG_BRANCH=$(BRANCH)

.PHONY: default checkout docs

default: checkout

checkout: packages $(mc_platform)-packages update-model-dbs

packages:
ifeq ($(USE_GIT_PACKAGES_REPO),1)
#	git clone -b $(BRANCH) $(GIT_REPO) $@;
#	cd $@ && git checkout $(GIT_HASH)
	git clone -n $(GIT_REPO) $@;
	cd $@ && git checkout $(BRANCH) && git reset --merge $(GIT_HASH)
else
	BK_GONE_OK=1 BK_GONE=$(GONE) bk clone $(BK_TAG) \
	$(BK_REPO) $@
endif

$(mc_platform)-packages:
	cvs -d $(CVS_REPO) \
	co $(CVS_TAG) $@

clang-sources:
	git clone -n $(CLANG_REPO) $@
	cd $@ ; git checkout $(CLANG_BRANCH)

.PHONY: update update-source-packages update-binary-packages update-model-dbs

update: checkout update-source-packages update-binary-packages update-model-dbs

update-source-packages: packages
ifeq ($(USE_GIT_PACKAGES_REPO),1)
#	cd packages && git fetch && git checkout $(GIT_HASH)
	cd packages && \
		git fetch && \
		git checkout $(BRANCH) && \
		git reset --merge $(GIT_HASH)
else
	LEVEL=`bk level | sed s/'^.* \([0-9]*\)$$/\1/'`; \
	cd packages; \
		bk parent $(BK_REPO) && \
		bk level $$LEVEL && \
		BK_GONE_OK=1 BK_GONE=$(GONE) bk pull $(BK_TAG)
endif

update-binary-packages: $(mc_platform)-packages
	cd $(mc_platform)-packages; \
		cvs up -r $(binary_packages_tag)

ifneq ($(filter macosx netbsd,$(mc_platform)),)
MD5SUM=openssl dgst -md5
else
MD5SUM=md5sum
endif

update-model-dbs:
	$(MAKE) -f checkout.mk check-model-dbs >/dev/null 2>&1 \
		|| $(MAKE) -f checkout.mk copy-jre-db copy-android-db copy-cs-system-db

check-model-dbs:
	@(($(MD5SUM) $(ANDROID_MODELS_DB) | grep -q $(ANDROID_MODELS_DB_HASH)) && \
	($(MD5SUM) $(JRE_MODELS_DB) | grep -q $(JRE_MODELS_DB_HASH)) && \
	($(MD5SUM) $(CS_SYSTEM_MODELS_DB) | grep -q $(CS_SYSTEM_MODELS_DB_HASH))) \
		|| (echo "Models DBS are out of date. Run make update-packages"; false)


# Generate a target to obtain $(1) from cvs, based on the hashes
# above.
define copy-model-db-from-cvs

copy-$(2)-db:
	@ # Might be there read-only already (from old checked-in file)
	rm -f "$$($(1))"
	FILE_NAME=`basename $$($(1))`-$$($(1)_HASH); \
	scp -C $(PACKAGE_USER)@cvs.sf.coverity.com:/home/coverity/java-models/$$$$FILE_NAME $$($(1))
	@ # Might not be writable (I believe scp preserves flags)
	chmod +w $$($(1))

endef

$(eval $(call copy-model-db-from-cvs,ANDROID_MODELS_DB,android))
$(eval $(call copy-model-db-from-cvs,JRE_MODELS_DB,jre))
$(eval $(call copy-model-db-from-cvs,CS_SYSTEM_MODELS_DB,cs-system))

.PHONY: check check-source-packages-tag check-binary-packages-tag

check-git-config:
	@# The "hooks" directory might not always exist for some reason...
	@# I saw this problem on b-solarissparc-02.
	@if [ ! -d .git/hooks ]; then mkdir .git/hooks; fi
	@# Clobber pre-commit and pre-rebase with whatever I have
	@cp scripts/git-hooks/pre-commit .git/hooks/pre-commit
	@chmod +x .git/hooks/pre-commit
	@cp scripts/git-hooks/pre-rebase .git/hooks/pre-rebase
	@chmod +x .git/hooks/pre-rebase
	@cp scripts/git-hooks/commit-msg .git/hooks/commit-msg
	@chmod +x .git/hooks/commit-msg

ifneq ($(AUTO_PKG_UPDATE),)

check: check-git-config update

else ifeq ($(SKIP_PKG_CHECKS),)

check: check-git-config \
        check-source-packages-tag check-binary-packages-tag \
	check-build-prereq check-model-dbs

check-source-packages-tag:
	@echo "Checking packages exist and have the correct revision... "
	@test -d packages || (echo "Error: you need to check out packages. Use make checkout-packages"; exit 1)
ifeq ($(USE_GIT_PACKAGES_REPO),1)
	@cd packages; \
	current_rev="`git rev-parse HEAD`"; \
	if [ '!' "$$current_rev" = "$(source_packages_git_hash)" ]; then \
		echo "Error: packages revision is $$current_rev, expected $(source_packages_git_hash). You may need to run make update-packages"; \
		exit 1; \
	fi
else
	@cd packages; \
		current_rev="`bk -R log -hr+ -nd:MD5KEY: ChangeSet`"; \
	if [ '!' "$$current_rev" = "$(source_packages_tag)" ]; then \
		echo "Error: packages revision is $$current_rev, expected $(source_packages_tag). You may need to run make update-packages"; \
		exit 1; \
	fi
endif
	@echo 'Success!'

check-binary-packages-tag:
	@echo "Checking binary packages exist and have the correct tag... "
	@test -d $(mc_platform)-packages || (echo "Error: you need to check out platform packages. Use make checkout-packages"; exit 1)
	@cd $(mc_platform)-packages; \
		current_rev=`cat TAG | grep -v '#'`; \
	if [ '!' "$$current_rev" = "$(binary_packages_tag)" ]; then \
		echo "Error: platform packages tag is \"$$current_rev\", expected \"$(binary_packages_tag)\". You may need to run make update-packages"; \
		exit 1; \
	fi
	@echo 'Success!'
	@echo "Updating files if necessary"
	@$(MAKE) -C $(mc_platform)-packages

# This isn't really a packages check, but it is a convenient place to
# put a prereq check without adding a lot of noise.
check-build-prereq:
ifeq ($(IS_WINDOWS),1)
ifeq ($(SKIP_DOTNET_CHECKS),)
	@# Check for .NET Framework (bug 54307)
	@# First run with no redirection to check for buggy cygwin
	'$(MSBUILD)' /version
	@echo
	@# Now run looking for any non-empty output.  (Can't promote all msbuild warnings to errors!)
	@for PROJ in csfe/dummy-projects/*.csproj; do \
		echo ".NET Framework check: $$PROJ"; \
		'$(MSBUILD)' /nologo /verbosity:quiet $$PROJ 2>&1 | grep . && echo "ERROR: msbuild generated output.  See csfe/dummy-projects/README." && exit 2; \
		true; \
	done
endif
	@# The detours build needs unzip.  See build/capture/detours-build-3.0.sh.
	@if which unzip 2>/dev/null; then true; else \
	  echo "Error: 'unzip' must be installed to build on Windows."; \
	  exit 2; \
	fi
endif
	@echo "Build prereqs satisfied."

else # SKIP_PKG_CHECKS

check: check-git-config
	echo "skipping pkg checks as per SKIP_PKG_CHECKS"

endif # SKIP_PKG_CHECKS

DOC_RELEASE=/nfs/qabuild/prevent-releases/latest/docs

# Don't include docs in the checkout target because the
# soft link we create below causes problems when building
# the release.
docs:
	@if [ -d "$(DOC_RELEASE)" ]; then \
	    docdir=$$(ls -rt $(DOC_RELEASE) | tail -1); \
	    mkdir -p $(RELEASE_DOC_DIR); \
	    ln -s $(DOC_RELEASE)/$$docdir/prevent/doc/help $(RELEASE_DOC_DIR); \
	fi

# EOF


