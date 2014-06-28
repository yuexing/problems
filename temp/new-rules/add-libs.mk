# 

# This files adds the given library dependencies to the link flags,
# a list of binaries, and adds them as targets
# Arguments:
# DEPENDENCIES: Directory for library dependencies. The directory must contain a "lib.mk".
# See add-lib.mk for format of lib.mk
#
# Output:
#
# LIB_LINK_FLAGS: List of link flags for the created libraries e.g. -lcrypto -lzip
# LIB_EXTRA_LINK_FLAGS: List of link flags for required libraries e.g. -lnettle -lz
#
# LIB_DEPENDENCIES: List of library binary files that the program should depend on
# e.g objs/libs/libcrypto.a objs/libs/libzip.a

# Foreach dependency, include lib.mk
# Then include add-lib.mk

# Save DEPENDENCIES, just in case
ADD_LIBS_DEPENDENCIES:=$(DEPENDENCIES)

$(foreach source_dir,$(ADD_LIBS_DEPENDENCIES), $(eval $(call save_and_clear_build_vars)) $(eval SOURCE_DIR:=$(value source_dir)) $(eval include $$(SOURCE_DIR)/lib.mk $$(ADD_LIB)) $(eval $(call restore_build_vars)))
