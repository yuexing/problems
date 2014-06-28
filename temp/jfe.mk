include new-rules/all-rules.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

include symlinks.mk

SOURCE_DIR:=jfe/src

# errinfo

include $(SOURCE_DIR)/prog-errinfo.mk

include $(CREATE_PROG)


# cov-emit-java

include $(SOURCE_DIR)/prog.mk

include $(CREATE_PROG)


endif # NO_JAVA
