
BINARIES=ymount.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:ymount.pdb

BIN_OBJS=\
	 mount.obj         \

MOD_OBJS=\
	 mmount.obj     \

compile: $(BIN_OBJS) builtins.lib

ymount.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mmount.obj: mount.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ mount.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@
