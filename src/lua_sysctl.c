/*   _                                      _   _
 *  | |_   _  __ _       ___ _   _ ___  ___| |_| |
 *  | | | | |/ _` |_____/ __| | | / __|/ __| __| |
 *  | | |_| | (_| |_____\__ \ |_| \__ \ (__| |_| |
 *  |_|\__,_|\__,_|     |___/\__, |___/\___|\__|_|
 *                           |___/
 *
 * lua-sysctl is a sysctl(3) interface for the lua scripting language.
 *
 * This library is basically a modified version of FreeBSD's sysctl(8)
 *      src/sbin/sysctl/sysctl.c
 *
 * Copyright (c) 1993
 *    The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2008-2016
 *    Alexandre Perrin <alex@kaworu.ch>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/vmmeter.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getpagesize(3) */
#include <inttypes.h>

/* Include the Lua API header files */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


static int	name2oid(char *name, int *oidp);
static int	oidfmt(int *oid, int len, char *fmt, size_t fmtsiz, u_int *kind);
static int	set_IK(char *str, int *val);

static int ctl_size[CTLTYPE + 1] = {
	[CTLTYPE_INT] = sizeof(int),
	[CTLTYPE_UINT] = sizeof(u_int),
	[CTLTYPE_LONG] = sizeof(long),
	[CTLTYPE_ULONG] = sizeof(u_long),
#if __FreeBSD_version < 900000
	[CTLTYPE_QUAD] = sizeof(quad_t),
#else
	[CTLTYPE_S64] = sizeof(int64_t),
	/* XXX: shouldn't this be sizeof(uint64_t) for CTLTYPE_U64 ? */
	[CTLTYPE_U64] = sizeof(int64_t),
#endif
};


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
		lua_pushnumber(L, (double)la->ldavg[i] / (double)la->fscale);
		lua_settable(L, -3);
	}

	return (1);
}


static int
S_timeval(lua_State *L, int l2, void *p)
{
	struct timeval *tv = (struct timeval *)p;

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
set_T_dev_t(lua_State *L, char *path, void **val, size_t *size)
{
	struct stat statb;
	int rc;

	if (strcmp(path, "none") != 0 && strcmp(path, "off") != 0) {
		rc = stat(path, &statb);
		if (rc)
			return (luaL_error(L, "cannot stat %s", path));

		if (!S_ISCHR(statb.st_mode))
			return (luaL_error(L, "must specify a device special file."));
	} else
		statb.st_rdev = NODEV;

	*val  = &statb.st_rdev;
	*size = sizeof(statb.st_rdev);
	return (1);
}


static int
luaA_sysctl_set(lua_State *L)
{
	int	i;
	int	len;
	int	intval;
	int	mib[CTL_MAXNAME];
	unsigned int uintval;
	u_int	kind;
	long	longval;
	unsigned long ulongval;
	size_t	s;
	size_t newsize = 0;
#if __FreeBSD_version < 900000
	quad_t	quadval;
#else
	int64_t i64val;
	uint64_t u64val;
	intmax_t intmaxval;
	uintmax_t uintmaxval;
#endif
	char	fmt[BUFSIZ];
	char	oid[BUFSIZ];
	char	nvalbuf[BUFSIZ];
	char	*endptr;
	void	*newval = NULL;

	/* get first argument from lua */
	s = strlcpy(oid, luaL_checkstring(L, 1), sizeof(oid));
	if (s >= sizeof(oid))
		return (luaL_error(L, "oid too long: '%s'", oid));
	/* get second argument from lua */
	s = strlcpy(nvalbuf, luaL_checkstring(L, 2), sizeof(nvalbuf));
	if (s >= sizeof(nvalbuf))
		return (luaL_error(L, "new value too long"));
	newval = nvalbuf;
	newsize = s;

	len = name2oid(oid, mib);
	if (len < 0)
		return (luaL_error(L, "unknown iod '%s'", oid));
	if (oidfmt(mib, len, fmt, sizeof(fmt), &kind) != 0)
		return (luaL_error(L, "couldn't find format of oid '%s'", oid));
	if ((kind & CTLTYPE) == CTLTYPE_NODE)
		return (luaL_error(L, "oid '%s' isn't a leaf node", oid));
	if (!(kind & CTLFLAG_WR)) {
		if (kind & CTLFLAG_TUN)
			return (luaL_error(L, "oid '%s' is a read only tunable. "
					"Tunable values are set in /boot/loader.conf", oid));
		else
			return (luaL_error(L, "oid '%s' is read only", oid));
	}
	if ((kind & CTLTYPE) == CTLTYPE_INT	||
	    (kind & CTLTYPE) == CTLTYPE_UINT	||
	    (kind & CTLTYPE) == CTLTYPE_LONG	||
	    (kind & CTLTYPE) == CTLTYPE_ULONG	||
#if __FreeBSD_version < 900000
	    (kind & CTLTYPE) == CTLTYPE_QUAD
#else
	    (kind & CTLTYPE) == CTLTYPE_S64 || (kind & CTLTYPE) == CTLTYPE_U64
#endif
	) {
		if (strlen(newval) == 0)
			return (luaL_error(L, "empty numeric value"));
	}

	switch (kind & CTLTYPE) {
	case CTLTYPE_INT:
		if (strcmp(fmt, "IK") == 0) {
			if (!set_IK(newval, &intval))
				return (luaL_error(L, "invalid value '%s'", (char *)newval));
		} else {
			longval = strtol(newval, &endptr, 0);
			if (endptr == newval || *endptr != '\0' ||
					longval > INT_MAX || longval < INT_MIN) {
				return (luaL_error(L, "invalid integer: '%s'", (char *)newval));
			}
			intval = (int)longval;
		}
		newval = &intval;
		newsize = sizeof(intval);
		break;
	case CTLTYPE_UINT:
		ulongval = strtoul(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0' || ulongval > UINT_MAX) {
			return (luaL_error(L, "invalid unsigned integer: '%s'", (char *)newval));
		}
		uintval = (unsigned int)ulongval;
		newval = &uintval;
		newsize = sizeof(uintval);
		break;
	case CTLTYPE_LONG:
		longval = strtol(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0') {
			return (luaL_error(L, "invalid long integer: '%s'", (char *)newval));
		}
		newval = &longval;
		newsize = sizeof(longval);
		break;
	case CTLTYPE_ULONG:
		ulongval = strtoul(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0') {
			return (luaL_error(L, "invalid unsigned long integer: '%s'",
					(char *)newval));
		}
		newval = &ulongval;
		newsize = sizeof(ulongval);
		break;
	case CTLTYPE_STRING:
		break;
#if __FreeBSD_version < 900000
	case CTLTYPE_QUAD:
		quadval = strtoq(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0') {
			return (luaL_error(L, "invalid quad_t: '%s'", (char *)newval));
		}
		newval = &quadval;
		newsize = sizeof(quadval);
		break;
#else
	case CTLTYPE_S64:
		intmaxval = strtoimax(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0' ||
				intmaxval > INT64_MAX || intmaxval < INT64_MIN) {
			return (luaL_error(L, "invalid int64_t integer: '%s'", (char *)newval));
		}
		i64val = (int64_t)intmaxval;
		newval = &i64val;
		newsize = sizeof(i64val);
		break;
	case CTLTYPE_U64:
		uintmaxval = strtoumax(newval, &endptr, 0);
		if (endptr == newval || *endptr != '\0' || uintmaxval > UINT64_MAX) {
			return (luaL_error(L, "invalid int64_t integer: '%s'", (char *)newval));
		}
		u64val = (uint64_t)uintmaxval;
		newval = &u64val;
		newsize = sizeof(u64val);
		break;
#endif
	case CTLTYPE_OPAQUE:
		if (strcmp(fmt, "T,dev_t") == 0) {
			set_T_dev_t(L, newval, &newval, &newsize);
			break;
		}
		/* FALLTHROUGH */
	default:
		return (luaL_error(L, "oid '%s' is type %d, cannot set that",
		    oid, kind & CTLTYPE));
	}

	if (sysctl(mib, len, NULL, NULL, newval, newsize) == -1) {
		switch (errno) {
		case EOPNOTSUPP:
			return (luaL_error(L, "%s: value is not available", oid));
		case ENOTDIR:
			return (luaL_error(L, "%s: specification is incomplete", oid));
		case ENOMEM:
			/* really? with ENOMEM !?! */
			return (luaL_error(L, "%s: type is unknown to this program", oid));
		default:
			i = strerror_r(errno, nvalbuf, sizeof(nvalbuf));
			if (i != 0)
				return (luaL_error(L, "strerror_r failed"));
			else
				return (luaL_error(L, "%s: %s", oid, nvalbuf));
		}
		/* NOTREACHED */
	}
	return (0);
}


static int
luaA_sysctl_get(lua_State *L)
{
	int	nlen;
	int	i;
	int	oid[CTL_MAXNAME];
	int	ctltype;
	u_int	kind;
	size_t	len;
	size_t	intlen;
	uintmax_t umv;
	intmax_t mv;
	char	fmt[BUFSIZ];
	char	buf[BUFSIZ];
	char	*val, *oval, *p;
	int (*func)(lua_State *, int, void *);

	bzero(fmt, BUFSIZ);
	bzero(buf, BUFSIZ);

	/* get first argument from lua */
	len = strlcpy(buf, luaL_checkstring(L, 1), sizeof(buf));
	if (len >= sizeof(buf))
		return (luaL_error(L, "oid too long"));

	nlen = name2oid(buf, oid);
	if (nlen < 0)
		return (luaL_error(L, "%s: unknown iod", buf));
	if (oidfmt(oid, nlen, fmt, sizeof(fmt), &kind) != 0)
		return (luaL_error(L, "couldn't find format of oid '%s'", buf));
	if ((kind & CTLTYPE) == CTLTYPE_NODE)
		return (luaL_error(L, "can't handle CTLTYPE_NODE"));

	/* find an estimate of how much we need for this var */
	len = 0;
	(void)sysctl(oid, nlen, NULL, &len, NULL, 0);
	len += len; /* we want to be sure :-) */
	val = oval = malloc(len + 1);
	if (val == NULL)
		return (luaL_error(L, "malloc failed"));

	i = sysctl(oid, nlen, val, &len, NULL, 0);
	if (i || !len) {
		free(oval);
		return (luaL_error(L, "sysctl failed"));
	}
	val[len] = '\0';

	p = val;
	ctltype = (kind & CTLTYPE);
	intlen = ctl_size[ctltype];

	switch (ctltype) {
	case CTLTYPE_STRING:
		lua_pushstring(L, p);
		break;
	case CTLTYPE_INT:    /* FALLTHROUGH */
	case CTLTYPE_UINT:   /* FALLTHROUGH */
	case CTLTYPE_LONG:   /* FALLTHROUGH */
	case CTLTYPE_ULONG:  /* FALLTHROUGH */
#if __FreeBSD_version < 900000
	case CTLTYPE_QUAD:
#else
	case CTLTYPE_S64:    /* FALLTHROUGH */
	case CTLTYPE_U64:
#endif
		/* an intlen of 0 or less will make us loop indefinitely */
		if (intlen <= 0) {
			free(oval);
			return (luaL_error(L, "sysctl error (intlen == %zd)", intlen));
		}
		i = 0;
		lua_newtable(L);
		while (len >= intlen) {
			i++;
			switch (ctltype) {
			case CTLTYPE_INT:  /* FALLTHROUGH */
			case CTLTYPE_UINT:
				umv = *(u_int *)p;
				mv = *(int *)p;
				break;
			case CTLTYPE_LONG:   /* FALLTHROUGH */
			case CTLTYPE_ULONG:
				umv = *(u_long *)p;
				mv = *(long *)p;
				break;
#if __FreeBSD_version < 900000
			case CTLTYPE_QUAD:
				umv = *(u_quad_t *)p;
				mv = *(quad_t *)p;
#else
			case CTLTYPE_S64: /* FALLTHROUGH */
			case CTLTYPE_U64:
				umv = *(uint64_t *)p;
				mv = *(int64_t *)p;
#endif
				break;
			default:
				return (luaL_error(L, "sysctl error (bug)"));
			  	/* NOTREACHED */
			}
			lua_pushinteger(L, i);
			switch (ctltype) {
			case CTLTYPE_INT:  /* FALLTHROUGH */
			case CTLTYPE_LONG:
				lua_pushinteger(L, mv);
				break;
			case CTLTYPE_UINT: /* FALLTHROUGH */
			case CTLTYPE_ULONG:
				lua_pushinteger(L, umv);
				break;
#if __FreeBSD_version < 900000
			case CTLTYPE_QUAD:
				lua_pushnumber(L, mv);
				break;
#else
			case CTLTYPE_S64:
				lua_pushnumber(L, mv);
				break;
			case CTLTYPE_U64:
				lua_pushnumber(L, umv);
				break;
#endif
			default:
				return (luaL_error(L, "sysctl error (bug)"));
			  	/* NOTREACHED */
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

		if (func) {
			(*func)(L, len, val);
			break;
		}
		/* FALLTHROUGH */
	default:
		free(oval);
		return (luaL_error(L, "unknown CTLTYPE: fmt=%s ctltype=%d",
		    fmt, ctltype));
	}
	free(oval);
	lua_pushstring(L, fmt);
	return (2); /* two returned value */
}


static int
luaA_sysctl_IK2celsius(lua_State *L)
{

	lua_pushnumber(L, (luaL_checkinteger(L, 1) - 2732.0) / 10);
	return (1); /* one returned value */
}


static int
luaA_sysctl_IK2farenheit(lua_State *L)
{

	lua_pushnumber(L, (luaL_checkinteger(L, 1) / 10.0) * 1.8 - 459.67);
	return (1); /* one returned value */
}


/*
 * Lua initialisation stuff
 */


static const luaL_reg lua_sysctl[] =
{
	{"get",			luaA_sysctl_get},
	{"set",			luaA_sysctl_set},
	{"IK2celsius",		luaA_sysctl_IK2celsius},
	{"IK2farenheit",	luaA_sysctl_IK2farenheit},
	{NULL,			NULL}
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


static int
set_IK(char *str, int *val)
{
	float temp;
	int len, kelv;
	const char *p;
	char *endptr;

	if ((len = strlen(str)) == 0)
		return (0);
	p = &str[len - 1];
	if (*p == 'C' || *p == 'F') {
		temp = strtof(str, &endptr);
		if (endptr == str || endptr != p)
			return (0);
		if (*p == 'F')
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
oidfmt(int *oid, int len, char *fmt, size_t fmtsiz, u_int *kind)
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

	if (fmt) {
		if (strlcpy(fmt, (char *)(buf + sizeof(u_int)), fmtsiz) >= fmtsiz)
			return (1);
	}
	return (0);
}
