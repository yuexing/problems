include new-rules/all-rules.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

all:
	$(ANT) -f java-capture/build.xml -Dobject.directory=../$(OBJ_DIR)

clean::
	$(ANT) -f java-capture/build.xml -Dobject.directory=../$(OBJ_DIR) clean

endif # NO_JAVA
