
BINARIES=yps.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:yps.pdb

BIN_OBJS=\
	 ps.obj         \

MOD_OBJS=\
	 mps.obj     \

compile: $(BIN_OBJS) builtins.lib

yps.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mps.obj: ps.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ ps.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@
