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
 * Copyright (c) 2008-2018
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


/* NOTE: our signature of oidfmt differ from sysctl.c because we check for the
   buffer's size */
static int	oidfmt(int *, int, char *, size_t, u_int *);
static int	name2oid(const char *, int *);

static int	strIKtoi(const char *, char **, const char *);

static int ctl_sign[CTLTYPE+1] = {
	[CTLTYPE_INT] = 1,
	[CTLTYPE_LONG] = 1,
	[CTLTYPE_S8] = 1,
	[CTLTYPE_S16] = 1,
	[CTLTYPE_S32] = 1,
	[CTLTYPE_S64] = 1,
};

static int ctl_size[CTLTYPE+1] = {
	[CTLTYPE_INT] = sizeof(int),
	[CTLTYPE_UINT] = sizeof(u_int),
	[CTLTYPE_LONG] = sizeof(long),
	[CTLTYPE_ULONG] = sizeof(u_long),
	[CTLTYPE_S8] = sizeof(int8_t),
	[CTLTYPE_S16] = sizeof(int16_t),
	[CTLTYPE_S32] = sizeof(int32_t),
	[CTLTYPE_S64] = sizeof(int64_t),
	[CTLTYPE_U8] = sizeof(uint8_t),
	[CTLTYPE_U16] = sizeof(uint16_t),
	[CTLTYPE_U32] = sizeof(uint32_t),
	[CTLTYPE_U64] = sizeof(uint64_t),
};

static const char *ctl_typename[CTLTYPE+1] = {
	[CTLTYPE_INT] = "integer",
	[CTLTYPE_UINT] = "unsigned integer",
	[CTLTYPE_LONG] = "long integer",
	[CTLTYPE_ULONG] = "unsigned long",
	[CTLTYPE_U8] = "uint8_t",
	[CTLTYPE_U16] = "uint16_t",
	[CTLTYPE_U32] = "uint16_t",
	[CTLTYPE_U64] = "uint64_t",
	[CTLTYPE_S8] = "int8_t",
	[CTLTYPE_S16] = "int16_t",
	[CTLTYPE_S32] = "int32_t",
	[CTLTYPE_S64] = "int64_t",
	[CTLTYPE_NODE] = "node",
	[CTLTYPE_STRING] = "string",
	[CTLTYPE_OPAQUE] = "opaque",
};

static int
S_clockinfo(lua_State *L, size_t l2, void *p)
{
	struct clockinfo *ci = (struct clockinfo *)p;

	if (l2 != sizeof(*ci))
		return (luaL_error(L, "S_clockinfo %zu != %zu", l2, sizeof(*ci)));

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
S_loadavg(lua_State *L, size_t l2, void *p)
{
	struct loadavg *la = (struct loadavg *)p;
	int i;

	if (l2 != sizeof(*la))
		return (luaL_error(L, "S_loadavg %zu != %zu", l2, sizeof(*la)));

	lua_newtable(L);
	for (i = 0; i < 3; i++) {
		lua_pushinteger(L, i + 1);
		lua_pushnumber(L, (double)la->ldavg[i] / (double)la->fscale);
		lua_settable(L, -3);
	}

	return (1);
}


static int
S_timeval(lua_State *L, size_t l2, void *p)
{
	struct timeval *tv = (struct timeval *)p;

	if (l2 != sizeof(*tv))
		return (luaL_error(L, "S_timeval %zu != %zu", l2, sizeof(*tv)));

	lua_newtable(L);
	lua_pushinteger(L, tv->tv_sec);
	lua_setfield(L, -2, "sec");
	lua_pushinteger(L, tv->tv_usec);
	lua_setfield(L, -2, "usec");

	return (1);
}


static int
S_vmtotal(lua_State *L, size_t l2, void *p)
{
	struct vmtotal *v = (struct vmtotal *)p;
	int pageKilo = getpagesize() / 1024;

	if (l2 != sizeof(*v))
		return (luaL_error(L, "S_vmtotal %zu != %zu", l2, sizeof(*v)));

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
luaA_sysctl_set(lua_State *L)
{
	int	i;
	int	len;
	int	mib[CTL_MAXNAME];
	int8_t i8val;
	uint8_t u8val;
	int16_t i16val;
	uint16_t u16val;
	int32_t i32val;
	uint32_t u32val;
	int intval;
	unsigned int uintval;
	long longval;
	unsigned long ulongval;
	int64_t i64val;
	uint64_t u64val;
	u_int	kind;
	size_t	s;
	size_t newsize = 0;
	char	fmt[BUFSIZ];
	char	bufp[BUFSIZ];
	char	strerrorbuf[BUFSIZ];
	char	nvalbuf[BUFSIZ];
	char	*endptr;
	const void	*newval = NULL;
	const char *newvalstr = NULL;

	/* get first argument from lua */
	s = strlcpy(bufp, luaL_checkstring(L, 1), sizeof(bufp));
	if (s >= sizeof(bufp))
		return (luaL_error(L, "oid too long: '%s'", bufp));
	/* get second argument from lua */
	s = strlcpy(nvalbuf, luaL_checkstring(L, 2), sizeof(nvalbuf));
	if (s >= sizeof(nvalbuf))
		return (luaL_error(L, "new value too long"));
	newvalstr = nvalbuf;
	newsize = s;

	len = name2oid(bufp, mib);
	if (len < 0)
		return (luaL_error(L, "unknown iod '%s'", bufp));
	if (oidfmt(mib, len, fmt, sizeof(fmt), &kind) != 0)
		return (luaL_error(L, "couldn't find format of oid '%s'", bufp));
	if ((kind & CTLTYPE) == CTLTYPE_NODE)
		return (luaL_error(L, "oid '%s' isn't a leaf node", bufp));
	if (!(kind & CTLFLAG_WR)) {
		if (kind & CTLFLAG_TUN)
			return (luaL_error(L, "oid '%s' is a read only tunable. "
					"Tunable values are set in /boot/loader.conf", bufp));
		else
			return (luaL_error(L, "oid '%s' is read only", bufp));
	}

	switch (kind & CTLTYPE) {
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
	case CTLTYPE_S8:
	case CTLTYPE_S16:
	case CTLTYPE_S32:
	case CTLTYPE_S64:
	case CTLTYPE_U8:
	case CTLTYPE_U16:
	case CTLTYPE_U32:
	case CTLTYPE_U64:
		if (strlen(newvalstr) == 0)
			return (luaL_error(L, "empty numeric value"));
		/* FALLTHROUGH */
	case CTLTYPE_STRING:
		break;
	default:
		return (luaL_error(L, "oid '%s' is type %d, cannot set that",
		    bufp, kind & CTLTYPE));
	}

	errno = 0;

	switch (kind & CTLTYPE) {
		case CTLTYPE_INT:
			if (strncmp(fmt, "IK", 2) == 0)
				intval = strIKtoi(newvalstr, &endptr, fmt);
			else
				intval = (int)strtol(newvalstr, &endptr,
				    0);
			newval = &intval;
			newsize = sizeof(intval);
			break;
		case CTLTYPE_UINT:
			uintval = (int) strtoul(newvalstr, &endptr, 0);
			newval = &uintval;
			newsize = sizeof(uintval);
			break;
		case CTLTYPE_LONG:
			longval = strtol(newvalstr, &endptr, 0);
			newval = &longval;
			newsize = sizeof(longval);
			break;
		case CTLTYPE_ULONG:
			ulongval = strtoul(newvalstr, &endptr, 0);
			newval = &ulongval;
			newsize = sizeof(ulongval);
			break;
		case CTLTYPE_STRING:
			newval = newvalstr;
			break;
		case CTLTYPE_S8:
			i8val = (int8_t)strtol(newvalstr, &endptr, 0);
			newval = &i8val;
			newsize = sizeof(i8val);
			break;
		case CTLTYPE_S16:
			i16val = (int16_t)strtol(newvalstr, &endptr,
			    0);
			newval = &i16val;
			newsize = sizeof(i16val);
			break;
		case CTLTYPE_S32:
			i32val = (int32_t)strtol(newvalstr, &endptr,
			    0);
			newval = &i32val;
			newsize = sizeof(i32val);
			break;
		case CTLTYPE_S64:
			i64val = strtoimax(newvalstr, &endptr, 0);
			newval = &i64val;
			newsize = sizeof(i64val);
			break;
		case CTLTYPE_U8:
			u8val = (uint8_t)strtoul(newvalstr, &endptr, 0);
			newval = &u8val;
			newsize = sizeof(u8val);
			break;
		case CTLTYPE_U16:
			u16val = (uint16_t)strtoul(newvalstr, &endptr,
			    0);
			newval = &u16val;
			newsize = sizeof(u16val);
			break;
		case CTLTYPE_U32:
			u32val = (uint32_t)strtoul(newvalstr, &endptr,
			    0);
			newval = &u32val;
			newsize = sizeof(u32val);
			break;
		case CTLTYPE_U64:
			u64val = strtoumax(newvalstr, &endptr, 0);
			newval = &u64val;
			newsize = sizeof(u64val);
			break;
		default:
			/* NOTREACHED */
			return (luaL_error(L, "unexpected type %d (bug)",
			    kind & CTLTYPE));
	}

	if (errno != 0 || endptr == newvalstr ||
	    (endptr != NULL && *endptr != '\0')) {
		return (luaL_error(L, "invalid %s '%s'",
		    ctl_typename[kind & CTLTYPE], newvalstr));
	}


	if (sysctl(mib, len, NULL, NULL, newval, newsize) == -1) {
		switch (errno) {
		case EOPNOTSUPP:
			return (luaL_error(L, "%s: value is not available", newvalstr));
		case ENOTDIR:
			return (luaL_error(L, "%s: specification is incomplete", newvalstr));
		case ENOMEM:
			return (luaL_error(L, "%s: type is unknown to this program", newvalstr));
		default:
			i = strerror_r(errno, strerrorbuf, sizeof(strerrorbuf));
			if (i != 0)
				return (luaL_error(L, "strerror_r failed"));
			else
				return (luaL_error(L, "%s: %s", newvalstr, strerrorbuf));
		}
		/* NOTREACHED */
	}
	return (0);
}


static int
luaA_sysctl_get(lua_State *L)
{
	int	i, nlen, sign, ctltype;
	int	oid[CTL_MAXNAME];
	u_int	kind;
	size_t	len;
	size_t	intlen;
	uintmax_t umv;
	intmax_t mv;
	char	fmt[BUFSIZ];
	char	buf[BUFSIZ];
	char	*val, *oval, *p;
	int (*func)(lua_State *, size_t, void *);

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
	sign = ctl_sign[ctltype];
	intlen = ctl_size[ctltype];

	switch (ctltype) {
	case CTLTYPE_STRING:
		lua_pushstring(L, p);
		break;
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
	case CTLTYPE_S8:
	case CTLTYPE_S16:
	case CTLTYPE_S32:
	case CTLTYPE_S64:
	case CTLTYPE_U8:
	case CTLTYPE_U16:
	case CTLTYPE_U32:
	case CTLTYPE_U64:
		i = 0;
		lua_newtable(L);
		while (len >= intlen) {
			i++;
			switch (ctltype) {
			case CTLTYPE_INT:
			case CTLTYPE_UINT:
				umv = *(u_int *)p;
				mv = *(int *)p;
				break;
			case CTLTYPE_LONG:
			case CTLTYPE_ULONG:
				umv = *(u_long *)p;
				mv = *(long *)p;
				break;
			case CTLTYPE_S8:
			case CTLTYPE_U8:
				umv = *(uint8_t *)p;
				mv = *(int8_t *)p;
				break;
			case CTLTYPE_S16:
			case CTLTYPE_U16:
				umv = *(uint16_t *)p;
				mv = *(int16_t *)p;
				break;
			case CTLTYPE_S32:
			case CTLTYPE_U32:
				umv = *(uint32_t *)p;
				mv = *(int32_t *)p;
				break;
			case CTLTYPE_S64:
			case CTLTYPE_U64:
				umv = *(uint64_t *)p;
				mv = *(int64_t *)p;
				break;
			}
			lua_pushinteger(L, i);
			if (sign) {
				if (intlen > sizeof(lua_Integer))
					lua_pushnumber(L, mv);
				else
					lua_pushinteger(L, mv);
			} else {
				if (intlen > sizeof(lua_Integer))
					lua_pushnumber(L, umv);
				else
					lua_pushinteger(L, (lua_Integer)(umv));
			}
			lua_settable(L, -3);
			len -= intlen;
			p += intlen;
		}
		if (i == 1) {
			/* only one number, replace the table by the numeric
			   value directly */
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


static const luaL_Reg lua_sysctl[] =
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
luaopen_sysctl(lua_State *L)
{

	luaL_newlib(L, lua_sysctl);
	return (1);
}


static int
strIKtoi(const char *str, char **endptrp, const char *fmt)
{
	int kelv;
	float temp;
	size_t len;
	const char *p;
	int prec, i;

	len = strlen(str);

	/*
	 * A format of "IK" is in deciKelvin. A format of "IK3" is in
	 * milliKelvin. The single digit following IK is log10 of the
	 * multiplying factor to convert Kelvin into the untis of this sysctl,
	 * or the dividing factor to convert the sysctl value to Kelvin. Numbers
	 * larger than 6 will run into precision issues with 32-bit integers.
	 * Characters that aren't ASCII digits after the 'K' are ignored. No
	 * localization is present because this is an interface from the kernel
	 * to this program (eg not an end-user interface), so isdigit() isn't
	 * used here.
	 */
	if (fmt[2] != '\0' && fmt[2] >= '0' && fmt[2] <= '9')
		prec = fmt[2] - '0';
	else
		prec = 1;
	p = &str[len - 1];
	if (*p == 'C' || *p == 'F' || *p == 'K') {
		temp = strtof(str, endptrp);
		if (*endptrp != str && *endptrp == p && errno == 0) {
			if (*p == 'F')
				temp = (temp - 32) * 5 / 9;
			*endptrp = NULL;
			if (*p != 'K')
				temp += 273.15;
			for (i = 0; i < prec; i++)
				temp *= 10.0;
			return ((int)(temp + 0.5));
		}
	} else {
		/* No unit specified -> treat it as a raw number */
		kelv = (int)strtol(str, endptrp, 10);
		if (*endptrp != str && *endptrp == p && errno == 0) {
			*endptrp = NULL;
			return (kelv);
		}
	}

	errno = ERANGE;
	return (0);
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
name2oid(const char *name, int *oidp)
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
