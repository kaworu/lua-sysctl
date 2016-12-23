SONAME   = lua_sysctl
BUILDDIR = build
SOLIB    = ${BUILDDIR}/sysctl.so
DESTDIR ?= sysctl

LDFLAGS += -shared -Wl,-soname,${SONAME}
CFLAGS  += -Wall -Wextra -fPIC `pkg-config --cflags lua-5.2`

all: ${SOLIB}

${SOLIB}: src/${SONAME}.c
	install -m 755 -d ${BUILDDIR}
	${CC} ${CFLAGS} -o ${.TARGET} ${LDFLAGS} ${.ALLSRC}

install: ${SOLIB}
	install -m 755 -d ${DESTDIR}
	install -m 644 ${SOLIB} ${DESTDIR}

.PHONY: clean

clean:
	rm -f ${SOLIB}
