SONAME = lua_sysctl
SOLIB = $(SONAME).so

LDFLAGS = -shared -soname $(SONAME)
CFLAGS = -g -fPIC `pkg-config --cflags lua-5.1`

all: $(SOLIB)

$(SONAME).so: $(SONAME).c
		$(CC) $(LDFLAGS) $(CFLAGS) -o $(.TARGET) $(.ALLSRC)

.PHONY: clean

clean:
		rm -f $(SOLIB)
