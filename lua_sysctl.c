#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

/* Include the Lua API header files */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


static int name2oid(char *name, int *oidp);
static int oidfmt(int *oid, int len, char *fmt, u_int *kind);


static int
S_clockinfo(lua_State *L, int l2, void *p)
{
	struct clockinfo *ci = (struct clockinfo *)p;

	if (l2 != sizeof(*ci))
        return (luaL_error(L, "S_clockinfo %d != %d", l2, sizeof(*ci)));

    lua_newtable(L);

    lua_pushinteger(L, ci->hz);
    lua_setfield(L, 2, "hz");
    lua_pushinteger(L, ci->tick);
    lua_setfield(L, 2, "tick");
    lua_pushinteger(L, ci->profhz);
    lua_setfield(L, 2, "profhz");
    lua_pushinteger(L, ci->stathz);
    lua_setfield(L, 2, "stathz");

	return (1);
}


static int
S_loadavg(lua_State *L, int l2, void *p)
{
	struct loadavg *tv = (struct loadavg *)p;

	if (l2 != sizeof(*tv))
        return (luaL_error(L, "S_loadavg %d != %d", l2, sizeof(*tv)));

    lua_newtable(L);

    lua_pushnumber(L, (double)tv->ldavg[0]/(double)tv->fscale);
    lua_setfield(L, 2, "0");
    lua_pushnumber(L, (double)tv->ldavg[1]/(double)tv->fscale);
    lua_setfield(L, 2, "1");
    lua_pushnumber(L, (double)tv->ldavg[2]/(double)tv->fscale);
    lua_setfield(L, 2, "2");

	return (1);
}


static int
S_timeval(lua_State *L, int l2, void *p)
{
#if 0
	struct timeval *tv = (struct timeval *)p;
	time_t tv_sec;
	char *p1, *p2;

	if (l2 != sizeof(*tv)) {
		warnx("S_timeval %d != %d", l2, sizeof(*tv));
		return (1);
	}
	printf(hflag ? "{ sec = %'ld, usec = %'ld } " :
		"{ sec = %ld, usec = %ld } ",
		tv->tv_sec, tv->tv_usec);
	tv_sec = tv->tv_sec;
	p1 = strdup(ctime(&tv_sec));
	for (p2=p1; *p2 ; p2++)
		if (*p2 == '\n')
			*p2 = '\0';
	fputs(p1, stdout);
#endif
	return (0);
}


static int
S_vmtotal(int l2, void *p)
{
#if 0
	struct vmtotal *v = (struct vmtotal *)p;
	int pageKilo = getpagesize() / 1024;

	if (l2 != sizeof(*v)) {
		warnx("S_vmtotal %d != %d", l2, sizeof(*v));
		return (1);
	}

	printf(
	    "\nSystem wide totals computed every five seconds:"
	    " (values in kilobytes)\n");
	printf("===============================================\n");
	printf(
	    "Processes:\t\t(RUNQ: %hd Disk Wait: %hd Page Wait: "
	    "%hd Sleep: %hd)\n",
	    v->t_rq, v->t_dw, v->t_pw, v->t_sl);
	printf(
	    "Virtual Memory:\t\t(Total: %dK, Active %dK)\n",
	    v->t_vm * pageKilo, v->t_avm * pageKilo);
	printf("Real Memory:\t\t(Total: %dK Active %dK)\n",
	    v->t_rm * pageKilo, v->t_arm * pageKilo);
	printf("Shared Virtual Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_vmshr * pageKilo, v->t_avmshr * pageKilo);
	printf("Shared Real Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_rmshr * pageKilo, v->t_armshr * pageKilo);
	printf("Free Memory Pages:\t%dK\n", v->t_free * pageKilo);

#endif
	return (0);
}


static int
T_dev_t(int l2, void *p)
{
#if 0
	dev_t *d = (dev_t *)p;

	if (l2 != sizeof(*d)) {
		warnx("T_dev_T %d != %d", l2, sizeof(*d));
		return (1);
	}
	if ((int)(*d) != -1) {
		if (minor(*d) > 255 || minor(*d) < 0)
			printf("{ major = %d, minor = 0x%x }",
				major(*d), minor(*d));
		else
			printf("{ major = %d, minor = %d }",
				major(*d), minor(*d));
	}
#endif
	return (0);
}


static int
luaA_sysctl(lua_State *L)
{
    int nlen, i, oid[CTL_MAXNAME];
    size_t len;
    char fmt[BUFSIZ], key[BUFSIZ];
    u_int kind, *val, *oval;
    int (*func)(lua_State *, int, void *);

    strlcpy(key, luaL_checkstring(L, 1), sizeof(key));; /* get first argument from lua */

	nlen = name2oid(key, oid);

	if (nlen < 0)
        return (luaL_error(L, "unknown iod '%s'", key));

	if (oidfmt(oid, nlen, fmt, &kind))
		return (luaL_error(L, "couldn't find format of oid '%s'", key));

	/* find an estimate of how much we need for this var */
    len = 0;
    i = sysctl(oid, nlen, 0, &len, 0, 0);
	len += len; /* we want to be sure :-) */

	val = oval = malloc(len + 1);

	i = sysctl(oid, nlen, val, &len, 0, 0);
	if (i || !len) {
		free(oval);
		return (luaL_error(L, "sysctl(3) failed"));
	}
	val[len] = '\0';

    switch (kind & CTLTYPE) {
    case CTLTYPE_NODE:
        free(oval);
		return (luaL_error(L, "unsupported operation")); // FIXME: should return a table etc.
    case CTLTYPE_INT:
    case CTLTYPE_UINT:
    case CTLTYPE_LONG:
    case CTLTYPE_ULONG:
        lua_pushinteger(L, *(lua_Integer *)val);
        break;
    case CTLTYPE_QUAD:
        lua_pushnumber(L, *(lua_Number *)val);
        break;
    case CTLTYPE_STRING:
        lua_pushstring(L, (char *)val);
        break;
    case CTLTYPE_OPAQUE:
		if (strcmp(fmt, "S,clockinfo") == 0)
			func = S_clockinfo;
		else if (strcmp(fmt, "S,loadavg") == 0)
			func = S_loadavg;
		else if (strcmp(fmt, "S,timeval") == 0)
			func = S_timeval;
		else if (strcmp(fmt, "S,vmtotal") == 0)
			func = S_vmtotal;
		else if (strcmp(fmt, "T,dev_t") == 0)
			func = T_dev_t;
		else
            func = NULL;

        if (func)
            (*func)(L, len, val);
        else {
            free(oval);
		    return (luaL_error(L, "unknown CTLTYPE: %s", fmt)); // FIXME
        }
        break;
    default:
        free(oval);
		return (luaL_error(L, "unknown CTLTYPE: %s", fmt)); // FIXME
    }

    free(oval);
    lua_pushstring(L, fmt);
	return (2); /* two returned value */
}


/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
static int
name2oid(char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = 0;
	oid[1] = 3;

	j = CTL_MAXNAME * sizeof(int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0)
		return (i);
	j /= sizeof(int);
	return (j);
}


static int
oidfmt(int *oid, int len, char *fmt, u_int *kind)
{
	int qoid[CTL_MAXNAME+2];
	u_char buf[BUFSIZ];
	int i;
	size_t j;

	qoid[0] = 0;
	qoid[1] = 4;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if (i)
        return (1);

	if (kind)
		*kind = *(u_int *)buf;

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));
	return (0);
}


/* Lua initialisation stuff */


static const luaL_reg lua_sysctl[] =
{
    {"query",        luaA_sysctl},
    {NULL,          NULL}
};

/*
 * Open our library
 */
LUALIB_API int
luaopen_sysctl(lua_State *L)
{
    luaL_openlib(L, "sysctl", lua_sysctl, 0);
    return (1);
}
