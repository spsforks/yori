
BINARIES=scut.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:scut.pdb

BIN_OBJS=\
	 scut.obj         \

MOD_OBJS=\
	 mscut.obj     \

compile: $(BIN_OBJS) builtins.lib

scut.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mscut.obj: scut.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ scut.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@
