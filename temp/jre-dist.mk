ifeq (,$(JRE_DIST_INCLUDED))
JRE_DIST_INCLUDED:=1
# This makefile extracts JREs that are packaged with static analysis
# as part of the prevent build.  (At time of writing, a Java 6 JRE is
# packaged by the installer under "jre", not handled here.) It also create
# the fast-jre for testing.
#
# These JREs were originally intended for use by FindBugs, but have
# seen expanded use for
# * Security/sanitizer DA
# * Running Jasper for emitting JSPs
# * Framework analyzer(?)
# These are flagged by major version so that JRE consumers can specify
# version requirements and the best available version can then be chosen
# and used.

# For debugging just this file:
include new-rules/all-rules.mk

# Copy-pasta (needed?)
POP_BUILD = $(if $(COVERITY_DISENGAGE_EXES),,$(COVERITY_OUTPUT))

ifeq ($(NO_JAVA),)

# 1 = target JRE dir, 2 = java executable in there, 3 = "from" JRE .tar.gz
# 4 = JRE major version, 5 = severity if not found (fail if ERROR)
define jre_extraction
$(value 2): $(value 3)
ifeq (,$(value 3))
	@echo $(value 5): There is no JRE$(value 4) in the packages dir for this platform.
	@test "ERROR" "!=" "$(value 5)"
else
	# Clean up
	rm -rf "$(value 1)"-tmp "$(value 1)"
	# Also clean up old name: jre-findbugs
	rm -rf $(RELEASE_ROOT_DIR)/jre-findbugs
	# Make a temp dir
	mkdir -p "$(value 1)"-tmp
	# Unzip the tgz
	cd "$(value 1)"-tmp && tar -xzf "$(value 3)"
	# Remove any files starting with "._" (Bug 64446)
	cd "$(value 1)"-tmp && find . -name '._*' | xargs rm -rf
	# Assume the tgz contains only a single directory (and we don't
	# care what the name is), otherwise this will fail.
	# Also, only keep whats under Contents/Home (for MacOSX)
	if [ -d "$(value 1)"-tmp/*/Contents/Home ]; then mv "$(value 1)"-tmp/*/Contents/Home "$(value 1)"; else mv "$(value 1)"-tmp/* "$(value 1)"; fi
	# Discard the empty temp dir
	rm -rf "$(value 1)"-tmp
	# Sanity check what was extracted
	"$$@" -version
	# Touch the output file to make sure it's considered up-to-date
	# despite timestamps extracted by tar.
	touch "$$@"
endif
endef

JRE7_TGZ := $(wildcard $(abspath $(PLATFORM_PACKAGES_DIR))/jre-7*.tar.gz)
JRE7_EXE := $(JRE7_DIR)/bin/java$(DOT_EXE)
$(eval $(call jre_extraction,$(JRE7_DIR),$(JRE7_EXE),$(JRE7_TGZ),7,ERROR))
all: $(JRE7_EXE)
# Also, cov-emit-java and cov-analyze can use these, so make sure they're
# in place for anything that uses those.
# This is much easier than identifying the specific consumers of
# cov-emit-java and cov-analyze that might care, and the cost is minimal
# because distributed JREs change so infrequently.
$(COV_EMIT_JAVA): $(JRE7_EXE)
$(COV_ANALYZE): $(JRE7_EXE)

JRE8_TGZ := $(wildcard $(abspath $(PLATFORM_PACKAGES_DIR))/jre-8*.tar.gz)
JRE8_EXE := $(JRE8_DIR)/bin/java$(DOT_EXE)
$(eval $(call jre_extraction,$(JRE8_DIR),$(JRE8_EXE),$(JRE8_TGZ),8,NOTE))
# XXX: Because of a current error, pretend like we don't have Java 8
# while under PoP.  Looks like this when running java -version:
#  Error: Could not find or load main class
FAST_LIB_JRE := $(call SHELL_TO_NATIVE,$(abspath $(JRE7_DIR)))
FAST_LIB_EXE := $(JRE7_EXE)
ifeq (,$(POP_BUILD))
FAST_LIB_JRE := $(call SHELL_TO_NATIVE,$(abspath $(JRE8_DIR)))
FAST_LIB_EXE := $(JRE8_EXE)

all: $(JRE8_EXE)
$(COV_EMIT_JAVA): $(JRE8_EXE)
$(COV_ANALYZE): $(JRE8_EXE)
endif

# make the test-data/fast-lib/rt.jar from the latest jre with the packages required to make testsuite pass:
# java/beans: mainly used by xss-injections
# java/match: divide-by-zero
# java/text: null-returns, os-cmd-injection, xss-injection uses it for format
# java/security: mainly used by risky-crypto, weak-password-hash, xss-injection
# java/sql: xss-injection, sql-injection
# javax/accessibility: forward-null/jtest-DR64503
# javax/imageio: resource-leak
# javax/management: os-cmd-injection
# javax/naming: ldap-injection
# javax/script: script-code-injection
# javax/security: auth/kerberos
# javax/sql: jp-testsuite
# javax/swing: forward-null null-returns
# com.sun.net: hardcoded-credentials
# org.*: xss-injection
# sun.misc: os-cmd-injection
# sun.util.calendar: os-cmd-injection
FAST_LIB_PATH:=$(RELEASE_ROOT_DIR)/test-data/fast-lib
$(FAST_LIB_PATH): $(FAST_LIB_EXE)
	rm -rf $(FAST_LIB_PATH)
	mkdir -p $(FAST_LIB_PATH)
	cd $(FAST_LIB_PATH) && mkdir -p temp rt/java rt/javax rt/com/sun/ rt/org/w3c/ rt/org/xml rt/sun/util/ rt/sun/ \
	    && cd temp && $(JAR) -xf $(FAST_LIB_JRE)/lib/rt.jar > /dev/null \
	    && mv META-INF ../rt \
	    && mv java/awt java/lang java/util java/io java/nio java/math java/net java/sql java/text java/beans java/security ../rt/java \
	    && mv javax/accessibility javax/imageio javax/management javax/naming javax/net javax/script javax/security javax/sql javax/swing javax/xml ../rt/javax \
	    && mv com/sun/net ../rt/com/sun/ \
	    && mv org/xml ../rt/org/ \
	    && mv org/w3c/dom ../rt/org/w3c \
	    && mv sun/misc ../rt/sun \
	    && mv sun/util/calendar ../rt/sun/util/ \
	    && cd .. && $(JAR) -cf rt.jar -C rt . \
	    && rm -rf temp rt

all: $(FAST_LIB_PATH)
$(COV_EMIT_JAVA): $(FAST_LIB_PATH)
$(COV_ANALYZE): $(FAST_LIB_PATH)
endif # !NO_JAVA

endif # !JRE_DIST_INCLUDED
