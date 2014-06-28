include new-rules/all-rules.mk

# Do not build with "NO_JAVA"
ifeq ($(NO_JAVA),)

java-security-da-build:
	$(ANT) -f java-security-da/build.xml -Dobject.directory=../$(OBJ_DIR) -Dpackages.directory=../packages

all: java-security-da-build

clean::
	$(ANT) -f java-security-da/build.xml -Dobject.directory=../$(OBJ_DIR) -Dpackages.directory=../packages clean

endif # NO_JAVA
