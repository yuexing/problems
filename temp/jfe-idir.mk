include new-rules/all-rules.mk

# Don't build this tgz if running in POP. It's usually a bad idea to
# run our tools within PoP, and it doesn't help.
POP_BUILD = $(if $(COVERITY_DISENGAGE_EXES),,$(COVERITY_OUTPUT))
# Also don't build if NO_JAVA, obviously.
ifeq (,$(POP_BUILD)$(NO_JAVA))

# Inside the command below, we clear COVERITY_BUILD_INVOCATION_ID
# so it will work inside "make -f desktop.mk start-shell".

.PHONY: java-rt-emit-db fast-jre-emit-db

java-rt-emit-db: $(JAVA_RT_JAR_IDIR_TGZ)
fast-jre-emit-db: $(FAST_JRE_JAR_IDIR_TGZ)

$(JAVA_RT_JAR_IDIR_TGZ) : $(COV_EMIT_JAVA) $(COV_INTERNAL_EMIT_JAVA_BYTECODE) $(COV_MANAGE_EMIT)
	rm -rf $(JAVA_RT_JAR_IDIR_DIR)
	rm -f $(JAVA_RT_JAR_IDIR_TGZ)
	rm -f $(JAVA_RT_JAR_IDIR_SRC_FILE)
	echo "package ignoreme;" > $(JAVA_RT_JAR_IDIR_SRC_FILE)
	COVERITY_BUILD_INVOCATION_ID= \
	  $(COV_EMIT_JAVA) --dir $(JAVA_RT_JAR_IDIR_DIR) \
		--no-compiler-outputs --enable-decomp \
		--disable-decomp-bodies \
		--fail-on-recoverable-error \
		$(JAVA_RT_JAR_IDIR_SRC_FILE)
	$(COV_MANAGE_EMIT) --dir $(JAVA_RT_JAR_IDIR_DIR) list
	# JFE code expects source file to be in TU 1 (g_shouldDeleteTU1AtEnd)
	$(COV_MANAGE_EMIT) --dir $(JAVA_RT_JAR_IDIR_DIR) --tu 1 print-source-files | grep '[.]java'
	tar czf $(JAVA_RT_JAR_IDIR_TGZ) -C $(JAVA_RT_JAR_IDIR_DIR) .
	rm -rf $(JAVA_RT_JAR_IDIR_DIR)
	rm -f $(JAVA_RT_JAR_IDIR_SRC_FILE)

$(FAST_JRE_JAR_IDIR_TGZ) : $(COV_EMIT_JAVA) $(COV_INTERNAL_EMIT_JAVA_BYTECODE) $(COV_MANAGE_EMIT)
	rm -rf $(FAST_JRE_JAR_IDIR_DIR)
	rm -f $(FAST_JRE_JAR_IDIR_TGZ)
	rm -f $(FAST_JRE_JAR_IDIR_SRC_FILE)
	echo "package ignoreme;" > $(FAST_JRE_JAR_IDIR_SRC_FILE)
	COVERITY_BUILD_INVOCATION_ID= \
	  $(COV_EMIT_JAVA) --dir $(FAST_JRE_JAR_IDIR_DIR) \
		--no-compiler-outputs --enable-decomp \
		--disable-decomp-bodies \
		--fail-on-recoverable-error \
		--use-fast-jre \
		$(FAST_JRE_JAR_IDIR_SRC_FILE)
	$(COV_MANAGE_EMIT) --dir $(FAST_JRE_JAR_IDIR_DIR) list
	# JFE code expects source file to be in TU 1 (g_shouldDeleteTU1AtEnd)
	$(COV_MANAGE_EMIT) --dir $(FAST_JRE_JAR_IDIR_DIR) --tu 1 print-source-files | grep '[.]java'
	tar czf $(FAST_JRE_JAR_IDIR_TGZ) -C $(FAST_JRE_JAR_IDIR_DIR) .
	rm -rf $(FAST_JRE_JAR_IDIR_DIR)
	rm -f $(FAST_JRE_JAR_IDIR_SRC_FILE)

all: fast-jre-emit-db java-rt-emit-db

endif
