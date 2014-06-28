include new-rules/all-rules.mk

cs-primitives-all: cs-primitives
all: cs-primitives-all

# Primitives
cs-primitives: $(CSHARP_PRIMITIVES)

#LIB:=analysis/checkers/models/cs/primitives

CSHARP_PRIMITIVES_DIR:=analysis/checkers/models/cs/primitives

CSHARP_PRIMITIVES_SOURCES:=\
$(wildcard $(CSHARP_PRIMITIVES_DIR)/*.cs)

$(CSHARP_PRIMITIVES): $(CSHARP_PRIMITIVES_SOURCES)
	LIB= \
	PLATFORM=$(MSBUILD_PLATFORM) \
	OBJ_DIR=$(OBJ_DIR) \
	CSHARP_BIN_DIR=$(RELEASE_ROOT_DIR)/library \
	'$(MSBUILD)' $(CSHARP_PRIMITIVES_DIR)/primitives.csproj
