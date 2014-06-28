include new-rules/all-rules.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

java-anttask-build:
	$(ANT) -f java-anttask/build.xml -Dobject.directory=../$(OBJ_DIR)

all: java-anttask-build

clean::
	$(ANT) -f java-anttask/build.xml -Dobject.directory=../$(OBJ_DIR) clean

endif # NO_JAVA

