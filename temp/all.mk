# The "all" target will be set by create-prog / create-lib
default: all

include new-rules/all-rules.mk
include new-rules/package-check.mk

include jsprewriter.mk
include packages.mk
include utilities.mk
include distributor.mk
include edg.mk
include csfe.mk
include jfe.mk
include java-capture.mk
include java-anttask.mk
include java-security-da.mk
include java-framework-analyzer.mk

ifeq ($(BUILD_FE_CLANG),1)
include clang.mk
endif

ifneq ($(mc_platform),aix)
ifneq ($(BUILD_FE_SUBSET),1)
include cov-analyze.mk
include findbugs-integration.mk
include test-analysis.mk
endif
endif

include cov-build.mk
include libs/libs.mk
include symlinks.mk
include jre-dist.mk

ifneq ($(mc_platform),aix)
ifneq ($(BUILD_FE_SUBSET),1)
include gui.mk
include cim.mk
include extend.mk
endif
endif

include locale.mk

include jfe-idir.mk
include csfe-idir.mk

ifneq ($(filter hpux-pa linux linux64 linux-ia64 netbsd netbsd64 solaris-sparc solaris-x86 freebsd freebsdv6 freebsd64,$(mc_platform)),)
include sb.mk
endif

# EOF
