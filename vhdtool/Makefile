
BINARIES=vhdtool.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:vhdtool.pdb

BIN_OBJS=\
	 vhdtool.obj         \

MOD_OBJS=\
	 mvhdtool.obj     \

compile: $(BIN_OBJS) builtins.lib

vhdtool.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mvhdtool.obj: vhdtool.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ vhdtool.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@

