include new-rules/all-rules.mk

# Don't build this tgz if running in POP. It's usually a bad idea to
# run our tools within PoP, and it doesn't help.
POP_BUILD = $(if $(COVERITY_DISENGAGE_EXES),,$(COVERITY_OUTPUT))
# Also don't build if NO_CSHARP, obviously.
ifeq (,$(POP_BUILD)$(NO_CSHARP))

# Inside the command below, we clear COVERITY_BUILD_INVOCATION_ID
# so it will work inside "make -f desktop.mk start-shell".

.PHONY: csharp-rt-emit-db

csharp-rt-emit-db: $(CSHARP_MSCORLIB_DLL_IDIR_TGZ)

# Note: dependency on copying system assemblies now tied to $(COV_EMIT_CSHARP)

$(CSHARP_MSCORLIB_DLL_IDIR_TGZ) : $(COV_EMIT_CSHARP_DEPS)
	rm -rf $(CSHARP_MSCORLIB_DLL_IDIR_DIR)
	rm -f $(CSHARP_MSCORLIB_DLL_IDIR_TGZ)
	rm -f $(CSHARP_MSCORLIB_DLL_IDIR_SRC_FILE)
	echo "namespace ignoreme { struct ignoreme {} };" > $(CSHARP_MSCORLIB_DLL_IDIR_SRC_FILE)
	COVERITY_BUILD_INVOCATION_ID= \
	  $(COV_EMIT_CSHARP) \
		--no-out \
		--dir $(CSHARP_MSCORLIB_DLL_IDIR_DIR) \
		--noconfig \
		--decompilation=metadata \
		--disable-indirect-references \
		--semi-strict \
		$(CSHARP_MSCORLIB_DLL_IDIR_SRC_FILE)
	tar czf $(CSHARP_MSCORLIB_DLL_IDIR_TGZ) -C $(CSHARP_MSCORLIB_DLL_IDIR_DIR) .
	rm -rf $(CSHARP_MSCORLIB_DLL_IDIR_DIR)
	rm -f $(CSHARP_MSCORLIB_DLL_IDIR_SRC_FILE)

all: csharp-rt-emit-db

endif
