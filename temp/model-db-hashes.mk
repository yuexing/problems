# md5sums of the model DBs in the "config" directory
# See copy-model-db-{to,from}-cvs in cov-analyze.mk

# It's in its own file so that it can be marked as a dependency of the
# model DB files themselves
# ****************************
# ***************************
# IMPORTANT NOTE: IF YOU ARE CHANGING THE HASHES HERE, REMEMBER TO DO A "make copy-models"
# The build will fail if you push without copying the models.
# ****************************
# ***************************
JRE_MODELS_DB_HASH:=902831af64c46bc25921290150a6ef24
ANDROID_MODELS_DB_HASH:=6ffafd33a3f72655faec13d861db8dda
CS_SYSTEM_MODELS_DB_HASH:=d54ae75e281b3ed9214aca56a270e0c8
