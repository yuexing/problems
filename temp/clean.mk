# clean.mk
# Implements "make clean"

# get $(OBJ_DIR) and friends
include new-rules/paths.mk

clean::
	$(RM) -r $(OBJ_DIR)

# EOF
