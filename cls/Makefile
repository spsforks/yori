
BINARIES=ycls.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:ycls.pdb

BIN_OBJS=\
	 cls.obj         \

MOD_OBJS=\
	 mcls.obj     \

compile: $(BIN_OBJS) builtins.lib

ycls.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mcls.obj: cls.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ cls.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@

