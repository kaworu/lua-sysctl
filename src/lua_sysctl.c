#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/vmmeter.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Include the Lua API header files */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int name2oid(char *name, int *oidp);
static int oidfmt(int *oid, int len, char *fmt, u_int *kind);
static int set_IK(char *str, int *val);


static int
S_clockinfo(lua_State *L, int l2, void *p)
{
    struct clockinfo *ci = (struct clockinfo *)p;

    if (l2 != sizeof(*ci))
        return (luaL_error(L, "S_clockinfo %d != %d", l2, sizeof(*ci)));

    lua_newtable(L);

    lua_pushinteger(L, ci->hz);
    lua_setfield(L, -2, "hz");
    lua_pushinteger(L, ci->tick);
    lua_setfield(L, -2, "tick");
    lua_pushinteger(L, ci->profhz);
    lua_setfield(L, -2, "profhz");
    lua_pushinteger(L, ci->stathz);
    lua_setfield(L, -2, "stathz");

    return (1);
}


static int
S_loadavg(lua_State *L, int l2, void *p)
{
    struct loadavg *la = (struct loadavg *)p;
    int i;

    if (l2 != sizeof(*la))
        return (luaL_error(L, "S_loadavg %d != %d", l2, sizeof(*la)));

    lua_newtable(L);

    for (i = 0; i < 3; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushnumber(L, (double)la->ldavg[i]/(double)la->fscale);
        lua_settable(L, -3);
    }

    return (1);
}


static int
S_timeval(lua_State *L, int l2, void *p)
{
    struct timeval *tv = (struct timeval *)p;
    time_t tv_sec;
    char *p1, *p2;

    if (l2 != sizeof(*tv))
        return (luaL_error(L, "S_timeval %d != %d", l2, sizeof(*tv)));

    lua_newtable(L);

    lua_pushinteger(L, tv->tv_sec);
    lua_setfield(L, -2, "sec");
    lua_pushinteger(L, tv->tv_usec);
    lua_setfield(L, -2, "usec");

    return (1);
}


static int
S_vmtotal(lua_State *L, int l2, void *p)
{
    struct vmtotal *v = (struct vmtotal *)p;
    int pageKilo = getpagesize() / 1024;

    if (l2 != sizeof(*v))
        return (luaL_error(L, "S_vmtotal %d != %d", l2, sizeof(*v)));

    lua_newtable(L);

    lua_pushinteger(L, v->t_rq);
    lua_setfield(L, -2, "rq");
    lua_pushinteger(L, v->t_dw);
    lua_setfield(L, -2, "dw");
    lua_pushinteger(L, v->t_pw);
    lua_setfield(L, -2, "pw");
    lua_pushinteger(L, v->t_sl);
    lua_setfield(L, -2, "sl");

    lua_pushinteger(L, v->t_vm * pageKilo);
    lua_setfield(L, -2, "vm");
    lua_pushinteger(L, v->t_avm * pageKilo);
    lua_setfield(L, -2, "avm");

    lua_pushinteger(L, v->t_rm * pageKilo);
    lua_setfield(L, -2, "rm");
    lua_pushinteger(L, v->t_arm * pageKilo);
    lua_setfield(L, -2, "arm");

    lua_pushinteger(L, v->t_vmshr * pageKilo);
    lua_setfield(L, -2, "vmshr");
    lua_pushinteger(L, v->t_avmshr * pageKilo);
    lua_setfield(L, -2, "avmshr");

    lua_pushinteger(L, v->t_rmshr * pageKilo);
    lua_setfield(L, -2, "rmshr");
    lua_pushinteger(L, v->t_armshr * pageKilo);
    lua_setfield(L, -2, "armshr");

    lua_pushinteger(L, v->t_free * pageKilo);
    lua_setfield(L, -2, "free");

    return (1);
}


static int
T_dev_t(lua_State *L, int l2, void *p)
{
    dev_t *d = (dev_t *)p;

    if (l2 != sizeof(*d))
        return (luaL_error(L, "T_dev_t %d != %d", l2, sizeof(*d)));

    lua_newtable(L);

    if ((int)(*d) != -1) {
        lua_pushinteger(L, minor(*d));
        lua_setfield(L, -2, "minor");
        lua_pushinteger(L, major(*d));
        lua_setfield(L, -2, "major");
    }

    return (1);
}


static int
luaA_sysctl_set(lua_State *L)
{
    int nlen, intval, oid[CTL_MAXNAME];
    size_t len, newsize = 0;
    u_int kind;
    char fmt[BUFSIZ], buf0[BUFSIZ], buf1[BUFSIZ], *endptr;
    void *newval = NULL;

    /* get first argument from lua */
    len = strlcpy(buf0, luaL_checkstring(L, 1), sizeof(buf0));
    if (len > sizeof(buf0))
        return (luaL_error(L, "first arg too long"));

    /* get second argument from lua */
    len = strlcpy(buf1, luaL_checkstring(L, 2), sizeof(buf1));
    if (len > sizeof(buf1))
        return (luaL_error(L, "second arg too long"));
    newval = buf1;

    nlen = name2oid(buf0, oid);
    if (nlen < 0)
        return (luaL_error(L, "unknown iod '%s'", buf0));

    if (oidfmt(oid, nlen, fmt, &kind))
        return (luaL_error(L, "couldn't find format of oid '%s'", buf0));

    if ((kind & CTLTYPE) == CTLTYPE_NODE)
        return (luaL_error(L, "oid '%s' isn't a leaf node", buf0));

    if (!(kind & CTLFLAG_WR))
        return (luaL_error(L, "oid '%s' is %s", buf0,
                    (kind & CTLFLAG_TUN) ? "read only tunable" : "read only"));

    if ((kind & CTLTYPE) == CTLTYPE_INT ||
            (kind & CTLTYPE) == CTLTYPE_UINT ||
            (kind & CTLTYPE) == CTLTYPE_LONG ||
            (kind & CTLTYPE) == CTLTYPE_ULONG ||
            (kind & CTLTYPE) == CTLTYPE_QUAD) {
        if (strlen(newval) == 0)
            return (luaL_error(L, "empty numeric value"));
    }


    switch (kind & CTLTYPE) {
    case CTLTYPE_INT:
        if (strcmp(fmt, "IK") == 0) {
            if (!set_IK(newval, &intval))
                return (luaL_error(L, "invalid value '%s'", (char *)newval));
        }
        else {
            intval = (int)strtol(newval, &endptr, 0);
            if (endptr == newval || *endptr != '\0')
                return (luaL_error(L, "invalid integer '%s'", (char *)newval));
        }
        break;
    }

    return (0);
}


static int
luaA_sysctl_get(lua_State *L)
{
    int nlen, i, oid[CTL_MAXNAME], hexlen;
    size_t len, intlen;
    char fmt[BUFSIZ], buf[BUFSIZ], *val, *oval, *p;
    u_int kind;
    int (*func)(lua_State *, int, void *);
    uintmax_t umv;
    intmax_t mv;

    bzero(fmt, BUFSIZ);
    bzero(buf, BUFSIZ);

    len = strlcpy(buf, luaL_checkstring(L, 1), sizeof(buf)); /* get first argument from lua */
    if (len > sizeof(buf))
        return (luaL_error(L, "first arg too long"));

    nlen = name2oid(buf, oid);
    if (nlen < 0)
        return (luaL_error(L, "unknown iod '%s'", buf));

    if (oidfmt(oid, nlen, fmt, &kind))
        return (luaL_error(L, "couldn't find format of oid '%s'", buf));

    if ((kind & CTLTYPE) == CTLTYPE_NODE)
        return (luaL_error(L, "can't handle CTLTYPE_NODE atm")); // FIXME

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

    p = val;
    switch (*fmt) {
    case 'A':
        lua_pushstring(L, p);
        break;
    case 'I':
    case 'L':
    case 'Q':
        switch (*fmt) {
        case 'I': intlen = sizeof(int); break;
        case 'L': intlen = sizeof(long); break;
        case 'Q': intlen = sizeof(quad_t); break;
        }
        hexlen = 2 + (intlen * CHAR_BIT + 3) / 4;
        i = 0;
        lua_newtable(L);
        while (len >= intlen) {
            i++;
            switch (*fmt) {
            case 'I':
                umv = *(u_int *)p;
                mv = *(int *)p;
                break;
            case 'L':
                umv = *(u_long *)p;
                mv = *(long *)p;
                break;
            case 'Q':
                umv = *(u_quad_t *)p;
                mv = *(quad_t *)p;
                break;
            }

            lua_pushinteger(L, i);
            switch (*fmt) {
            case 'I':
            case 'L':
                lua_pushinteger(L, fmt[1] == 'U' ? umv : mv);
                break;
            case 'Q':
                lua_pushnumber(L, fmt[1] == 'U' ? umv : mv);
                break;
            }
            lua_settable(L, -3);

            len -= intlen;
            p += intlen;
        }
        if (i == 1) {
            lua_pushinteger(L, i);
            lua_gettable(L, -2);
            lua_remove(L, lua_gettop(L) - 1); /* remove table */
        }
        break;
    case 'T':
    case 'S':
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

        if (func) {
            (*func)(L, len, val);
            break;
        }
        /* FALLTHROUGH */
    default:
        free(oval);
        return (luaL_error(L, "unknown CTLTYPE: fmt=%s, kind=%d", fmt, (kind & CTLTYPE))); // FIXME
    }

    free(oval);
    lua_pushstring(L, fmt);
    return (2); /* two returned value */
}


/*
 * Lua initialisation stuff
 */


static const luaL_reg lua_sysctl[] =
{
    {"get",        luaA_sysctl_get},
    {"set",        luaA_sysctl_set},
    {NULL,          NULL}
};

/*
 * Open our library
 */
LUALIB_API int
luaopen_sysctl_core(lua_State *L)
{
    luaL_openlib(L, "sysctl", lua_sysctl, 0);
    return (1);
}


/*
 * Copyright (c) 1993
 *    The Regents of the University of California.  All rights reserved.
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
set_IK(char *str, int *val)
{
    float temp;
    int len, kelv;
    char *p, *endptr, unit;

    if ((len = strlen(str)) == 0)
        return (0);
    p = &str[len - 1];
    if (*p == 'C' || *p == 'F') {
        unit = *p;
        *p = '\0';
        temp = strtof(str, &endptr);
        if (endptr == str || *endptr != '\0')
            return (0);
        if (unit == 'F')
            temp = (temp - 32) * 5 / 9;
        kelv = temp * 10 + 2732;
    } else {
        kelv = (int)strtol(str, &endptr, 10);
        if (endptr == str || *endptr != '\0')
            return (0);
    }
    *val = kelv;
    return (1);
}

/*
 * These functions uses a presently undocumented interface to the kernel
 * to walk the tree and get the type so it can print the value.
 * This interface is under work and consideration, and should probably
 * be killed with a big axe by the first person who can find the time.
 * (be aware though, that the proper interface isn't as obvious as it
 * may seem, there are various conflicting requirements.
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
