
BINARIES=yhash.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:yhash.pdb

BIN_OBJS=\
	 hash.obj         \

MOD_OBJS=\
	 mhash.obj     \

compile: $(BIN_OBJS) builtins.lib

yhash.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mhash.obj: hash.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ hash.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@

