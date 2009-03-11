#!/usr/bin/env lua
sysctl = package.loadlib('./lua_sysctl.so', 'luaopen_sysctl')()
g = sysctl.get
s = sysctl.set
