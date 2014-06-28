include new-rules/all-rules.mk

java-primitives-all: java-primitives java-annotations java-primitives-javadoc java-annotations-javadoc ja-javadoc
all: java-primitives-all

# Primitives
java-primitives: $(JAVA_PRIMITIVES)

JAVA_PRIMITIVES_DIR:=analysis/checkers/models/java/primitives

JAVA_PRIMITIVES_SOURCES:=\
$(wildcard $(JAVA_PRIMITIVES_DIR)/com/coverity/primitives/*.java)

$(JAVA_PRIMITIVES): $(JAVA_PRIMITIVES_SOURCES)
	mkdir -p $(CLASSES_DIR)/primitives-build
	$(JAVAC) -d $(CLASSES_DIR)/primitives-build -source 1.5 -target 1.5 \
		$(JAVA_PRIMITIVES_SOURCES)
	$(JAR) cf $(JAVA_PRIMITIVES) -C $(CLASSES_DIR)/primitives-build .
	rm -rf $(CLASSES_DIR)/primitives-build

# Annotations
java-annotations: $(JAVA_ANNOTATIONS)

JAVA_ANNOTATIONS_SOURCES:=\
$(wildcard $(JAVA_PRIMITIVES_DIR)/com/coverity/annotations/*.java) \
$(wildcard $(JAVA_PRIMITIVES_DIR)/com/coverity/primitives/SensitivePrimitives.java)

$(JAVA_ANNOTATIONS): $(JAVA_ANNOTATIONS_SOURCES)
	mkdir -p $(CLASSES_DIR)/annotations-build
	# Ensure no annotations are using RUNTIME retention; see bug 63593
	grep -l RUNTIME $(JAVA_ANNOTATIONS_SOURCES); test $$? = 1
	$(JAVAC) -d $(CLASSES_DIR)/annotations-build -source 1.5 -target 1.5 \
		$(JAVA_ANNOTATIONS_SOURCES)
	$(JAR) cf $(JAVA_ANNOTATIONS) -C $(CLASSES_DIR)/annotations-build .
	rm -rf $(CLASSES_DIR)/annotations-build

# Primitives Javadoc
JAVA_PRIMITIVES_JAVADOC_DEP:=$(JAVA_PRIMITIVES_JAVADOC_DIR)/index.html

java-primitives-javadoc: $(JAVA_PRIMITIVES_JAVADOC_DEP)

$(JAVA_PRIMITIVES_JAVADOC_DEP): $(JAVA_PRIMITIVES_SOURCES)
	mkdir -p $(JAVA_PRIMITIVES_JAVADOC_DIR)
	$(JAVADOC) -d $(JAVA_PRIMITIVES_JAVADOC_DIR) \
        -nodeprecated \
        -windowtitle "Coverity Static Analysis for Java - Modeling Primitives" \
        -doctitle "<h1>Coverity Static Analysis for Java Modeling Primitives</h1>" \
        -bottom '<i>Copyright &#169; 2007-2012 Coverity, Inc. All rights reserved.</i>' \
        -subpackages com.coverity.primitives \
        -sourcepath $(JAVA_PRIMITIVES_DIR)

# Annotations Javadoc
JAVA_ANNOTATIONS_JAVADOC_DEP:=$(JAVA_ANNOTATIONS_JAVADOC_DIR)/index.html

java-annotations-javadoc: $(JAVA_ANNOTATIONS_JAVADOC_DEP)

$(JAVA_ANNOTATIONS_JAVADOC_DEP): $(JAVA_ANNOTATIONS_SOURCES)
	mkdir -p $(JAVA_ANNOTATIONS_JAVADOC_DIR)
	$(JAVADOC) -d $(JAVA_ANNOTATIONS_JAVADOC_DIR) \
        -nodeprecated \
        -windowtitle "Coverity Static Analysis for Java Annotations" \
        -doctitle "<h1>Coverity Static Analysis for Java Annotations</h1>" \
        -bottom '<i>Copyright &#169; 2007-2012 Coverity, Inc. All rights reserved.</i>' \
        -subpackages com.coverity.annotations \
        -sourcepath $(JAVA_PRIMITIVES_DIR)

# Japanese Javadocs
JA_JAVADOC_ZIP:=$(JAVA_PRIMITIVES_DIR)/../javadocs_ja.zip
JA_JAVADOC_DEP:=$(RELEASE_DOC_DIR)/ja/annotations/index.html

ja-javadoc: $(JA_JAVADOC_DEP)

$(JA_JAVADOC_DEP): $(JA_JAVADOC_ZIP)
	mkdir -p $(RELEASE_DOC_DIR)
	(cd $(RELEASE_ROOT_DIR) && $(JAR) x) < $(JA_JAVADOC_ZIP)
	# careless translators include Thumbs.db crap from Windows
	rm -f $(RELEASE_DOC_DIR)/ja/*/resources/Thumbs.db
	# to prevent repeated re-running
	touch $(JA_JAVADOC_DEP)

