
BINARIES=touch.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:touch.pdb

BIN_OBJS=\
	 touch.obj         \

MOD_OBJS=\
	 mtouch.obj     \

compile: $(BIN_OBJS) builtins.lib

touch.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mtouch.obj: touch.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ touch.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@
