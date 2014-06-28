# 

# Some environment variables which are useful for debugging will change the
# output produced and cause tests to fail if not suppressed.  This list is by
# no means complete.
unexport USE_OLD_MESSAGES
unexport COVERITY_PRINT_MEM_AT_END
unexport COVERITY_DEBUG
unexport COVERITY_PRINT_ARENA_CREATE_DESTROY
unexport COV_PRINT_EXCEPTIONS

