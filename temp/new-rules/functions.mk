# 

# functions.mk
# Utility functions to write makefiles

# all-rules.mk should be included first
ifeq (1,$(ALL_RULES_INCLUDES))


# --------------------------- push, pop, etc. --------------------------------
# Stack functions.
# Allows writing recursive includes
# They use side effects
#
# Typical syntax:
#
#   $(call push,my_var_stack,$(my_var))
#   my_var:=$(pop my_var_stack)
#
# Which can also be written:
#
#   $(call save_var,my_var)
#   $(call restore_var,my_var)

ifneq ($(IS_WINDOWS),1)
COVCP:=cp
else

# We specifically disable the Cygwin NT permissions
# security mappings madness on copies to avoid losing
# Windows specific permissions necessary for some binaries
# and other files in the release, like Apache
COVCP:=CYGWIN=nontsec cp
endif

# expands to the empty string
EMPTY:=

# expands to a single space character
SPACE:= $(EMPTY) $(EMPTY)

# sequence used to encode spaces so they don't interfere with 'make'
# parsing rules
SPACE_ESCAPE:=<space>
EMPTY_ESCAPE:=<empty>

# Escape spaces (by turning them into $(SPACE_ESCAPE))
escape=$(if $(1),$(subst $(SPACE),$(SPACE_ESCAPE),$(1)),$(EMPTY_ESCAPE))
unescape=$(subst $(SPACE_ESCAPE),$(SPACE),$(subst $(EMPTY_ESCAPE),,$(1)))

# Push by adding in front of the "stack" (space-separated list). Spaces must be escaped.
push=$(eval $$(1):=$$(call escape,$$(2)) $$($$(1)))
# pop by evaluating unescaped front of list, and removing the first element
# Note that breaking this line will introduce an unwanted space at the end of the variable...
pop=$(call unescape,$(firstword $($(1))))$(eval $$(1):=$$(wordlist 2,$$(words $$($$(1))),$$($$(1))))

define save_and_clear_var_uneval
$$(call push,$(value 1)_save_var_stack,$$($(value 1)))
$(value 1):=
endef
save_and_clear_var=$(eval $(call save_and_clear_var_uneval,$(1)))

save_var=$(call push,$(1)_save_var_stack,$($(1)))
restore_var=$(eval $$(1):=$$(call pop,$$(1)_save_var_stack))


# -------------------------------- add_generic_file ----------------------------
# Create a rule for a C or C++ file. Used by build.mk
#
# arg 1 = source file
# arg 2 = source extension (.c / .cpp)
# arg 3 = compiler var name (CC / CXX)
# arg 4 = compiler flags var prefix (C / CXX). Also prefix for _OBJS, _DEPS
#
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
#   the objects being created (e.g. packages, to get appropriate headers)
# EXTRA_ORDER_DEPENDENCIES: Same as above, but ordering only
# EXTRA_SHARED_CFLAGS: Additional CFLAGS.
#
# BUILD_OBJ_DIR = directory for objects
# It will mirror the hierarchy for sources (starting from root)
#
# This will be "eval"'d so escape $, "value" arguments
define add_generic_file

SOURCE:=$(value 1)

# strip the extension, prepend the objdir
OBJ_FILE_STUB:=$$(BUILD_OBJ_DIR)/$$(patsubst %$(2),%,$$(SOURCE))

# name of the variable containing file-specific flags
SRC_FILE_FLAGS_VAR:=$$(call file_name_to_flags_var_name,$$(SOURCE))
#$$(info $$(SRC_FILE_FLAGS_VAR) is $$($$(SRC_FILE_FLAGS_VAR)))
SRC_FILE_PRE_FLAGS_VAR:=$$(call file_name_to_preflags_var_name,$$(SOURCE))
#$$(info $$(SRC_FILE_PRE_FLAGS_VAR) is $$($$(SRC_FILE_PRE_FLAGS_VAR)))

$$(OBJ_FILE_STUB).o: PRE_FILE_FLAGS:=$$($$(SRC_FILE_PRE_FLAGS_VAR))
$$(OBJ_FILE_STUB).o: PRE_EXTRA_FLAGS:=$$(PRE_EXTRA_SHARED_CFLAGS)
$$(OBJ_FILE_STUB).o: EXTRA_FLAGS:=$$(EXTRA_$(4)FLAGS) $$(EXTRA_SHARED_CFLAGS) $$($$(SRC_FILE_FLAGS_VAR))
$$(OBJ_FILE_STUB).o: OBJ_FILE_STUB:=$$(OBJ_FILE_STUB)
$$(OBJ_FILE_STUB).o: GLOBAL_FLAGS:=$$($(4)FLAGS) $$(SHARED_CFLAGS)
$$(OBJ_FILE_STUB).o: EXTRA_COMPILE_ONLY_CFLAGS:=$$(SHARED_COMPILE_ONLY_CFLAGS)
$$(OBJ_FILE_STUB).o: COMPILER:=$$($(3))

ifneq ($$(value PREPROCESS_OPTS),)

# We want to preprocess. The target doesn't count (it may not even
# refer to a real source file if PREPROCESS_OPTS doesn't contain $(SOURCE))
.PHONY: $$(OBJ_FILE_STUB).o

$$(OBJ_FILE_STUB).o:
	$$(POP_TRANSLATE) $$(CCACHE) $$(COMPILER) $$(PRE_EXTRA_FLAGS) $$(GLOBAL_FLAGS) $$(EXTRA_FLAGS) $$(EXTRA_COMPILE_ONLY_CFLAGS) $$(PREPROCESS_OPTS)

else

# the -MP option to gcc will create "dummy" rules for all the headers, so
# 'make' doesn't complain if a header file has disappeared
#
# SGM 2007-02-07: I moved $$(EXTRA_FLAGS) to be last so that they can
# override both $($(4)FLAGS) *and* $$(SHARED_CFLAGS); in general, the
# more-specific flags should be allowed to override the more-general
# flags.
#
# Looking into bk history, I see that in rev 1.9 Charles moved
# $$(SHARED_CFLAGS) to be last, but the comment ("build fix") does not
# explain why.
#
# 2007-02-20: Ok, I think the reason is that sqlite3.h used to be in
# both source-packages and binary-packages, and we wanted the one in
# source-packages, so its -I was added to $$(EXTRA_FLAGS).  The problem
# is that -I overrides on the left, whereas -D and -O override on the
# right.  :(  However, since sqlite3 has been removed from
# binary-packages, and in general we will try to avoid those conflicts,
# I will stick with $$(EXTRA_FLAGS) being last.
#
$$(OBJ_FILE_STUB).o: $$(SOURCE) $$(EXTRA_DEPENDENCIES) \
| $$(EXTRA_ORDER_DEPENDENCIES) $(POP_CONFIG)
	@mkdir -p $$(dir $$@)
	$$(POP_TRANSLATE) $$(CCACHE) $$(COMPILER) $$(PRE_FILE_FLAGS) $$< $$(PRE_EXTRA_FLAGS) $$(GLOBAL_FLAGS) $$(EXTRA_FLAGS) $$(EXTRA_COMPILE_ONLY_CFLAGS) -c -o $$@
	$$(COMPILER) $$(PRE_FILE_FLAGS) $$< $$(PRE_EXTRA_FLAGS) $$(GLOBAL_FLAGS) $$(EXTRA_FLAGS) -MQ "$$(OBJ_FILE_STUB).o" -E -MM -MP -o $$(OBJ_FILE_STUB).d

# We have no rules to create the .d files, so we need -include (or we'll get an error).
# We're still good as long as .d and .o files are always present at the same time.
-include $$(OBJ_FILE_STUB).d

endif # PREPROCESS_OPTS

$(4)_OBJS+= $$(OBJ_FILE_STUB).o
$(4)_DEPS+= $$(OBJ_FILE_STUB).d

endef # add_generic_file

define add_cxx_file
$(call add_generic_file,$(value 1),.cpp,CXX,CXX)
endef

define add_cxx2_file
$(call add_generic_file,$(value 1),.cxx,CXX,CXX)
endef

define add_cc_file
$(call add_generic_file,$(value 1),.cc,CXX,CXX)
endef

define add_c_file
$(call add_generic_file,$(value 1),.c,CC,C)
endef

# Add a set of .ast files to the build
# See the description of AST_SOURCES in build.mk
# Takes 2 parameters:
# - A list of .ast files, relative to the root.
# - A name for a subdirectory of $(GENSRC_DIR), to store the resulting files
# It creates a rule to generate the files, and adds the generated C++ file
# to GEN_SOURCES
define add_ast_files

# List of .ast files, relative to the root
AST_SOURCES:=$(value 1)

# First file in list, for naming the result
FIRST_AST_FILE:=$$(word 1, $$(AST_SOURCES))

# Header file
AST_HEADER:=$$(FIRST_AST_FILE).hpp
# Fwd header file
AST_HEADER_FWD:=$$(FIRST_AST_FILE)-fwd.hpp
# C++ file, relative to GENSRC_DIR
AST_CXX_SOURCE:=$(value 2)/$$(notdir $$(FIRST_AST_FILE)).cpp

# Add the C++ file to GEN_SOURCES
GEN_SOURCES:=$$(GEN_SOURCES) $$(AST_CXX_SOURCE)

# Now make it relative to root
AST_CXX_SOURCE:=$$(GENSRC_DIR)/$$(AST_CXX_SOURCE)

$$(AST_CXX_SOURCE): FILE_BASE:=$$(FIRST_AST_FILE)
$$(AST_CXX_SOURCE): AST_SOURCES:=$$(AST_SOURCES)
$$(AST_CXX_SOURCE): HEADER_DIR:=$$(dir $$(AST_HEADER))

# "make" doesn't appear to have a way to tell that a single command
# produces 2 files.
# So, instead, use one of the files as target, and have the other
# file depend on the first file, being created by doing nothing.
$$(AST_CXX_SOURCE): $$(ASTGEN) $$(AST_SOURCES)
	$$(ASTGEN) -o $$(FILE_BASE) -h $$(HEADER_DIR) $$(AST_SOURCES)
	mkdir -p $$(dir $$@)
	mv $$(FILE_BASE).cpp $$@

$$(AST_HEADER): $$(AST_CXX_SOURCE)
	@ # Nothing to do, file is created along $$(AST_CXX_SOURCE)

$$(AST_HEADER_FWD): $$(AST_CXX_SOURCE)
	@ # Nothing to do, file is created along $$(AST_CXX_SOURCE)

endef


# Add a of .y file to the build
# Takes 2 parameters:
# - A .y file, relative to the root.
# - A name for a subdirectory of $(GENSRC_DIR), to store the resulting files
# It creates a rule to generate the files, and adds the generated C++ file
# to GEN_SOURCES
define add_bison_file

BISON_FILE:=$(value 1)

BISON_HEADER:=$$(patsubst %.y,%.tab.hpp,$$(BISON_FILE))
BISON_HEADER_FWD:=$$(patsubst %.y,%.tab-fwd.h,$$(BISON_FILE))

# C++ file, relative to GENSRC_DIR
BISON_CXX_SOURCE:=$(value 2)/$$(notdir $$(patsubst %.y,%.tab.cpp,$$(BISON_FILE)))

# Add the C++ file to GEN_SOURCES
GEN_SOURCES:=$$(GEN_SOURCES) $$(BISON_CXX_SOURCE)

# Now make it relative to root
BISON_CXX_SOURCE:=$$(GENSRC_DIR)/$$(BISON_CXX_SOURCE)

$$(BISON_CXX_SOURCE): BISON_OUTPUT_BASE:=$$(patsubst %.cpp,%,$$(BISON_CXX_SOURCE))
$$(BISON_CXX_SOURCE): BISON_HEADER:=$$(BISON_HEADER)
$$(BISON_CXX_SOURCE): BISON_HEADER_FWD:=$$(BISON_HEADER_FWD)

$$(BISON_CXX_SOURCE): $$(BISON_FILE) $$(BISON)
	mkdir -p $$(dir $$@)
	$$(BISON) -d -v $$< -o $$(BISON_OUTPUT_BASE).orig.cpp
	@ #
	@ # give the union a tag name so I can forward-declare it, and
	@ # suppress the attribute as g++ does not like it
	sed -e 's/typedef union {/typedef union YYSTYPE {/' \
	    -e 's/__attribute__ ((__unused__))//' \
	  $$(BISON_OUTPUT_BASE).orig.cpp \
	  > $$@
	echo "/* 
	@ # Normnalize the header guard, which seems to be based on the
	@ # output file name
	sed -e 's/typedef union {/typedef union YYSTYPE {/' \
	    -e 's/OBJS_.*_GENSRC/OBJS_GENSRC/' \
	  $$(BISON_OUTPUT_BASE).orig.hpp \
	  >> $$(BISON_HEADER)
	@ #
	@ # pull out the #defines so I can get them w/o dependencies
	grep '(c)\|# *define' $$(BISON_HEADER) \
	  > $$(BISON_HEADER_FWD)
	@#
	@# clean up
	$$(RM) $$(BISON_OUTPUT_BASE).orig.*

# See similar thing in the "astgen" case above.
$$(BISON_HEADER): $$(BISON_CXX_SOURCE)
	@true

$$(BISON_HEADER_FWD): $$(BISON_CXX_SOURCE)
	@true

endef


# Add a of .lex file to the build
# Takes 2 parameters:
# - A .lex file, relative to the root.
# - A name for a subdirectory of $$(GENSRC_DIR), to store the resulting files
# It creates a rule to generate the files, and adds the generated C++ file
# to GEN_SOURCES
define add_flex_file

FLEX_FILE:=$(value 1)

# C++ file, relative to GENSRC_DIR
FLEX_CXX_SOURCE:=$(value 2)/$$(notdir $$(patsubst %.lex,%.yy.cpp,$$(FLEX_FILE)))

# Add the C++ file to GEN_SOURCES
GEN_SOURCES:=$$(GEN_SOURCES) $$(FLEX_CXX_SOURCE)

# Now make it relative to root
FLEX_CXX_SOURCE:=$$(GENSRC_DIR)/$$(FLEX_CXX_SOURCE)

$$(FLEX_CXX_SOURCE): $$(FLEX_FILE) $$(FLEX) utilities/run-flex.pl
	mkdir -p $$(dir $$@)
	FLEX=$(TEST_BIN_DIR)/flex perl utilities/run-flex.pl -o$$@ $$<

endef

msbuild-check:
	@if ! test -f "$(MSBUILD)"; then \
	  echo "Could not find MSBuild from Microsoft .NET/Windows SDK for Framework 4.x.";\
	  echo "It is a free download." ;\
	  false; \
	fi

.PHONY: msbuild-check
ifeq ($(.DEFAULT_GOAL),msbuild-check)
# Prevent the msbuild-check target from being made the implicit default target
.DEFAULT_GOAL:=
endif

# -------------------------- add_msbuild ------------------------------
# Creates a rule to generate $(1) from $(CS_SOURCES) and $(CSPROJ) in
# $(SOURCE_DIR)
# Arguments:
# 1 = target
# SOURCES = source prerequisites, relative to $(SOURCE_DIR)
# CSPROJ = .csproj file to build, relative to
# $(SOURCE_DIR). Optional; if not set, defaults to $(NAME).csproj
# SOURCE_DIR = directory containing the .csproj file

# The .csproj file should contain something like this:

#  <PropertyGroup>
#    <AssemblyName>cov-internal-emit-cs-source</AssemblyName>
#    <RepoRoot>..\..</RepoRoot>
#  </PropertyGroup>
#  <Import Project="$(RepoRoot)/new-rules/msbuildvars.xml"/>

# We cannot include "progname" in "OBJ_DIR" (as passed to msbuild)
# because csproj files may include each other, so OBJ_DIR must be
# globally the same.
# Note that OBJ_DIR is relative to the root while msbuild paths are
# relative to the directory containing the .csproj file, hence the
# need for a RepoRoot.

define add_msbuild

ifeq ($$(CSPROJ),)
CSPROJ:=$$(NAME).csproj
endif

ifeq ($$(CS_PLATFORM),)
CS_PLATFORM:=$$(MSBUILD_PLATFORM)
endif

CSPROJ:=$$(SOURCE_DIR)/$$(CSPROJ)

$(value 1): CSPROJ:=$$(CSPROJ)
$(value 1): CS_PLATFORM:=$$(CS_PLATFORM)
$(value 1): SOURCE_DIR:=$$(SOURCE_DIR)

CS_SOURCES:=$$(foreach src,$$(SOURCES),$$(SOURCE_DIR)/$$(src))


# The output from msbuild/csc will write file names relative to the
# project's directory, even when run from a different directory (we
# run it from the repository root)
# Pipe through 'sed' to fix that

# I'm not sure how "LIB" gets into the environment but it confuses
# msbuild, so we reset it to empty

# Depend on libraries, since they need to be compiled before they're
# referenced
#
# TODO: Currently, this puts .pdb files in the "bin" directory. Maybe
# we'll need to do something about this.
$(value 1): $$(CS_SOURCES) $$(CSPROJ) $$(LIB_DEPENDENCIES) | msbuild-check
	LIB= \
	MC_PLATFORM=$$(mc_platform) \
	PLATFORM=$$(CS_PLATFORM) \
	CSHARP_BIN_DIR=$$(CSHARP_BIN_DIR) \
	OBJ_DIR=$$(OBJ_DIR) \
	MSBuildUseNoSolutionCache=1 \
	'$$(MSBUILD)' \
		/consoleloggerparameters:Verbosity=minimal \
		$$(MSBUILD_OPTS) $$(CSPROJ) \
	| sed 's%^\( *\)\([-0-9a-zA-Z_./]*\.cs\)%\1$$(SOURCE_DIR)/\2%'; \
	exit $$$${PIPESTATUS[0]}

# Set "OBJS" to the intermediate file, for "make clean"
OBJS:=$(value 1)

endef


# -------------------------- add_obj_to_lib ------------------------------
# When eval'd with argument 1 being an object
# and arg 2 a library, creates a rule to add the object to the library,
# and adds the library member target to LIB_MEMBERS

# This is not used as it doesn't work with make -j which I plan to use.
define add_obj_to_lib
OBJ:=$(value 1)
LIB:=$(value 2)
MEMBER:=$$(LIB)($$(notdir $$(OBJ)))
$$(MEMBER):$$(OBJ)
	@mkdir -p $$(dir $$@)
	ar -ru $$@ $$<

LIB_MEMBERS+= $$(MEMBER)

endef # add_obj_to_lib


# ------------------------ setup_file_copy_2_dirs ------------------------
# Arguments: src_dir, dst_dir, file
#   1: src_dir is relative to repo root
#   2: dst_dir is relative to release root
#   3: file is the common part of the name; it can have directory info
#
# Dirs must end with / (or be empty)
#
# Calling this function will add a target to create a copy:
#
#   $(eval $(call setup_file_copy_2_dirs,dir1/,dir2/,file))
#
# will create a copy of <source repo>/dir1/file in <obj dir>/dir2/file
#
# (At one point, this function would arrange to create symlinks on
# unix, but that causes problems when moving the repository; and
# hardlinks would introduce dependencies on physical filesystem
# organization.  So we decided to just always make copies.)
#
define setup_file_copy_2_dirs
SOURCE:=$(value 1)$(value 3)
TARGET:=$$(RELEASE_ROOT_DIR)/$(value 2)$(value 3)

TARGET_DIR:=$$(patsubst %/,%,$$(dir $$(TARGET)))

# Create a guard for the directory creation
# Note that a variable name can have any character besides #, :, = and
# whitespace, so using the directory name directly is safe.

ifeq ($$($$(TARGET_DIR)),)

$$(TARGET_DIR):=1

$$(TARGET_DIR):
	mkdir -p "$$@"

endif

$$(TARGET): $$(SOURCE) | $$(TARGET_DIR)
	$(COVCP) -f "$$<" "$$@"

ALL_TARGETS+=$$(TARGET)
all:$$(TARGET)

SYMLINK_FILES+=$$(TARGET)

endef # setup_file_copy_2_dirs

# the set of files that are to be copied ("symlink" terminology is
# from an earlier design); initialized to empty, and as an
# early-expansion (":=") variable type
SYMLINK_FILES:=


# ----------------------- set_debug_src_file -----------------------
# Set a particular source file to be compiled with debug info.
#
# Arguments:
#   1: name of the source file, relative to the repo root, and
#      including its extension
#
define inner_set_debug_src_file

SOURCE:=$(value 1)
$$(info Debug compile: $$(SOURCE))

# Map the file name to its flags variable name.  The same procedure
# is done by 'add_generic_file', above.
SRC_FILE_FLAGS_VAR:=$$(call file_name_to_flags_var_name,$$(SOURCE))
#$$(info Variable: $$(SRC_FILE_FLAGS_VAR))

# Set it to compile w/ debug info.  Disabling optimization depends on
# this variable's flags coming *after* the default flags.
$$(SRC_FILE_FLAGS_VAR):=-g -O0

endef # inner_set_debug_src_file


# As above, but this time with wildcard expansion of the argument.
define set_debug_src_file

# This expansion is also useful because it lets us check for typos
# in file names early.
SOURCE:=$$(wildcard $(value 1))

ifeq ($$(SOURCE),)
$$(error Does not match a file name: $(value 1))
else
$$(foreach file,$$(SOURCE),$$(eval $$(call inner_set_debug_src_file,$$(file))))
endif

endef # set_debug_src_file


# transform a source file into the name of the per-file flags
define file_name_to_flags_var_name
$(subst -,_,$(subst /,_,$(subst .,_,$(value 1))))_FLAGS
endef # file_name_to_flags_var_name

define file_name_to_preflags_var_name
$(subst -,_,$(subst /,_,$(subst .,_,$(value 1))))_PRE_FLAGS
endef # file_name_to_flags_var_name

# ------------------------- save_and_clear_build_vars ------------------------
# Save the state of build variables to the stack.
define save_and_clear_build_vars
$(call save_and_clear_var,NAME)
$(call save_and_clear_var,SOURCES)
$(call save_and_clear_var,GEN_SOURCES)
$(call save_and_clear_var,SOURCE_DIR)
$(call save_and_clear_var,EXTRA_CXXFLAGS)
$(call save_and_clear_var,EXTRA_CFLAGS)
$(call save_and_clear_var,EXTRA_SHARED_CFLAGS)
$(call save_and_clear_var,PRE_EXTRA_SHARED_CFLAGS)
$(call save_and_clear_var,EXTRA_LDFLAGS)
$(call save_and_clear_var,EXTRA_OBJS)
$(call save_and_clear_var,EXTRA_DEPENDENCIES)
$(call save_and_clear_var,EXTRA_ORDER_DEPENDENCIES)
endef # save_and_clear_build_vars

# ----------------------- restore_build_vars -----------------------
# Restore the state of build variables to the stack.
define restore_build_vars
$(call restore_var,NAME)
$(call restore_var,SOURCES)
$(call restore_var,GEN_SOURCES)
$(call restore_var,SOURCE_DIR)
$(call restore_var,EXTRA_CXXFLAGS)
$(call restore_var,EXTRA_CFLAGS)
$(call restore_var,EXTRA_SHARED_CFLAGS)
$(call restore_var,PRE_EXTRA_SHARED_CFLAGS)
$(call restore_var,EXTRA_LDFLAGS)
$(call restore_var,EXTRA_OBJS)
$(call restore_var,EXTRA_DEPENDENCIES)
$(call restore_var,EXTRA_ORDER_DEPENDENCIES)
endef # restore_build_vars

# See Bug 57233 - Rebuilding C# models on Windows is not equivalent to rebuilding them on Linux
# Turns out that we don't want to build JRE or Android models on Windows either.
# Several places depend on these rules to make sure that we exit as soon as
# possible (i.e. don't spend a long time running things before exiting with an
# error).
# Will exit immediately (with error 2 and an appropriate error message) if
# this is not being run on linux64
testsuite-shipping-models-linux64-check:
ifeq (1,$(ENABLE_BUILDING_LIB_MODELS))
	exit 0;
else
	@echo "Shipping models can only be regenerated on linux64, not $(mc_platform).";exit 2;
endif

# ---------------------------- prologue --------------------------
else

$(error You should include all-rules.mk)

endif
