# config.guess will return "cygwin", but we want mingw64.
COMPILER_FILES+= x86_64-w64-mingw32
# On win64, some libraries are in there.
# The libraries that are added manually in "mingw-compiler-files.mk"
# are in the x86_64-w64-mingw32 directory instead in mingw64.
COMPILER_LIB_DIR:=lib64
