# Variables that are shared between "cov-analyze.mk" (which can
# generate the DBs) and "checkout.mk" (which checks them out)
JRE_MODELS_DB:=config/jre-models.db
ANDROID_MODELS_DB:=config/android-models.db
CS_SYSTEM_MODELS_DB:=config/cs-system-models.db

EMIT_VER:=$(shell sed -n 's/\#define EMIT_VER \([0-9]*\)/\1/p' libs/build/version.hpp)

# This indicates the schema version for model DBs.
# Must be equal to what's expected by cov-analyze.
# Downloaded model DBs will be upgraded to have this version.
# In general, changing the emit version will change the model schema
# version.
# The reason is that models may contain AST fragments, specifically,
# text-serialized types.
# If you don't change the format of serialized types, then there's no
# need to regenerate the model DBs, so you can just change the number
# here.
# See bug 46648.
MODEL_DB_EXPECTED_VERSION:=121

DB_SCHEMA_VER:=$(shell sed -n 's/const int model_schema_version = \([0-9]*\);/\1/p' libs/parse-errors/model-db.cpp)

# Version detected from source
MODEL_DB_REAL_VERSION:=$(shell echo $$(($(DB_SCHEMA_VER) + $(EMIT_VER))))
