SONAME = lua_sysctl
SODIR  = sysctl
SOLIB  = ${SODIR}/core.so

LDFLAGS += -shared -soname ${SONAME}
CFLAGS  += -Wall -Wextra -fPIC `pkg-config --cflags lua-5.1`

all: ${SOLIB}

${SOLIB}: src/${SONAME}.c
	install -m 755 -d ${SODIR}
	${CC} ${LDFLAGS} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

.PHONY: clean

clean:
	rm -f ${SOLIB}
