
BINARIES=yattrib.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:yattrib.pdb

BIN_OBJS=\
	 attrib.obj         \

MOD_OBJS=\
	 mattrib.obj     \

compile: $(BIN_OBJS) builtins.lib

yattrib.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mattrib.obj: attrib.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ attrib.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@

