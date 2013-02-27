SONAME   = lua_sysctl
BUILDDIR = build
SOLIB    = ${BUILDDIR}/core.so
DESTDIR ?= sysctl

LDFLAGS += -shared -soname ${SONAME}
CFLAGS  += -Wall -Wextra -fPIC `pkg-config --cflags lua-5.1`

all: ${SOLIB}

${SOLIB}: src/${SONAME}.c
	install -m 755 -d ${BUILDDIR}
	${CC} ${CFLAGS} -o ${.TARGET} ${LDFLAGS} ${.ALLSRC}

install: ${SOLIB} sysctl.lua
	install -m 755 -d ${DESTDIR}/sysctl
	install -m 644 ${SOLIB} ${DESTDIR}/sysctl
	install -m 644 sysctl.lua ${DESTDIR}

.PHONY: clean

clean:
	rm -f ${SOLIB}
