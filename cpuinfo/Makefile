
BINARIES=ycpuinfo.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:ycpuinfo.pdb

BIN_OBJS=\
	 cpuinfo.obj         \

MOD_OBJS=\
	 mcpu.obj     \

compile: $(BIN_OBJS) builtins.lib

ycpuinfo.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

mcpu.obj: cpuinfo.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=1 $(CFLAGS) -Fo$@ cpuinfo.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@
