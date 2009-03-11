SONAME = lua_sysctl
SOLIB = sysctl/core.so

LDFLAGS = -shared -soname $(SONAME)
CFLAGS = -g -fPIC `pkg-config --cflags lua-5.1`

all: $(SOLIB)

$(SOLIB): src/$(SONAME).c
		$(CC) $(LDFLAGS) $(CFLAGS) -o $(.TARGET) $(.ALLSRC)

.PHONY: clean

clean:
		-rm -f $(SOLIB)
